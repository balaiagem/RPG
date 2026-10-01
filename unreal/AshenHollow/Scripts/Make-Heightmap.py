"""Builds the Ashen Hollow landscape: a 1 km heightmap for Unreal to import.

Why this is a heightmap and not code in the game
------------------------------------------------
The ground used to be two and a half thousand boxes spawned at runtime, and
that is why navigation never worked: a navmesh can only be baked over geometry
that is already in the map when you press Build. A Landscape is in the map.
Bake once, navigation is solved forever and costs nothing per frame.

Why this file was rewritten a third time
----------------------------------------
The first two attempts simulated erosion (thermal, then droplets) and blew up.
The third made a beautiful map with domain warping, measured it at 94% walkable,
and was *still* miserable to walk on. That gap is the whole lesson, and it is
worth stating plainly, because it is the same mistake made three different ways:

    mobility was a REPORT, not a CONSTRAINT.

The generator built ground to look like geology, and then measured whether
Recast would technically accept it. Recast walks up to 44 degrees. Ground at
30 degrees is walkable to Recast, invisible as an obstacle to the player, and
horrible to move on: the camera fights you, the pawn crawls, pathfinding takes
long ways round, and everything that stands on it stands crooked. A whole map
in that band passes every test and plays like mud.

So the rule this file is built on, and the thing every stage below serves:

    either the ground is gentle enough to walk without thinking about it,
    or it is a cliff that reads as a wall. Nothing in between.

Nothing in the miserable 20-to-40-degree band. How that is achieved:

* **Plateaus, not hills.** A low-frequency warped noise field is QUANTISED into
  a handful of terrace levels. Inside a terrace the ground undulates by a couple
  of metres over tens of metres -- visibly not flat, nowhere near steep. All of
  the map's relief lives in the boundaries between terraces.
* **Every boundary is either a cliff or a ramp,** decided by a low-frequency
  mask so that long escarpments alternate with wide saddles rather than
  ringing each terrace completely.
* **A symmetric slope limiter with per-edge caps** does the enforcing. Gentle
  cap everywhere; uncapped across cliff edges. Run to convergence, a ramp
  boundary automatically stretches its step out into fifty metres of gradient,
  and a cliff boundary stays a wall. This is the stage that makes mobility a
  constraint: the limiter cannot terminate while a single edge in the map
  violates its cap.
* **Connectivity is repaired before the limiter runs,** on the cap field
  itself, so it is cheap: components of the gentle-edge graph are found and
  cliff is opened into ramp until the whole map is one component. A terrace
  walled in on all sides is a beautiful prison and the old generator made
  several.

And the measurements at the end were changed too, because the old one lied:
percentage under the GAMEPLAY slope (not Recast's), CONNECTED walkable area
rather than total, and a **detour factor** -- how much longer it is to walk
between two points than to fly. That last number is the one that actually means
"mobilidade", and no version of this file before this one computed it.
"""

import json
import math

import numpy as np
from PIL import Image
from scipy import ndimage
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components, dijkstra

# ── The map ─────────────────────────────────────────────────────────────────
# 1009 is one of Unreal's recommended landscape sizes: 16 x 16 components of 63
# quads, so 1008 quads plus the closing vertex. At 100 uu per quad that is
# 100 800 uu -- one metre of ground per quad, almost exactly a kilometre across.
SIZE    = 1009
QUAD_UU = 100.0
SEED    = 20260926

# Unreal maps the 16-bit range onto -256 m .. +256 m at Z scale 100, so 32768 is
# zero and one metre is 128 counts.
MID       = 32768
PER_METRE = 128.0

# Arrays are [row, col]: row is +Y, col is +X. Said once and held to, because an
# earlier version mixed the two and put the mountains on the wrong side of the
# map and the spawn point in the wrong place.

# ── The two slopes, and nothing between them ────────────────────────────────
# One quad is one metre, so the rise between neighbours IS the tangent of the
# slope. BUILD_DEG is what the limiter enforces; PLAY_DEG is the slightly looser
# line the finished map is measured against, so that diagonal steps (which can
# reach sqrt(2) times the axis cap) still count as comfortable.
BUILD_DEG = 12.0
PLAY_DEG  = 17.0
BUILD_RISE = math.tan(math.radians(BUILD_DEG))   # 0.213 m per metre
PLAY_RISE  = math.tan(math.radians(PLAY_DEG))    # 0.306 m per metre
CLIFF_RISE = 99.0                                # i.e. no cap at all

