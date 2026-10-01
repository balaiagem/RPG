"""Can you actually walk into the forts and the dungeons?

Asked of the real generator's output and the real UCX collision boxes, on a
20 cm grid, with the navmesh's own agent radius. This exists because "nao
consigo entrar" has now been reported four times and every answer so far came
from me reading the placement code and reasoning about it, which has been
wrong twice. A flood fill does not reason.
"""
import json
import math
import sys

import numpy as np

KIT = json.load(open("/home/claude/kit/out/kit.json"))
HULLS = {}          # name -> [(cx, cy, wide, deep, yaw_deg)] in centimetres
for piece in KIT["pieces"]:
    boxes = []
    for hull in piece["collision"]:
        cx, cy, _cz = hull["centre"]
        w, d, _t = hull["size"]
        boxes.append((cx * 100.0, cy * 100.0, w * 100.0, d * 100.0,
                      hull.get("yaw_deg", 0.0)))
    HULLS[piece["name"]] = boxes

KIND = ["Acampamento", "Circulo", "Ruina", "Cemiterio", "Posto",
        "Forte", "Lenhadores", "Pedreira", "Masmorra"]

CELL = 20.0          # centimetres per raster cell
AGENT = 42.0         # the navmesh agent radius, from DefaultEngine.ini


def load(path):
    spawn, marks, camps, pieces = None, [], [], []
    for line in open(path):
        bit = line.split()
        if bit[0] == "SPAWN":
            spawn = (float(bit[1]), float(bit[2]))
        elif bit[0] == "MARCO":
            marks.append((int(bit[1]), float(bit[2]), float(bit[3]), int(bit[4])))
        elif bit[0] == "CAMPO":
            camps.append((float(bit[1]), float(bit[2]), int(bit[3]), bit[4] == "1"))
        elif bit[0] == "PECA":
            pieces.append((bit[1], float(bit[2]), float(bit[3]), float(bit[4]),
                           float(bit[5]), float(bit[6]), float(bit[7]),
                           float(bit[8]), bit[9]))
    return spawn, marks, camps, pieces


def blocked_grid(pieces, cx, cy, half):
    """True where a walking character cannot stand."""
    side = int(2 * half / CELL)
    grid = np.zeros((side, side), dtype=bool)
    ax = np.arange(side) * CELL + cx - half + CELL * 0.5
    ay = np.arange(side) * CELL + cy - half + CELL * 0.5
    gx, gy = np.meshgrid(ax, ay, indexing="ij")
    for name, px, py, _pz, yaw, sx, sy, _sz, flags in pieces:
        if flags[0] == "1":                     # paving: no collision at all
            continue
        if abs(px - cx) > half + 900 or abs(py - cy) > half + 900:
            continue
        boxes = HULLS.get(name)
        if not boxes:                           # not a kit piece: skip
            continue
        rad = math.radians(yaw)
        cos, sin = math.cos(rad), math.sin(rad)
        for hx, hy, hw, hd, hyaw in boxes:
            # the hull's centre in world space, with the actor's scale and yaw
            ox, oy = hx * sx, hy * sy
            wx = px + ox * cos - oy * sin
            wy = py + ox * sin + oy * cos
            # half extents, scaled, plus the agent radius
            ex = hw * sx * 0.5 + AGENT
            ey = hd * sy * 0.5 + AGENT
            tot = math.radians(yaw + hyaw)
            c2, s2 = math.cos(tot), math.sin(tot)
            # the grid point in the hull's own frame
            dx, dy = gx - wx, gy - wy
            lx = dx * c2 + dy * s2
            ly = -dx * s2 + dy * c2
            grid |= (np.abs(lx) <= ex) & (np.abs(ly) <= ey)
    return grid


def flood_from_border(free):
    """Everything reachable from outside the box, four-connected."""
    side = free.shape[0]
    seen = np.zeros_like(free)
    stack = []
    for i in range(side):
        for j in (0, side - 1):
            if free[i, j]:
                stack.append((i, j)); seen[i, j] = True
            if free[j, i]:
                stack.append((j, i)); seen[j, i] = True
    while stack:
        i, j = stack.pop()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = i + di, j + dj
            if 0 <= a < side and 0 <= b < side and free[a, b] and not seen[a, b]:
                seen[a, b] = True
                stack.append((a, b))
    return seen


def check(path, want_kinds=(5, 8), draw=None):
    spawn, marks, camps, pieces = load(path)
    report = []
    for index, mx, my, kind in marks:
        if kind not in want_kinds:
            continue
        half = 2600.0 if kind == 8 else 1800.0
        grid = blocked_grid(pieces, mx, my, half)
        free = ~grid
        seen = flood_from_border(free)
        side = grid.shape[0]
        mid = side // 2
        # Is the middle of the place reachable from outside?
        window = seen[mid - 6:mid + 6, mid - 6:mid + 6]
        openw = free[mid - 6:mid + 6, mid - 6:mid + 6]
        inside_free = int(openw.sum())
        inside_seen = int(window.sum())
        # And, for a dungeon, each indoor camp
        rooms, roomsok = 0, 0
        for cxx, cyy, _foes, indoor in camps:
            if not indoor or abs(cxx - mx) > half or abs(cyy - my) > half:
                continue
            rooms += 1
            i = int((cxx - (mx - half)) / CELL)
            j = int((cyy - (my - half)) / CELL)
            if 0 <= i < side and 0 <= j < side and seen[i, j]:
                roomsok += 1
        report.append((index, KIND[kind], inside_free, inside_seen, rooms, roomsok))
        if draw is not None and kind == draw:
            picture(grid, seen, mx, my, half, KIND[kind], index)
    return report


def picture(grid, seen, mx, my, half, label, index):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    img = np.zeros(grid.shape + (3,), dtype=float)
    img[...] = 0.16                       # unreachable open ground: dark
    img[seen] = (0.30, 0.65, 0.32)        # reachable from outside: green
    img[grid] = (0.75, 0.24, 0.20)        # solid: red
    fig, ax = plt.subplots(figsize=(7, 7), dpi=110)
    ax.imshow(np.transpose(img, (1, 0, 2)), origin="lower")
    ax.set_title("%s #%d em (%.0f, %.0f) -- verde = da pra chegar de fora"
                 % (label, index, mx, my), fontsize=10)
    ax.axis("off")
    out = "/home/claude/kit/out/planta_%s_%d.png" % (label.lower(), index)
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print("desenhei", out)


if __name__ == "__main__":
    path = sys.argv[1]
    draw = int(sys.argv[2]) if len(sys.argv) > 2 else None
    for row in check(path, draw=draw):
        index, label, free, seen, rooms, roomsok = row
        verdict = "ABERTO" if seen > free * 0.5 else "*** FECHADO ***"
        print("%-12s #%-2d  miolo livre %4d, alcancavel %4d  %-16s  salas %d/%d"
              % (label, index, free, seen, verdict, roomsok, rooms))