# ── The terraces ────────────────────────────────────────────────────────────
# Six levels of about nine metres: fifty metres of total relief, which at a
# kilometre across is a gentle land with a lot of edges in it -- and edges are
# what make a place legible. A hundred and sixteen metres of relief, which is
# what the old map had, is a mountain range you have to negotiate.
LEVELS  = 6
STEP_M  = 9.0
# How much of each terrace boundary is allowed to be cliff. The rest becomes
# ramp. Above about 0.6 the map starts to feel like a maze of walls.
CLIFF_SHARE = 0.46

# Where the map is guaranteed level: the arrival apron first, then the shelves
# the settlements stand on. Rows and columns as fractions of the map, then the
# flat radius in fractions of the map.
# The radius is not decoration: the generator builds out to seven cells (70 m)
# from a site, so anything under that is a village standing half on a slope.
# A house is eight metres wide and its pivot is one point in the middle, so on
# sloping ground the uphill half goes into the hill -- which is exactly what
# "casas que aparecem embaixo da terra" was. A town stands on a flat, the way
# towns in real maps do, and the flat has to be bigger than the town.
SITES = [
    ("Chegada",       0.090, 0.420, 0.085),
    ("Vale do Norte", 0.300, 0.200, 0.080),
    ("Passo Alto",    0.270, 0.700, 0.078),
    ("Feira do Meio", 0.560, 0.400, 0.080),
    ("Corte Leste",   0.640, 0.790, 0.076),
    ("Pedreira",      0.790, 0.230, 0.076),
]


# ── Noise ───────────────────────────────────────────────────────────────────
def sample(lat, row, col):
    """Bilinear, smoothstepped read of a lattice at fractional positions.

    Positions are arrays, and that is what makes domain warping possible: the
    coordinates handed in can themselves have been displaced by other noise.
    """
    n = lat.shape[0]
    row = np.clip(row, 0.0, n - 1.001)
    col = np.clip(col, 0.0, n - 1.001)
    r0 = row.astype(np.int32)
    c0 = col.astype(np.int32)
    fr = row - r0
    fc = col - c0
    fr = fr * fr * (3.0 - 2.0 * fr)
    fc = fc * fc * (3.0 - 2.0 * fc)
    r1 = np.minimum(r0 + 1, n - 1)
    c1 = np.minimum(c0 + 1, n - 1)
    return (lat[r0, c0] * (1 - fc) * (1 - fr) + lat[r0, c1] * fc * (1 - fr)
            + lat[r1, c0] * (1 - fc) * fr + lat[r1, c1] * fc * fr)


def fbm(rng, row, col, octaves, cells, gain=0.5):
    """Fractal noise read at the given (possibly warped) unit coordinates."""
    total = np.zeros_like(row)
    amp, weight = 1.0, 0.0
    for _ in range(octaves):
        lat = rng.random((cells + 1, cells + 1))
        total += sample(lat, row * cells, col * cells) * amp
        weight += amp
        amp *= gain
        cells *= 2
    return total / weight


def blur(field, passes=1):
    """Cheap separable box blur, three cells wide, edges held."""
    for _ in range(passes):
        pad = np.pad(field, 1, mode="edge")
        field = (pad[:-2, 1:-1] + pad[2:, 1:-1] + pad[1:-1, :-2] + pad[1:-1, 2:]
                 + 2.0 * field) / 6.0
    return field


# ── The limiter: the one stage that makes mobility a constraint ─────────────
def limit(height, cap_r, cap_c, rounds=3000, relax=0.85):
    """Push the terrain until no axis edge rises faster than its own cap.

    Symmetric: when an edge is too steep both ends move, half the excess each,
    so a ramp grows outward from the boundary instead of one terrace sliding
    down. Explicit slicing rather than np.roll, because np.roll wraps the north
    edge of the map onto the south one and the old generator did exactly that.

    Returns the settled height and how many rounds it took; if it ever comes
    back having used every round, the map is NOT within its caps and the number
    printed at the end is a lie. That is why the count is returned.
    """
    h = height.astype(np.float64).copy()
    for round_index in range(rounds):
        worst = 0.0
        for axis, cap in ((0, cap_r), (1, cap_c)):
            lo = (slice(None, -1), slice(None)) if axis == 0 else (slice(None), slice(None, -1))
            hi = (slice(1, None), slice(None)) if axis == 0 else (slice(None), slice(1, None))
            drop = h[hi] - h[lo]
            excess = np.abs(drop) - cap
            active = excess > 0.0
            if not active.any():
                continue
            worst = max(worst, float(excess[active].max()))
            shove = np.where(active, np.sign(drop) * excess * 0.5 * relax, 0.0)
            h[lo] += shove
            h[hi] -= shove
        if worst <= 1e-3:
            return h, round_index + 1
    return h, rounds


def edge_caps(level, cliff_ok):
    """Per-edge slope caps: free across a cliff boundary, gentle everywhere else.

    A boundary edge is one where the terrace level changes. It becomes a cliff
    only where the cliff mask allows it at BOTH ends -- so the mask's own
    boundaries turn into ramps, which is where escarpments get their saddles.
    """
    cap_r = np.full((level.shape[0] - 1, level.shape[1]), BUILD_RISE)
    cap_c = np.full((level.shape[0], level.shape[1] - 1), BUILD_RISE)
    step_r = level[1:, :] != level[:-1, :]
    step_c = level[:, 1:] != level[:, :-1]
    cap_r[step_r & cliff_ok[1:, :] & cliff_ok[:-1, :]] = CLIFF_RISE
    cap_c[step_c & cliff_ok[:, 1:] & cliff_ok[:, :-1]] = CLIFF_RISE
    return cap_r, cap_c


# ── Connectivity, repaired on the caps rather than on the heights ───────────
def gentle_graph(cap_r, cap_c):
    """Adjacency of the cells a character can walk between, as a sparse graph.

    Built from the CAPS, not from the finished heights, and that is the trick
    that makes the repair loop affordable: after the limiter runs, an edge is
    walkable if and only if its cap was gentle, so connectivity can be settled
    before paying for the limiter even once.
    """
    n = cap_r.shape[1]
    index = np.arange(n * n).reshape(n, n)
    rows, cols = [], []
    for cap, a, b in ((cap_r, index[:-1, :], index[1:, :]),
                      (cap_c, index[:, :-1], index[:, 1:])):
        keep = cap < CLIFF_RISE * 0.5
        rows.append(a[keep])
        cols.append(b[keep])
    rows = np.concatenate(rows)
    cols = np.concatenate(cols)
    data = np.ones(rows.size, dtype=np.int8)
    return coo_matrix((data, (rows, cols)), shape=(n * n, n * n))


def open_ramps(level, cliff_ok, height, seed_rc, radius=26, tries=60):
    """Open cliff into ramp until the whole map is one walkable component.

    Each pass finds the walkable components, then for every island picks the
    single cliff edge with the smallest drop that leads out of it and clears the
    cliff mask in a disc around it. A disc, not an edge: one gentle quad in a
    wall of cliff is a slot the player cannot see and pathfinding will thread
    like a needle. Twenty-six metres is a saddle you can spot from a distance.
    """
    size = level.shape[0]
    rows, cols = np.mgrid[0:size, 0:size]
    opened = 0
    for _ in range(tries):
        cap_r, cap_c = edge_caps(level, cliff_ok)
        count, label = connected_components(gentle_graph(cap_r, cap_c),
                                            directed=False)
        label = label.reshape(size, size)
        main = label[seed_rc]
        island = label != main
        if not island.any():
            return cliff_ok, opened, count, 100.0 * (~island).mean()

        # Every cliff edge that crosses from the mainland to somewhere else, and
        # the gentlest of them is where the stair goes.
        best = None
        for axis in (0, 1):
            lo = (slice(None, -1), slice(None)) if axis == 0 else (slice(None), slice(None, -1))
            hi = (slice(1, None), slice(None)) if axis == 0 else (slice(None), slice(1, None))
            cap = cap_r if axis == 0 else cap_c
            crosses = (cap >= CLIFF_RISE * 0.5) & (label[lo] != label[hi]) \
                & ((label[lo] == main) | (label[hi] == main))
            if not crosses.any():
                continue
            drop = np.abs(height[hi] - height[lo])
            drop = np.where(crosses, drop, np.inf)
            flat = int(np.argmin(drop))
            value = float(drop.flat[flat])
            here = np.unravel_index(flat, drop.shape)
            if best is None or value < best[0]:
                best = (value, here[0], here[1])

        if best is None:                     # nothing touches the mainland
            biggest = np.bincount(label.ravel())
            for other in np.argsort(-biggest):
                if other != main and (label == other).any():
                    seed_rc = tuple(int(v) for v in
                                    np.argwhere(label == other)[0])
                    break
            continue

        _drop, br, bc = best
        near = np.hypot(rows - br, cols - bc) < radius
        cliff_ok = cliff_ok & ~near
        opened += 1

    cap_r, cap_c = edge_caps(level, cliff_ok)
    count, label = connected_components(gentle_graph(cap_r, cap_c), directed=False)
    label = label.reshape(size, size)
    main = label[seed_rc]
    return cliff_ok, opened, count, 100.0 * (label == main).mean()


# ── Somewhere to build ──────────────────────────────────────────────────────
def settle(height, row, col, radius, blend, level=None):
    """Ease a patch of ground level, for somewhere a village can stand.

    Eased over a blend far wider than the patch itself. A narrow blend stamps a
    crater into the hillside; a wide one lets a shelf arrive out of it.
    """
    size = height.shape[0]
    rows, cols = np.mgrid[0:size, 0:size]
    far = np.hypot(cols - col, rows - row)
    target = float(np.median(height[far < radius])) if level is None else level
    weight = np.clip((blend - (far - radius)) / blend, 0.0, 1.0)
    weight = weight * weight * (3.0 - 2.0 * weight)
    return height * (1.0 - weight) + target * weight


# ── Rivers ──────────────────────────────────────────────────────────────────
def drainage(height, rounds=200):
    """How much water passes through each cell, by steepest descent.

    Eight neighbours, not four: routing on four is what combed the first
    version of this map into vertical stripes.
    """
    steps = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1))
    best = np.full(height.shape, -1, dtype=np.int8)
    fall = np.zeros_like(height)
    for index, (dr, dc) in enumerate(steps):
        lower = np.roll(np.roll(height, -dr, axis=0), -dc, axis=1)
        slope = (height - lower) / np.hypot(dr, dc)
        better = slope > fall
        fall = np.where(better, slope, fall)
        best = np.where(better, np.int8(index), best)

    water = np.ones_like(height)
    for _ in range(rounds):
        moved = np.zeros_like(height)
        for index, (dr, dc) in enumerate(steps):
            moved += np.roll(np.roll(np.where(best == index, water, 0.0), dr, axis=0),
                             dc, axis=1)
        water = moved + np.where(best < 0, water, 0.0) + 1.0
    return water


# ── The map itself ──────────────────────────────────────────────────────────
def build(report):
    rng = np.random.default_rng(SEED)
    rows, cols = np.mgrid[0:SIZE, 0:SIZE] / (SIZE - 1.0)

    # Domain warp: the coordinates the terrace field is read at are themselves
    # displaced, so terrace boundaries fold and meander instead of looking like
    # a contour map of added octaves.
    warp_r = fbm(rng, rows, cols, 4, 3) - 0.5
    warp_c = fbm(rng, rows, cols, 4, 3) - 0.5
    wr = rows + warp_r * 0.26
    wc = cols + warp_c * 0.26

    # The land as a whole: low in the south-west where the player arrives,
    # rising to the north and east. A map with no overall direction has nothing
    # to walk towards.
    lay = np.clip(rows * 0.62 + cols * 0.44 - 0.06, 0.0, 1.0)
    field = 0.52 * lay + 0.48 * fbm(rng, wr, wc, 4, 3, gain=0.52)
    field -= field.min()
    field /= field.max()

    # ── Quantise: all of the relief lives in the boundaries ───────────────
    level = np.clip((field * LEVELS).astype(np.int32), 0, LEVELS - 1)
    # A terrace smaller than a courtyard is noise, not a place. Opening and
    # closing the level field removes the speckle along its boundaries.
    for value in range(LEVELS):
        mask = ndimage.binary_closing(level == value, np.ones((9, 9)))
        mask = ndimage.binary_opening(mask, np.ones((9, 9)))
        level = np.where(mask, value, level)
    level = ndimage.median_filter(level, size=11)

    # Terraces at nine metre spacing, jittered so they are not audibly a
    # staircase, plus the undulation that keeps a terrace from reading as a
    # car park: three metres over a hundred, one and a half over forty. Both
    # are a long way under the twelve degree cap, on purpose -- the limiter
    # should have almost nothing to do inside a terrace.
    jitter = (rng.random(LEVELS) - 0.5) * 2.2
    base = level.astype(np.float64) * STEP_M + jitter[level]
    base += (fbm(rng, wr, wc, 3, 8, gain=0.5) - 0.5) * 6.0
    base += (fbm(rng, wr, wc, 3, 24, gain=0.5) - 0.5) * 3.0

    # ── Which boundaries are allowed to be cliff ──────────────────────────
    grain = fbm(rng, rows, cols, 3, 5, gain=0.5)
    cliff_ok = grain > np.quantile(grain, 1.0 - CLIFF_SHARE)
    # No cliff through a settlement, nor through the ground it is approached
    # over. A village on the lip of an escarpment is a village with one road.
    site_rc = []
    keep_rows, keep_cols = np.mgrid[0:SIZE, 0:SIZE]
    for _name, frow, fcol, frad in SITES:
        r, c = int(SIZE * frow), int(SIZE * fcol)
        site_rc.append((r, c))
        cliff_ok &= np.hypot(keep_cols - c, keep_rows - r) > SIZE * frad + 70.0

    cliff_ok, opened, parts, joined = open_ramps(level, cliff_ok, base, site_rc[0])
    report["ramps_abertas"] = opened
    report["componentes_antes_do_limitador"] = int(parts)
    report["ligado_pct_previsto"] = round(joined, 2)

    # ── Flatten the settlement shelves ────────────────────────────────────
    height = settle(base, site_rc[0][0], site_rc[0][1],
                    SIZE * SITES[0][3], SIZE * 0.11,
                    level=float(np.percentile(base, 10)))
    for (r, c), (_n, _fr, _fc, frad) in zip(site_rc[1:], SITES[1:]):
        height = settle(height, r, c, SIZE * frad, SIZE * 0.075)

    # ── Enforce ───────────────────────────────────────────────────────────
    cap_r, cap_c = edge_caps(level, cliff_ok)
    height, used = limit(height, cap_r, cap_c)
    report["rodadas_do_limitador"] = used

    # ── Rivers, cut where the water actually goes, then enforced again ─────
    # Shallow on purpose. A river is scenery here: what stops the player
    # wading is the invisible blocker the game places, not a gorge. A gorge
    # would put two unwalkable banks down the middle of the map.
    water = drainage(height)
    stream = np.clip((np.log1p(water) - 6.3) / 2.2, 0.0, 1.0)
    plain = blur(stream, 16)
    height = height - stream * 1.1 - plain * 2.0
    height, used = limit(height, cap_r, cap_c)
    report["rodadas_do_limitador_pos_rio"] = used

    height -= height.min()
    return height, stream, level, cliff_ok, site_rc


# ── Measuring the thing that actually matters ───────────────────────────────
def walkable_mask(height, rise):
    """Cells whose four axis neighbours are all within the given rise."""
    gentle = np.ones(height.shape, dtype=bool)
    step_r = np.abs(height[1:, :] - height[:-1, :])
    step_c = np.abs(height[:, 1:] - height[:, :-1])
    gentle[:-1, :] &= step_r <= rise
    gentle[1:, :] &= step_r <= rise
    gentle[:, :-1] &= step_c <= rise
    gentle[:, 1:] &= step_c <= rise
    return gentle


def connected_walkable(height, seed_rc, rise):
    """The one patch of gentle ground the player can actually reach, and its size.

    The old generator reported total gentle area, which is the number that let a
    map with an unreachable half score 94%.
    """
    gentle = walkable_mask(height, rise)
    label, _count = ndimage.label(gentle)
    tag = label[seed_rc]
    if tag == 0:
        return np.zeros_like(gentle), 0.0
    reached = label == tag
    return reached, 100.0 * reached.mean()


def detour(reached, samples=900, step=4, rng=None):
    """How much longer it is to walk between two points than to fly.

    This is "mobilidade" as a number. A map of open ground scores near 1.0; a
    map of corridors and walls scores 1.4 and up, and at 1.6 the player says
    the map is horrible without being able to point at any one place.

    Measured on the map coarsened by `step` metres, on an octile graph, with
    Dijkstra from a dozen sources: exact enough to trust, cheap enough to run
    on every build.
    """
    rng = rng or np.random.default_rng(7)
    small = reached[::step, ::step]
    n = small.shape[0]
    index = np.full(small.shape, -1)
    inside = np.argwhere(small)
    if inside.size == 0:
        return None
    index[small] = np.arange(inside.shape[0])

    rows, cols, dist = [], [], []
    for dr, dc in ((0, 1), (1, 0), (1, 1), (1, -1)):
        src_r = slice(max(0, -dr), n - max(0, dr))
        src_c = slice(max(0, -dc), n - max(0, dc))
        dst_r = slice(max(0, dr), n - max(0, -dr))
        dst_c = slice(max(0, dc), n - max(0, -dc))
        a = index[src_r, src_c]
        b = index[dst_r, dst_c]
        keep = (a >= 0) & (b >= 0)
        rows.append(a[keep])
        cols.append(b[keep])
        dist.append(np.full(int(keep.sum()), math.hypot(dr, dc) * step))
    graph = coo_matrix((np.concatenate(dist),
                        (np.concatenate(rows), np.concatenate(cols))),
                       shape=(inside.shape[0], inside.shape[0]))

    picks = rng.choice(inside.shape[0], size=min(12, inside.shape[0]), replace=False)
    walked = dijkstra(graph, directed=False, indices=picks)

    ratios = []
    for row, source in zip(walked, picks):
        sr, sc = inside[source] * step
        flew = np.hypot(inside[:, 0] * step - sr, inside[:, 1] * step - sc)
        far = (flew > 120.0) & np.isfinite(row)
        if not far.any():
            continue
        pick = rng.choice(np.flatnonzero(far),
                          size=min(samples // len(picks), int(far.sum())),
                          replace=False)
        ratios.extend((row[pick] / flew[pick]).tolist())
    if not ratios:
        return None
    ratios = np.array(ratios)
    return {"medio": round(float(ratios.mean()), 3),
            "p95": round(float(np.percentile(ratios, 95)), 3),
            "pior": round(float(ratios.max()), 3)}


def slope_profile(height):
    """Where the map's slopes actually sit, in degrees, by area.

    The band that matters is 20 to 40: walkable to the engine, invisible to the
    player, awful to move on. It should be nearly empty.
    """
    step_r = np.abs(np.diff(height, axis=0))
    step_c = np.abs(np.diff(height, axis=1))
    rise = np.maximum(
        np.maximum(np.pad(step_r, ((0, 1), (0, 0))), np.pad(step_r, ((1, 0), (0, 0)))),
        np.maximum(np.pad(step_c, ((0, 0), (0, 1))), np.pad(step_c, ((0, 0), (1, 0)))))
    deg = np.degrees(np.arctan(rise))
    return {
        "ate_12_graus_pct": round(100.0 * float((deg <= 12.0).mean()), 1),
        "ate_17_graus_pct": round(100.0 * float((deg <= PLAY_DEG).mean()), 1),
        "faixa_ruim_20_a_40_pct": round(100.0 * float(((deg > 20.0) & (deg < 40.0)).mean()), 2),
        "penhasco_acima_de_55_pct": round(100.0 * float((deg >= 55.0).mean()), 2),
    }


# ── Pictures, because every rejected version of this file was rejected by eye ─
def shade(height, stream, reached=None):
    gr, gc = np.gradient(height)
    light = np.clip((-gc * 0.55 - gr * 0.55 + 1.15) / np.sqrt(gr**2 + gc**2 + 1.0), 0.0, 1.0)
    band = np.clip(height / max(height.max(), 1.0), 0.0, 1.0)

    stops = [(0.00, (86, 118, 68)), (0.16, (104, 136, 72)), (0.34, (132, 150, 84)),
             (0.54, (156, 148, 104)), (0.72, (150, 138, 122)), (0.88, (170, 164, 160)),
             (1.00, (232, 236, 240))]
    rgb = np.zeros(height.shape + (3,))
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        mask = (band >= a) & (band <= b)
        t = np.where(mask, (band - a) / (b - a), 0.0)
        for c in range(3):
            rgb[..., c] += mask * (ca[c] + (cb[c] - ca[c]) * t)
    rgb *= (0.46 + 0.74 * light)[..., None]
    wet = (stream > 0.30)[..., None]
    rgb = np.where(wet, np.array([58, 102, 140]) * (0.75 + 0.45 * light)[..., None], rgb)
    if reached is not None:
        rgb = np.where((~reached)[..., None], rgb * 0.50 + 54.0, rgb)
    for _name, frow, fcol, _r in SITES:
        r, c = int(height.shape[0] * frow), int(height.shape[0] * fcol)
        rgb[max(r - 5, 0):r + 5, max(c - 5, 0):c + 5] = (250, 80, 60)
    return np.clip(rgb, 0, 255).astype(np.uint8)


def mobility_picture(reached, height):
    """Green where you can walk, dark where you cannot. No shading, no colour
    ramp, nothing pretty -- this is the picture that answers the complaint."""
    rgb = np.where(reached[..., None],
                   np.array([104, 168, 96]), np.array([46, 42, 44])).astype(float)
    rgb *= (0.78 + 0.44 * np.clip(height / max(height.max(), 1.0), 0.0, 1.0))[..., None]
    for _name, frow, fcol, _r in SITES:
        r, c = int(reached.shape[0] * frow), int(reached.shape[0] * fcol)
        rgb[max(r - 6, 0):r + 6, max(c - 6, 0):c + 6] = (250, 80, 60)
    return np.clip(rgb, 0, 255).astype(np.uint8)


if __name__ == "__main__":
    report = {}
    height, stream, level, cliff_ok, site_rc = build(report)
    reached, percent = connected_walkable(height, site_rc[0], PLAY_RISE)

    counts = np.clip(MID + height * PER_METRE, 0, 65535).astype(np.uint16)
    Image.fromarray(counts).save("heightmap_ashen_1km.png")
    Image.fromarray(shade(height, stream, reached)).save("previa_relevo.png")
    Image.fromarray(mobility_picture(reached, height)).save("previa_mobilidade.png")

    # 257 x 257 floats in metres, [x][y], for the offline harness. Same terrain
    # the engine will get, so the harness and the game agree about the ground.
    coarse = height[::(SIZE - 1) // 256, ::(SIZE - 1) // 256][:257, :257]
    with open("terrain.bin", "wb") as out:
        out.write(np.int32(257).tobytes())
        out.write(np.ascontiguousarray(coarse.T, dtype="<f4").tobytes())

    half = (SIZE - 1) * QUAD_UU * 0.5
    places = {}
    for (name, _fr, _fc, _rad), (r, c) in zip(SITES, site_rc):
        patch = height[max(r - 70, 0):r + 70, max(c - 70, 0):c + 70]
        places[name] = {
            "uu": [round(c * QUAD_UU - half), round(r * QUAD_UU - half)],
            "solo_m": round(float(height[r, c]), 1),
            "desnivel_no_bairro_140m": round(float(np.ptp(patch)), 2),
            "alcancavel": bool(reached[r, c]),
        }

    report.update({
        "metros_de_lado": round((SIZE - 1) * QUAD_UU / 100.0),
        "relevo_total_m": round(float(height.max()), 1),
        "mediana_m": round(float(np.median(height)), 1),
        "andavel_ligado_pct": round(percent, 1),
        "rio_pct": round(100.0 * float((stream > 0.30).mean()), 2),
        "declives": slope_profile(height),
        "fator_de_desvio": detour(reached),
        "sitios": places,
    })
    print(json.dumps(report, indent=2, ensure_ascii=False))
