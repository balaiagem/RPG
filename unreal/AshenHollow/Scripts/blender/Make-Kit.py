"""Builds the Ashen Hollow art kit: every building and prop in the game, by script.

WHY THE KIT IS GENERATED AND NOT BOUGHT
----------------------------------------
Three weeks of the same complaint -- houses in the road, banners in the path,
posts at the mouth of a bridge, things standing inside other things -- were all
one problem wearing different clothes: **the game knew a bounding box and
nothing else.** The village pack's blueprints carry a banner's pole, a tent's
guy ropes and a bridge's newel posts outside the box the generator reasons
about, and nothing anywhere records which face of a house is its front.

No clearance rule can fix that, because the information does not exist. So the
kit is generated, and every piece is born knowing three things the pack could
never tell us:

* **its pivot is on the ground, at the centre of its footprint** -- so "put this
  here" means the same thing for every asset in the game;
* **its footprint is declared**, in metres, in `kit.json`, next to the mesh --
  not measured at runtime from whatever geometry happens to exist;
* **its door faces +X** -- so "face the street" is a rotation, not a guess.

Everything is modelled inside its declared footprint. A market stall's canopy
is cantilevered over its own counter rather than propped on poles in the
walkway; a bridge has no posts at its mouth; a banner hangs off a wall instead
of standing on a mast in the road. Those are not fixes, they are the shapes.

STYLE
-----
Low-poly, hard shapes, small palette, everything bevelled 3 cm so edges catch
the light. That combination is what makes a stylized game look deliberate
rather than cheap, and it is the one look a script can hold perfectly
consistent across forty assets -- which is the whole of the art direction.

UNITS AND AXES
--------------
Modelled in metres, exported to FBX so one metre becomes 100 Unreal units.
Blender +X becomes Unreal +X, so the door convention survives the trip.

Run with:  python3 make_kit.py            (needs the bpy module, not the app)
"""
import json
import math
import os
import sys

import bpy
import bmesh
import addon_utils
from mathutils import Vector

addon_utils.enable("io_scene_fbx")

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)

# ── The palette ────────────────────────────────────────────────────────────
# Small on purpose. Forty assets sharing nine colours read as one world; forty
# assets with their own textures read as an asset store.
PALETTE = {
    "reboco":    ((0.862, 0.800, 0.663), 0.88),   # lime plaster walls
    "reboco2":   ((0.690, 0.616, 0.494), 0.88),   # a second, dirtier plaster
    "madeira":   ((0.180, 0.118, 0.078), 0.80),   # dark structural timber
    "tabua":     ((0.459, 0.310, 0.176), 0.82),   # planks, shutters, carts
    "telha":     ((0.553, 0.216, 0.133), 0.74),   # fired roof tiles
    "telha2":    ((0.404, 0.145, 0.098), 0.76),   # the course under the last
    "colmo":     ((0.678, 0.522, 0.243), 0.94),   # thatch
    "colmo2":    ((0.545, 0.408, 0.180), 0.94),
    "pedra":     ((0.545, 0.492, 0.415), 0.84),   # walls, wells, bridges
    "pedra_esc": ((0.355, 0.312, 0.268), 0.86),   # paving, foundations
    "folha":     ((0.149, 0.318, 0.133), 0.92),   # foliage
    "folha2":    ((0.224, 0.388, 0.157), 0.92),
    "casca":     ((0.224, 0.157, 0.106), 0.90),   # bark
    "vidro":     ((0.090, 0.114, 0.125), 0.22),   # the dark of a window
    "metal":     ((0.286, 0.294, 0.318), 0.42),   # hinges, bands, blades
    "pano":      ((0.560, 0.180, 0.157), 0.92),   # awnings, banners
    "pano2":     ((0.827, 0.765, 0.643), 0.92),
    "brasa":     ((0.949, 0.435, 0.137), 0.60),   # embers -- emissive
    "couro":     ((0.216, 0.125, 0.071), 0.78),   # oiled leather: packs, straps
    "runa":      ((0.980, 0.804, 0.392), 0.45),   # an objective, lit -- emissive
    # ── The animals ────────────────────────────────────────────────────────
    # Four more colours for three more assets, and worth it: an animal in the
    # same nine colours as the buildings is a lump of scenery that moves, and
    # movement without a silhouette of its own reads as a bug rather than as a
    # bird. Feathers, down, fur, and the hard amber of a beak and a hoof.
    "pena":      ((0.075, 0.082, 0.110), 0.52),   # crow: near-black, cold
    "penugem":   ((0.796, 0.714, 0.592), 0.94),   # hen: warm off-white
    "pelo":      ((0.478, 0.333, 0.204), 0.88),   # deer: tan
    "bico":      ((0.741, 0.545, 0.180), 0.55),   # beak, hoof, antler
}
EMISSIVE = {"brasa", "runa"}

# ── How much each material varies face to face ─────────────────────────────
#
# Lucas, twice: "as texturas ainda estao cor cinza". The first answer was a
# per-face brightness of 0.84 to 1.14 -- which sounds like a range until you
# notice vertex colours are eight-bit and clamp at 1.0, so HALF THE FACES came
# out identical and the other half varied by a few per cent. It was invisible,
# and invisible is the same as absent.
#
# This is the same idea done properly, and per material rather than globally,
# because the amount is the art direction: stone is a wall of individual
# stones and wants a lot, plaster is one smooth surface and wants almost none.
# Everything is baked around CENTRE and the material multiplies it back up by
# 1/CENTRE, so the average colour is exactly the palette's and nothing ever
# clamps.
GRAIN = {
    "pedra":     0.25, "pedra_esc": 0.25,   # every stone its own
    "colmo":     0.22, "colmo2":    0.22,   # thatch is straw, not felt
    "casca":     0.20, "folha":     0.20, "folha2": 0.20,
    "tabua":     0.18, "madeira":   0.15,   # planks differ, beams less
    "telha":     0.16, "telha2":    0.16,
    "couro":     0.14,
    "pano":      0.11, "pano2":     0.11,
    "reboco":    0.09, "reboco2":   0.09,   # a rendered wall IS smooth
    "metal":     0.12,
    "vidro":     0.04,
    "brasa":     0.04, "runa":      0.0,    # emissive: variation reads as dirt
    # Feathers and fur vary along the body, not stone by stone: enough to stop a
    # hen reading as one solid blob, far less than a wall of masonry.
    "pena":      0.13, "penugem":   0.15,
    "pelo":      0.14, "bico":      0.08,
}
GRAIN_DEFAULT = 0.15
# What the baked value averages, and what the Unreal material divides back out.
# Keep the two in step: Scripts/Import-Kit.py multiplies by 1 / GRAIN_CENTRE.
GRAIN_CENTRE = 0.78
# Every shade actually baked, so the gain the game divides by is MEASURED
# rather than assumed. The up/down bias pulls the real mean a little under
# GRAIN_CENTRE, and a nominal number that is three per cent off is three per
# cent of darkness applied to every asset in the game.
SHADES = []

MATERIALS = {}


def material(name):
    if name in MATERIALS:
        return MATERIALS[name]
    colour, rough = PALETTE[name]
    made = bpy.data.materials.new("M_Kit_" + name)
    made.use_nodes = True
    bsdf = made.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    if name in EMISSIVE:
        bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 3.0
    MATERIALS[name] = made
    return made


# ── A part is a bag of geometry with one material ──────────────────────────
HULLS = {}


class Build:
    """Accumulates boxes and prisms, then becomes one mesh with material slots.

    It also collects COLLISION boxes. Unreal's FBX importer treats an object
    named ``UCX_<mesh>_00`` as that mesh's collision, and that is the only
    honest way to ship it: an auto-generated convex hull wraps whatever the
    asset contains, so a tree comes into the game as a five-metre invisible
    cylinder around its canopy -- which is exactly what "problemas pra se mover
    especialmente perto de arvores" was. The player was walking into the shadow
    of the leaves.

    Anything that does not declare its own collision gets a box of its DECLARED
    FOOTPRINT, which is the promise the whole kit is built on, now made
    physical: what stops you is the rectangle the piece was modelled inside.
    """

    def __init__(self):
        self.verts = []
        self.faces = []
        self.mats = []
        self.slots = []
        self.hulls = []

    def hull(self, centre, size, yaw=0.0):
        """Declare one collision box, in the mesh's own space."""
        self.hulls.append((centre, size, yaw))

    def slot(self, name):
        if name not in self.slots:
            self.slots.append(name)
        return self.slots.index(name)

    def add(self, points, faces, name):
        index = self.slot(name)
        base = len(self.verts)
        self.verts.extend(points)
        for face in faces:
            self.faces.append([base + i for i in face])
            self.mats.append(index)

    # ── primitives ─────────────────────────────────────────────────────
    def box(self, centre, size, name, yaw=0.0):
        cx, cy, cz = centre
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        corners = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
                   (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
        cos, sin = math.cos(yaw), math.sin(yaw)
        points = [(cx + x * cos - y * sin, cy + x * sin + y * cos, cz + z)
                  for x, y, z in corners]
        self.add(points, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                          (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], name)

    def gable(self, centre, size, name, ridge_along_y=True, overhang=0.0):
        """A roof: a triangular prism with its ridge over the middle."""
        cx, cy, cz = centre
        hx, hy = size[0] / 2 + overhang, size[1] / 2 + overhang
        h = size[2]
        if ridge_along_y:
            points = [(cx - hx, cy - hy, cz), (cx + hx, cy - hy, cz),
                      (cx + hx, cy + hy, cz), (cx - hx, cy + hy, cz),
                      (cx, cy - hy, cz + h), (cx, cy + hy, cz + h)]
            faces = [(0, 3, 2, 1), (0, 1, 4), (2, 3, 5), (1, 2, 5, 4), (3, 0, 4, 5)]
        else:
            points = [(cx - hx, cy - hy, cz), (cx + hx, cy - hy, cz),
                      (cx + hx, cy + hy, cz), (cx - hx, cy + hy, cz),
                      (cx - hx, cy, cz + h), (cx + hx, cy, cz + h)]
            faces = [(0, 3, 2, 1), (0, 4, 3), (1, 2, 5), (0, 1, 5, 4), (2, 3, 4, 5)]
        self.add(points, faces, name)

    def plate(self, quad, thickness, name):
        """A slab lying on an arbitrary quad, extruded along its own normal.

        This is what lets battens sit ON a roof slope instead of near it.
        """
        a, b, c = (Vector(quad[0]), Vector(quad[1]), Vector(quad[2]))
        normal = (b - a).cross(c - b)
        if normal.length < 1e-9:
            return
        normal.normalize()
        lower = [Vector(p) for p in quad]
        upper = [p + normal * thickness for p in lower]
        points = [tuple(p) for p in lower + upper]
        self.add(points, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                          (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], name)

    def roof(self, centre, size, rise, name, shade, courses=5, overhang=0.26,
             ridge_along_x=True, cap="madeira"):
        """A pitched roof with its courses modelled, an eave and a ridge cap.

        The first version of this kit had roofs that were one smooth triangular
        prism with a big overhang, and every house looked like a slab of pink
        resting on a box. Four thin battens per slope and a beam along the
        ridge cost eighty faces and are the difference between "a roof" and
        "a shape on top of a house". Steep, too: a shallow pitch with a wide
        overhang is what made them read as slabs.
        """
        cx, cy, cz = centre
        hx = size[0] / 2 + overhang
        hy = size[1] / 2 + overhang
        self.gable(centre, (size[0], size[1], rise), name,
                   ridge_along_y=not ridge_along_x, overhang=overhang)

        for side in (-1, 1):
            for k in range(courses):
                near, far = k / courses, (k + 0.88) / courses
                # All one colour: the shadow the batten casts on the course
                # below it is the detail. Alternating two tones turned every
                # roof into a deckchair.
                tone = shade if k == courses - 1 else name
                if ridge_along_x:
                    self.plate([(cx - hx, side * hy * (1 - near), cz + rise * near),
                                (cx + hx, side * hy * (1 - near), cz + rise * near),
                                (cx + hx, side * hy * (1 - far), cz + rise * far),
                                (cx - hx, side * hy * (1 - far), cz + rise * far)][::side],
                               0.032, tone)
                else:
                    self.plate([(side * hx * (1 - near), cy - hy, cz + rise * near),
                                (side * hx * (1 - near), cy + hy, cz + rise * near),
                                (side * hx * (1 - far), cy + hy, cz + rise * far),
                                (side * hx * (1 - far), cy - hy, cz + rise * far)][::side],
                               0.032, tone)

        if ridge_along_x:
            self.box((cx, cy, cz + rise + 0.03), (2 * hx + 0.10, 0.22, 0.17), cap)
            for side in (-1, 1):                       # eave fascia
                self.box((cx, side * hy, cz - 0.04), (2 * hx + 0.10, 0.14, 0.20), cap)
        else:
            self.box((cx, cy, cz + rise + 0.03), (0.22, 2 * hy + 0.10, 0.17), cap)
            for side in (-1, 1):
                self.box((side * hx, cy, cz - 0.04), (0.14, 2 * hy + 0.10, 0.20), cap)

    def window(self, at, facing, width, height, frame="madeira"):
        """A recessed opening: dark glass, a frame around it, a sill under it.

        A painted rectangle reads as a sticker. Three boxes read as a window,
        because the frame casts a shadow onto the glass.
        """
        x, y, z = at
        if facing == "y":
            self.box((x, y, z), (width, 0.10, height), "vidro")
            self.box((x, y - 0.04, z + height / 2 + 0.06), (width + 0.20, 0.14, 0.12), frame)
            self.box((x, y - 0.04, z - height / 2 - 0.06), (width + 0.28, 0.20, 0.12), frame)
            for side in (-1, 1):
                self.box((x + side * (width / 2 + 0.07), y - 0.04, z), (0.14, 0.14, height), frame)
            self.box((x, y - 0.04, z), (0.07, 0.12, height), frame)
        else:
            self.box((x, y, z), (0.10, width, height), "vidro")
            self.box((x - 0.04, y, z + height / 2 + 0.06), (0.14, width + 0.20, 0.12), frame)
            self.box((x - 0.04, y, z - height / 2 - 0.06), (0.20, width + 0.28, 0.12), frame)
            for side in (-1, 1):
                self.box((x - 0.04, y + side * (width / 2 + 0.07), z), (0.14, 0.14, height), frame)

    def cylinder(self, centre, radius, height, name, sides=10, top_radius=None):
        cx, cy, cz = centre
        top = radius if top_radius is None else top_radius
        lower, upper = [], []
        for i in range(sides):
            a = 2 * math.pi * i / sides
            lower.append((cx + radius * math.cos(a), cy + radius * math.sin(a), cz))
            upper.append((cx + top * math.cos(a), cy + top * math.sin(a), cz + height))
        points = lower + upper
        faces = [tuple(range(sides - 1, -1, -1)), tuple(range(sides, 2 * sides))]
        for i in range(sides):
            j = (i + 1) % sides
            faces.append((i, j, sides + j, sides + i))
        self.add(points, faces, name)

    def blob(self, centre, radii, name, rings=3, sides=7, squash=1.0):
        """A faceted lump -- foliage, boulders. Low-poly on purpose."""
        cx, cy, cz = centre
        rx, ry, rz = radii
        points = [(cx, cy, cz + rz * squash)]
        for r in range(1, rings + 1):
            pitch = math.pi * r / (rings + 1)
            for s in range(sides):
                yaw = 2 * math.pi * s / sides + r * 0.37
                points.append((cx + rx * math.sin(pitch) * math.cos(yaw),
                               cy + ry * math.sin(pitch) * math.sin(yaw),
                               cz + rz * math.cos(pitch) * squash))
        points.append((cx, cy, cz - rz * squash))
        faces = []
        for s in range(sides):
            faces.append((0, 1 + s, 1 + (s + 1) % sides))
        for r in range(rings - 1):
            a, b = 1 + r * sides, 1 + (r + 1) * sides
            for s in range(sides):
                t = (s + 1) % sides
                faces.append((a + s, b + s, b + t, a + t))
        last = 1 + (rings - 1) * sides
        bottom = len(points) - 1
        for s in range(sides):
            faces.append((bottom, last + (s + 1) % sides, last + s))
        self.add(points, faces, name)

    # ── becoming a real object ─────────────────────────────────────────
    def finish(self, name, bevel=0.03):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.verts, [], self.faces)
        mesh.validate(verbose=False)
        for slot in self.slots:
            mesh.materials.append(material(slot))
        for face, index in zip(mesh.polygons, self.mats):
            face.material_index = index
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)

        # Bevel then shade: the 3 cm chamfer is what stops a box looking like a
        # box, and it is the cheapest stylization there is.
        if bevel > 0:
            bm = bmesh.new()
            bm.from_mesh(mesh)
            bmesh.ops.bevel(bm, geom=list(bm.verts) + list(bm.edges),
                            offset=bevel, segments=1, affect="EDGES",
                            clamp_overlap=True, profile=0.6)
            bm.to_mesh(mesh)
            bm.free()
        mesh.shade_flat()

        # ── A different shade for every face, baked into the mesh ────────
        #
        # Lucas: "as texturas ainda estao cor cinza". They were: nine flat
        # colours and nothing else, so a stone wall was one unbroken grey
        # rectangle and read as untextured, because it was.
        #
        # Textures are not the only answer and they are the expensive one.
        # A random brightness PER FACE, hashed from where the face is so it
        # never flickers between builds, turns one grey into a wall of
        # individually shaded stones -- and it costs nothing: no texture
        # memory, no UV work, no sampler. It is the oldest trick in low-poly
        # and the reason the style works at all.
        colours = mesh.color_attributes.new(name="Col", type="BYTE_COLOR",
                                            domain="CORNER")
        for face in mesh.polygons:
            # Hashed on a TWELVE-CENTIMETRE grid, not on the face's exact
            # position. At one-centimetre resolution the two faces of the
            # same little stone got different shades, which reads as noise;
            # at twelve centimetres a stone is one shade and its neighbour is
            # another, which reads as stones. The grid is roughly the size of
            # the smallest thing in the kit that should be uniform.
            middle = face.center
            seed = (int(middle.x * 8.0) * 73856093
                    ^ int(middle.y * 8.0) * 19349663
                    ^ int(middle.z * 8.0) * 83492791)
            spread = GRAIN.get(self.slots[face.material_index], GRAIN_DEFAULT)
            roll = ((seed >> 7) & 0xFF) / 255.0 * 2.0 - 1.0      # -1 .. +1
            shade = GRAIN_CENTRE * (1.0 + roll * spread)
            # Upward faces a touch brighter, downward a touch darker: the
            # cheapest ambient occlusion there is, and it survives any light.
            # Deliberately sized so the brightest face in the kit lands just
            # under 1.0 -- an eight-bit vertex colour clamps there, and a
            # clamped range is a range that quietly stops varying.
            shade *= 0.94 + 0.08 * max(0.0, face.normal.z)
            shade = min(shade, 1.0)
            SHADES.append(shade)
            for loop in face.loop_indices:
                colours.data[loop].color = (shade, shade, shade, 1.0)

        HULLS[name] = list(self.hulls)
        return obj


# ══════════════════════════════════════════════════════════════════════════
#  The assets. Each returns (object, footprint_x, footprint_y, height, kind).
#  The door, where there is one, faces +X. Pivot is on the ground, centred.
# ══════════════════════════════════════════════════════════════════════════
CATALOGUE = []


def asset(name, kind, foot_x, foot_y):
    def wrap(maker):
        CATALOGUE.append((name, kind, foot_x, foot_y, maker))
        return maker
    return wrap


def timbering(b, depth, width, base, wall_h, wood="madeira"):
    """Corner posts, a mid rail and diagonal braces on the two long walls.

    One detail, and the single biggest one in the kit: a plastered box with
    black timber on it is a medieval house, and the same box without it is a
    box. The braces are what your eye reads as "built", because they are the
    only diagonals anywhere on the building.
    """
    hx, hy = depth / 2, width / 2
    mid = base + wall_h / 2
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((sx * (hx - 0.10), sy * (hy - 0.10), mid), (0.20, 0.20, wall_h), wood)
    for sy in (-1, 1):
        b.box((0, sy * (hy + 0.01), base + wall_h - 0.09), (depth, 0.12, 0.18), wood)
        b.box((0, sy * (hy + 0.01), base + 0.09), (depth, 0.12, 0.18), wood)
        for lean in (-1, 1):
            for slice_i in range(5):
                t = slice_i / 5.0
                b.box((lean * (hx - 0.22) * (1 - t * 0.55),
                       sy * (hy + 0.01),
                       base + 0.20 + t * (wall_h - 0.42)),
                      (0.30, 0.10, (wall_h - 0.42) / 5 + 0.10), wood)
    for sx in (-1, 1):
        b.box((sx * (hx + 0.01), 0, base + wall_h - 0.09), (0.12, width, 0.18), wood)


def cottage(b, depth, width, wall_h, storeys, plaster, roof_mat, roof_shade):
    """The common body of every house: plinth, walls, timbering, roof, door."""
    hx, hy = depth / 2, width / 2
    b.box((0, 0, 0.14), (depth + 0.34, width + 0.34, 0.28), "pedra_esc")   # plinth
    b.box((0, 0, 0.30), (depth + 0.22, width + 0.22, 0.10), "pedra")

    total = wall_h * storeys
    b.box((0, 0, 0.34 + total / 2), (depth, width, total), plaster)
    for floor in range(storeys):
        timbering(b, depth, width, 0.34 + floor * wall_h, wall_h)
    if storeys > 1:
        b.box((0, 0, 0.34 + wall_h), (depth + 0.14, width + 0.14, 0.20), "madeira")

    # Steep on purpose: a 45-degree pitch with a modest overhang. The first
    # version was shallow with a wide overhang and every house looked like a
    # slab resting on a box.
    rise = 0.56 * width
    b.roof((0, 0, 0.34 + total), (depth, width), rise, roof_mat, roof_shade,
           courses=4, overhang=0.24, ridge_along_x=True)

    # Door on +X, recessed under a lintel, with planks and iron.
    b.box((hx - 0.06, 0, 0.34 + 1.05), (0.18, 1.14, 2.10), "tabua")
    for at in (-0.36, 0.0, 0.36):
        b.box((hx + 0.02, at, 0.34 + 1.05), (0.05, 0.07, 2.00), "madeira")
    b.box((hx + 0.02, 0, 0.34 + 2.22), (0.28, 1.46, 0.22), "madeira")
    b.box((hx + 0.04, 0, 0.34 + 0.42), (0.06, 1.02, 0.10), "metal")
    b.box((hx + 0.04, 0, 0.34 + 1.70), (0.06, 1.02, 0.10), "metal")
    b.box((hx + 0.06, -0.38, 0.34 + 1.05), (0.08, 0.10, 0.14), "metal")
    b.box((hx + 0.03, 0, 0.34 + 2.44), (0.40, 0.92, 0.10), "tabua")       # hood

    # Windows on the long sides, and one in the gable over the door.
    for sy in (-1, 1):
        for along in (-0.27, 0.27):
            b.window((along * depth, sy * (hy + 0.03), 0.34 + wall_h * 0.58),
                     "y" if sy > 0 else "y", 0.82, 0.86)
    if storeys > 1:
        for sy in (-1, 1):
            for along in (-0.27, 0.27):
                b.window((along * depth, sy * (hy + 0.03),
                          0.34 + wall_h + wall_h * 0.58), "y", 0.76, 0.78)
    b.window((hx + 0.03, 0, 0.34 + total + rise * 0.34), "x", 0.70, 0.62)
    return b


def chimney(b, at, width, top):
    x, y = at
    b.box((x, y, top / 2), (width, width, top), "pedra")
    b.box((x, y, top + 0.09), (width + 0.26, width + 0.26, 0.18), "pedra_esc")
    for sx in (-1, 1):
        b.box((x + sx * (width / 2 - 0.05), y, top + 0.34), (0.12, width * 0.5, 0.32), "pedra")


# ── The houses ─────────────────────────────────────────────────────────────
# One builder and a table, not eight hand-written functions.
#
# This is the answer to the oldest complaint in the project -- *"as casas
# continuam sendo repetitivas"*. The village pack had fourteen house
# blueprints and the valley still read as a row of the same building, because
# fourteen is a small number when a village has forty houses. A parameterised
# builder is a different kind of answer: the table below is eight, adding a
# ninth is one line, and every one of them is genuinely a different building
# rather than the same one retextured.
#
# depth, width, wall height, storeys, plaster, roof, shade, chimney x,
# porch over the door, jettied upper floor, stone ground floor
HOUSE_KINDS = [
    ("casa_a", 5.2, 4.4, 2.9, 1, "reboco",  "telha", "telha2", -1.2, False, False, False),
    ("casa_b", 6.6, 5.2, 2.8, 2, "reboco2", "telha", "telha2",  1.6, True,  False, False),
    ("casa_c", 8.0, 6.0, 2.9, 2, "reboco",  "telha", "telha2", -2.2, False, True,  True),
    ("casa_d", 4.6, 4.0, 2.7, 1, "reboco2", "colmo", "colmo2",  0.9, False, False, False),
    ("casa_e", 7.2, 4.8, 2.8, 1, "reboco",  "colmo", "colmo2", -1.9, True,  False, False),
    ("casa_f", 5.8, 5.8, 3.0, 2, "reboco2", "telha", "telha2",  1.3, False, False, False),
    ("casa_g", 9.0, 5.4, 2.7, 2, "reboco",  "telha", "telha2", -2.7, True,  True,  False),
    ("casa_h", 4.2, 3.6, 2.6, 1, "reboco2", "colmo", "colmo2",  0.8, False, False, False),
]


def chimney(b, at, width, top):
    x, y = at
    b.box((x, y, top / 2), (width, width, top), "pedra")
    b.box((x, y, top + 0.09), (width + 0.26, width + 0.26, 0.18), "pedra_esc")
    for sx in (-1, 1):
        b.box((x + sx * (width / 2 - 0.05), y, top + 0.34), (0.12, width * 0.5, 0.32), "pedra")


def porch(b, depth):
    """A roof over the door carried on two corbels off the wall.

    Never on posts. A post beside a door is a post in the doorway, and that is
    half of what "coisas atrapalhando o caminho" was."""
    face = depth / 2
    b.box((face + 0.62, 0, 3.32), (1.40, 2.30, 0.16), "tabua")
    b.box((face + 0.10, 0, 3.46), (0.34, 2.30, 0.32), "madeira")
    for sy in (-1, 1):
        b.box((face + 0.04, sy * 0.96, 3.02), (0.50, 0.16, 0.44), "madeira")


def build_house(name, depth, width, wall_h, storeys, plaster, roof_mat, roof_shade,
                chimney_x, has_porch, jettied, stone_base):
    b = Build()
    b.box((0, 0, 0.14), (depth + 0.34, width + 0.34, 0.28), "pedra_esc")
    b.box((0, 0, 0.30), (depth + 0.22, width + 0.22, 0.10), "pedra")

    hx, hy = depth / 2, width / 2
    upper_w = width + (0.6 if jettied else 0.0)
    total = wall_h * storeys

    if stone_base:
        b.box((0, 0, 0.34 + wall_h / 2), (depth, width, wall_h), "pedra")
        timbering(b, depth, width, 0.34, wall_h, wood="pedra_esc")
    else:
        b.box((0, 0, 0.34 + wall_h / 2), (depth, width, wall_h), plaster)
        timbering(b, depth, width, 0.34, wall_h)

    if storeys > 1:
        b.box((0, 0, 0.34 + wall_h + wall_h / 2), (depth, upper_w, wall_h), plaster)
        timbering(b, depth, upper_w, 0.34 + wall_h, wall_h)
        b.box((0, 0, 0.34 + wall_h), (depth + 0.14, upper_w + 0.14, 0.20), "madeira")
        if jettied:
            # Corbels under the oversail. A jetty without visible brackets
            # reads as a modelling mistake rather than as a first floor.
            for sy in (-1, 1):
                for along in (-0.35, -0.12, 0.12, 0.35):
                    b.box((along * depth, sy * (width / 2 + 0.15), 0.34 + wall_h - 0.08),
                          (0.30, 0.46, 0.34), "madeira")

    rise = 0.56 * upper_w
    b.roof((0, 0, 0.34 + total), (depth, upper_w), rise, roof_mat, roof_shade,
           courses=4 + storeys, overhang=0.24 + (0.14 if roof_mat == "colmo" else 0.0))

    # Door on +X: recessed, planked, lintelled, ironed.
    b.box((hx - 0.06, 0, 0.34 + 1.05), (0.18, 1.14, 2.10), "tabua")
    for at in (-0.36, 0.0, 0.36):
        b.box((hx + 0.02, at, 0.34 + 1.05), (0.05, 0.07, 2.00), "madeira")
    b.box((hx + 0.02, 0, 0.34 + 2.22), (0.28, 1.46, 0.22), "madeira")
    for z in (0.42, 1.70):
        b.box((hx + 0.04, 0, 0.34 + z), (0.06, 1.02, 0.10), "metal")
    b.box((hx + 0.06, -0.38, 0.34 + 1.05), (0.08, 0.10, 0.14), "metal")

    for sy in (-1, 1):
        for along in (-0.27, 0.27):
            b.window((along * depth, sy * (hy + 0.03), 0.34 + wall_h * 0.58), "y", 0.82, 0.86)
    if storeys > 1:
        uy = upper_w / 2
        for sy in (-1, 1):
            for along in (-0.27, 0.27):
                b.window((along * depth, sy * (uy + 0.03),
                          0.34 + wall_h + wall_h * 0.58), "y", 0.76, 0.78)
    b.window((hx + 0.03, 0, 0.34 + total + rise * 0.34), "x", 0.70, 0.62)

    chimney(b, (chimney_x, 0.0), 0.72 + 0.06 * storeys, 0.34 + total + rise * 0.86)
    if has_porch:
        porch(b, depth)
    return b.finish("SM_Kit_" + name)


for _spec in HOUSE_KINDS:
    CATALOGUE.append(("SM_Kit_" + _spec[0], "house", _spec[1], _spec[2],
                      (lambda s=_spec: build_house(*s))))


@asset("SM_Kit_celeiro", "barn", 10.0, 7.0)
def celeiro():
    b = Build()
    b.box((0, 0, 0.14), (10.3, 7.3, 0.28), "pedra_esc")
    b.box((0, 0, 0.28 + 1.9), (10.0, 7.0, 3.8), "tabua")
    for along in (-4.0, -2.0, 0.0, 2.0, 4.0):
        for sy in (-1, 1):
            b.box((along, sy * 3.52, 0.28 + 1.9), (0.22, 0.14, 3.8), "madeira")
    b.roof((0, 0, 4.08), (10.0, 7.0), 3.9, "colmo", "colmo2",
           courses=6, overhang=0.42)
    # Cart doors, the full height of the wall, on +X.
    b.box((5.0 - 0.06, 0, 0.28 + 1.55), (0.20, 3.20, 3.10), "madeira")
    for at in (-1.2, -0.4, 0.4, 1.2):
        b.box((5.06, at, 0.28 + 1.55), (0.06, 0.12, 3.00), "tabua")
    b.box((5.06, 0, 0.28 + 1.55), (0.08, 0.20, 3.10), "metal")
    b.box((5.06, 0, 0.28 + 2.90), (0.08, 3.10, 0.14), "metal")
    return b.finish("SM_Kit_celeiro")


@asset("SM_Kit_torre", "tower", 3.8, 3.8)
def torre():
    b = Build()
    b.box((0, 0, 0.2), (4.4, 4.4, 0.4), "pedra_esc")
    for course in range(9):
        jog = (course % 2) * 0.035
        b.box((0, 0, 0.78 + course * 0.755), (3.4 - jog, 3.4 - jog, 0.72), "pedra")
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((sx * 1.62, sy * 1.62, 3.8), (0.30, 0.30, 6.8), "pedra_esc")
    # The gallery oversails, so the tower has a silhouette from far away --
    # this is a landmark, its whole job is to be recognised at 400 m.
    b.box((0, 0, 7.45), (4.6, 4.6, 0.30), "madeira")
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((sx * 1.75, sy * 1.75, 7.16), (0.34, 0.34, 0.52), "madeira")
    b.box((0, 0, 8.10), (4.2, 4.2, 1.10), "tabua")
    for sy in (-1, 1):
        b.box((0, sy * 2.12, 8.10), (4.2, 0.12, 1.10), "madeira")
        b.box((sy * 2.12, 0, 8.10), (0.12, 4.2, 1.10), "madeira")
    b.roof((0, 0, 8.65), (4.4, 4.4), 2.4, "telha", "telha2",
           courses=4, overhang=0.30)
    b.box((1.7 - 0.05, 0, 1.45), (0.18, 1.06, 2.16), "tabua")
    b.box((1.78, 0, 2.60), (0.26, 1.40, 0.20), "pedra_esc")
    for sy in (-1, 1):
        b.window((0, sy * 1.74, 4.9), "y", 0.52, 1.30)
        b.window((sy * 1.74, 0, 4.9), "x", 0.52, 1.30)
    return b.finish("SM_Kit_torre")


@asset("SM_Kit_poco", "well", 2.2, 2.2)
def poco():
    b = Build()
    b.cylinder((0, 0, 0.0), 1.05, 0.95, "pedra", sides=12)
    b.cylinder((0, 0, 0.95), 0.92, 0.10, "pedra_esc", sides=12)
    for sy in (-1, 1):
        b.box((0, sy * 0.86, 1.70), (0.18, 0.18, 1.50), "madeira")
    b.roof((0, 0, 2.30), (1.7, 2.2), 0.95, "telha", "telha2",
           courses=3, overhang=0.30, ridge_along_x=False)
    b.cylinder((0, 0, 2.15), 0.09, 0.0, "madeira", sides=6)
    b.box((0, 0, 2.18), (0.14, 1.70, 0.14), "madeira")
    b.box((0, 0, 1.55), (0.55, 0.55, 0.45), "tabua")
    return b.finish("SM_Kit_poco")


@asset("SM_Kit_banca", "stall", 2.8, 3.6)
def banca():
    """A market stall whose canopy hangs over its OWN counter.

    The pack's stalls had four poles, and two of them always ended up in the
    street. This one has two posts, both behind the counter, and the awning
    cantilevers forward off a beam. Nothing it owns sticks into the walkway.
    """
    b = Build()
    # The counter and its two back posts. The awning hangs over the customer
    # and must not be something the customer walks into.
    b.hull((-0.55, 0.0, 0.50), (0.60, 3.44, 1.00))
    for sy in (-1, 1):
        b.hull((-1.25, sy * 1.60, 1.15), (0.20, 0.20, 2.30))
    b.box((-0.55, 0, 0.95), (0.55, 3.40, 0.10), "tabua")            # counter top
    b.box((-0.55, 0, 0.45), (0.45, 3.20, 0.90), "madeira")          # counter body
    for sy in (-1, 1):
        b.box((-1.25, sy * 1.60, 1.15), (0.14, 0.14, 2.30), "madeira")
    b.box((-1.25, 0, 2.30), (0.16, 3.40, 0.16), "madeira")          # back beam
    # The awning: a shallow wedge leaning out over the counter, held by the
    # beam alone.
    # The awning: four cloth panels falling away from the beam, alternating
    # colour. One solid wedge read as a billboard in the first sheet.
    for panel in range(4):
        y0 = -1.80 + panel * 0.90
        tone = "pano" if panel % 2 else "pano2"
        b.add([(-1.25, y0, 2.44), (0.98, y0, 2.02), (0.98, y0 + 0.90, 2.02),
               (-1.25, y0 + 0.90, 2.44), (-1.25, y0, 2.40), (0.98, y0, 1.98),
               (0.98, y0 + 0.90, 1.98), (-1.25, y0 + 0.90, 2.40)],
              [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
               (2, 3, 7, 6), (3, 0, 4, 7)], tone)
    b.box((0.98, 0, 1.90), (0.10, 3.60, 0.14), "madeira")          # awning lip
    # Goods, so the stall reads as a stall and not as furniture.
    b.box((-0.55, -1.15, 1.16), (0.38, 0.38, 0.32), "tabua")
    b.box((-0.55, -1.15, 1.40), (0.30, 0.30, 0.16), "colmo")
    b.box((-0.55, 0.85, 1.14), (0.32, 0.46, 0.28), "tabua")
    b.blob((-0.55, 0.30, 1.10), (0.22, 0.22, 0.14), "pano", rings=2, sides=6)
    b.box((-0.90, 1.45, 1.30), (0.26, 0.26, 0.60), "metal")
    return b.finish("SM_Kit_banca")


@asset("SM_Kit_ponte", "bridge", 8.0, 4.4)
def ponte():
    """Eight metres of crossing with NOTHING at its mouth.

    Every bridge in the old world had a newel post at each corner, exactly where
    you walk on. The parapets here start 90 cm in from each end and stop 90 cm
    short of the other, so both approaches are clear across the full width.
    """
    b = Build()
    # A deck you stand ON, two parapets, and nothing at the mouths.
    b.hull((0.0, 0.0, 0.28), (8.0, 4.4, 0.56))
    for sy in (-1, 1):
        b.hull((0.0, sy * 2.02, 0.92), (6.2, 0.30, 0.78))
    b.box((0, 0, 0.30), (8.0, 4.4, 0.34), "pedra")
    b.box((0, 0, 0.50), (8.0, 4.0, 0.10), "tabua")
    for sy in (-1, 1):
        b.box((0, sy * 2.02, 0.80), (6.2, 0.26, 0.50), "pedra")
    for sy in (-1, 1):
        for along in (-2.4, 0.0, 2.4):
            b.box((along, sy * 2.02, 1.18), (0.44, 0.34, 0.26), "pedra_esc")
    for sx in (-1, 1):
        b.box((sx * 3.9, 0, 0.16), (0.5, 4.4, 0.32), "pedra_esc")   # ramped ends
    return b.finish("SM_Kit_ponte")


@asset("SM_Kit_muro", "wall", 4.0, 0.7)
def muro():
    """Coursed rubble: five bands of stone, each one offset a centimetre or two.

    A wall modelled as one grey box is a grey box. The bands cost thirty faces
    and give it a shadow every forty centimetres, which is all "stone" is at
    this distance.
    """
    b = Build()
    for course in range(5):
        jog = (course % 2) * 0.03
        b.box((0, 0, 0.21 + course * 0.40), (4.0 - jog, 0.58 - jog, 0.38), "pedra")
    b.box((0, 0, 2.14), (4.08, 0.72, 0.18), "pedra_esc")
    for at in (-1.35, 0.0, 1.35):
        b.box((at, 0, 1.05), (0.10, 0.62, 2.00), "pedra_esc")
    return b.finish("SM_Kit_muro")


@asset("SM_Kit_cerca", "fence", 4.0, 0.3)
def cerca():
    b = Build()
    for along in (-1.95, -0.65, 0.65, 1.95):
        b.box((along, 0, 0.58), (0.14, 0.14, 1.16), "madeira")
    for z in (0.45, 0.95):
        b.box((0, 0, z), (4.0, 0.08, 0.12), "tabua")
    return b.finish("SM_Kit_cerca")


@asset("SM_Kit_portao", "gate", 1.2, 5.6)
def portao():
    """A gateway you walk THROUGH. The clear opening is 3.4 m: wider than any
    road slab, so the arch can never be the thing standing in the way."""
    b = Build()
    # The two jambs only: a 3.4 m opening you can drive a cart through, and a
    # roof overhead that nothing has to path around.
    for sy in (-1, 1):
        b.hull((0.0, sy * 2.25, 1.80), (1.30, 1.30, 3.60))
    for sy in (-1, 1):
        b.box((0, sy * 2.25, 1.65), (1.10, 1.10, 3.30), "pedra")
        b.box((0, sy * 2.25, 3.42), (1.30, 1.30, 0.24), "pedra_esc")
    b.box((0, 0, 3.75), (1.00, 5.60, 0.42), "pedra")
    b.roof((0, 0, 3.96), (1.2, 5.6), 0.80, "telha", "telha2",
           courses=3, overhang=0.24, ridge_along_x=False)
    for sy in (-1, 1):
        for z in (0.55, 1.35, 2.15, 2.95):
            b.box((0, sy * 2.25, z), (1.18, 1.18, 0.10), "pedra_esc")
    return b.finish("SM_Kit_portao")


@asset("SM_Kit_placa", "sign", 1.0, 2.2)
def placa():
    b = Build()
    b.hull((0.0, 0.0, 1.30), (0.62, 0.62, 2.60))
    b.box((0, 0, 1.30), (0.22, 0.22, 2.60), "madeira")
    for sy, z, length in ((1, 2.24, 1.50), (-1, 1.72, 1.24)):
        b.box((0, sy * (length / 2 + 0.10), z), (0.11, length, 0.46), "tabua")
        b.box((0, sy * (length / 2 + 0.10), z + 0.27), (0.15, length + 0.10, 0.09), "madeira")
        b.box((0, sy * 0.22, z - 0.30), (0.10, 0.10, 0.34), "metal")
    b.box((0, 0, 2.66), (0.34, 0.34, 0.22), "madeira")
    b.box((0, 0, 0.12), (0.62, 0.62, 0.24), "pedra_esc")
    for i in range(7):
        a = 2 * math.pi * i / 7
        b.box((0.36 * math.cos(a), 0.36 * math.sin(a), 0.09),
              (0.26, 0.20, 0.18), "pedra", yaw=a)
    return b.finish("SM_Kit_placa")


# ── What fills the kilometre between the villages ──────────────────────────
# Ninety per cent of the map is untouched Landscape, and that was right for
# mobility and wrong for everything else: an open world with nothing in it is
# a field. These are the pieces the points of interest are assembled from.

@asset("SM_Kit_tenda", "tent", 3.0, 2.6)
def tenda():
    """A bandit tent, and the reason a camp reads as a camp.

    No guy ropes. The pack's tents had them and they were invisible to every
    placement rule in the game, which is how you end up walking into a string.
    This one stands on its own two poles, both INSIDE the footprint, and its
    canvas reaches the ground on both long sides.
    """
    b = Build()
    b.hull((0.0, 0.0, 0.92), (2.80, 2.60, 1.84))
    for sx in (-1, 1):
        b.box((sx * 1.28, 0, 0.92), (0.13, 0.13, 1.84), "madeira")
    b.box((0, 0, 1.80), (2.72, 0.12, 0.12), "madeira")             # ridge pole
    for side in (-1, 1):
        for step in range(5):
            near, far = step / 5.0, (step + 0.98) / 5.0
            b.plate([(-1.34, side * 1.28 * (1 - near), 0.06 + 1.74 * near),
                     (1.34, side * 1.28 * (1 - near), 0.06 + 1.74 * near),
                     (1.34, side * 1.28 * (1 - far), 0.06 + 1.74 * far),
                     (-1.34, side * 1.28 * (1 - far), 0.06 + 1.74 * far)][::side],
                    0.05, "pano2" if step % 2 else "pano")
    b.add([(-1.36, -1.30, 0.0), (-1.36, 1.30, 0.0), (-1.36, 0, 1.82)],
          [(0, 1, 2)], "pano")                                     # back gable
    for at in (-1.22, -0.42, 0.42, 1.22):                          # pegs
        for side in (-1, 1):
            b.box((at, side * 1.32, 0.05), (0.14, 0.14, 0.10), "madeira")
    b.box((0.98, 0, 0.30), (0.62, 0.52, 0.60), "tabua")            # a chest by the door
    return b.finish("SM_Kit_tenda", bevel=0.02)


@asset("SM_Kit_altar", "altar", 2.0, 1.4)
def altar():
    b = Build()
    b.box((0, 0, 0.11), (1.96, 1.36, 0.22), "pedra_esc")
    for sx in (-1, 1):
        b.box((sx * 0.62, 0, 0.58), (0.44, 0.92, 0.72), "pedra")
    b.box((0, 0, 1.02), (1.84, 1.20, 0.22), "pedra")
    b.box((0, 0, 1.16), (1.30, 0.80, 0.08), "pedra_esc")
    b.blob((0, 0, 1.26), (0.22, 0.18, 0.10), "brasa", rings=2, sides=6)
    return b.finish("SM_Kit_altar", bevel=0.035)


@asset("SM_Kit_arco", "arch", 1.2, 4.2)
def arco():
    """A ruined arch: two piers, a broken springer on each, no lintel.

    The gap is three metres wide, so a road can run through it and the arch can
    never be the thing standing in the way -- which is the rule the whole kit
    exists to make structural rather than hoped for.
    """
    b = Build()
    # Two piers and NOTHING between them. The three metres in the middle are
    # the whole point of an arch, and an auto hull would have filled them in.
    for side in (-1, 1):
        b.hull((0.0, side * 1.72, 2.0), (1.10, 1.02, 4.0))
    for side in (-1, 1):
        for course in range(6):
            jog = (course % 2) * 0.04
            b.box((0, side * 1.72, 0.30 + course * 0.58),
                  (1.02 - jog, 0.94 - jog, 0.56), "pedra")
        b.box((0, side * 1.72, 3.72), (1.10, 1.02, 0.20), "pedra_esc")
        for step in range(3):
            b.box((0, side * (1.52 - step * 0.22), 3.96 + step * 0.30),
                  (0.86, 0.60, 0.34), "pedra")
    for side in (-1, 1):
        for i in range(4):
            a = 2 * math.pi * i / 4 + side
            b.box((0.58 * math.cos(a), side * 1.72 + 0.58 * math.sin(a), 0.11),
                  (0.42, 0.34, 0.22), "pedra_esc", yaw=a)
    return b.finish("SM_Kit_arco", bevel=0.04)


@asset("SM_Kit_lapide", "grave", 1.1, 0.9)
def lapide():
    b = Build()
    b.box((0, 0, 0.07), (1.02, 0.82, 0.14), "pedra_esc")
    b.add([(-0.30, -0.10, 0.10), (0.30, -0.10, 0.10), (0.30, 0.10, 0.10),
           (-0.30, 0.10, 0.10), (-0.26, -0.09, 0.86), (0.26, -0.09, 0.86),
           (0.26, 0.09, 0.86), (-0.26, 0.09, 0.86)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], "pedra")
    b.box((0, 0, 0.94), (0.46, 0.22, 0.16), "pedra")               # rounded head
    b.box((0, -0.10, 0.62), (0.26, 0.05, 0.08), "pedra_esc")       # carved mark
    b.box((0, -0.10, 0.50), (0.08, 0.05, 0.30), "pedra_esc")
    return b.finish("SM_Kit_lapide", bevel=0.025)


@asset("SM_Kit_torre_ruina", "ruin", 4.4, 4.4)
def torre_ruina():
    """Half a tower: whole to head height, then two walls climbing and the
    other two gone.

    The first attempt shrank every course a little as it rose and came out as
    a stepped wedding cake -- a shape that reads as "unfinished model", not as
    "ruin". A ruin is a building with PARTS MISSING, so the courses stay the
    same width and the walls simply stop, at different heights, with the
    rubble they dropped lying around the base.
    """
    b = Build()
    half, thick = 1.80, 0.46
    b.box((0, 0, 0.22), (4.4, 4.4, 0.44), "pedra_esc")

    def wall(along_x, side, courses, start=0):
        for course in range(start, courses):
            jog = (course % 2) * 0.05
            z = 0.62 + course * 0.66
            if along_x:
                b.box((0, side * (half - thick / 2), z),
                      (2 * half - jog, thick, 0.64), "pedra")
            else:
                b.box((side * (half - thick / 2), 0, z),
                      (thick, 2 * half - jog, 0.64), "pedra")

    wall(True, -1, 7)                 # the wall that still stands
    wall(False, -1, 6)                # its neighbour, a course shorter
    wall(True, 1, 3)                  # broken off at head height
    wall(False, 1, 2)                 # barely a stub
    # A jagged crown on the standing wall: three merlons of different heights.
    for at, tall in ((-1.15, 0.52), (-0.10, 0.34), (1.05, 0.62)):
        b.box((at, -(half - thick / 2), 5.24 + tall / 2), (0.78, thick, tall), "pedra")
    b.box((1.60, -0.10, 1.30), (0.30, 1.10, 2.20), "pedra_esc")   # empty doorway
    b.box((-0.30, -(half - 0.05), 3.30), (0.90, 0.16, 1.00), "pedra_esc")  # window hole

    for i in range(9):                                            # fallen rubble
        a = 2 * math.pi * i / 9 + 0.6
        reach = 2.4 + (i % 3) * 0.35
        b.blob((reach * math.cos(a), reach * math.sin(a), 0.20),
               (0.48 + (i % 2) * 0.14, 0.40, 0.24),
               "pedra" if i % 2 else "pedra_esc", rings=2, sides=6)
    return b.finish("SM_Kit_torre_ruina", bevel=0.05)


@asset("SM_Kit_palicada", "palisade", 3.6, 0.5)
def palicada():
    """A run of sharpened stakes. What turns a clearing into a held position,
    and the thing a bandit fort is built out of."""
    b = Build()
    for step in range(9):
        x = -1.6 + step * 0.40
        tall = 2.05 + ((step * 7) % 5) * 0.06
        b.cylinder((x, 0, 0), 0.115, tall, "madeira", sides=6, top_radius=0.085)
        b.cylinder((x, 0, tall), 0.085, 0.24, "madeira", sides=6, top_radius=0.012)
    for z in (0.65, 1.55):
        b.box((0, 0.14, z), (3.6, 0.09, 0.13), "tabua")
    return b.finish("SM_Kit_palicada", bevel=0.02)


@asset("SM_Kit_lampiao", "lamp", 0.7, 0.7)
def lampiao():
    """A street lamp whose post is 18 cm thick and stands in its own base.

    The pack's lamps were the other half of "coisas atrapalhando o caminho":
    a lamp is placed at the edge of a road and its bracket swung out over the
    carriageway. This one is symmetrical and the lantern sits directly over the
    post, so its footprint and its silhouette are the same square.
    """
    b = Build()
    b.hull((0.0, 0.0, 1.20), (0.44, 0.44, 2.40))
    for i in range(6):
        a = 2 * math.pi * i / 6
        b.box((0.20 * math.cos(a), 0.20 * math.sin(a), 0.075), (0.24, 0.20, 0.15), "pedra", yaw=a)
    b.box((0, 0, 0.19), (0.34, 0.34, 0.12), "pedra_esc")
    b.cylinder((0, 0, 0.24), 0.085, 2.42, "madeira", sides=6, top_radius=0.062)
    b.box((0, 0, 2.70), (0.26, 0.26, 0.10), "metal")
    b.box((0, 0, 2.94), (0.30, 0.30, 0.40), "brasa")               # the light itself
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((sx * 0.155, sy * 0.155, 2.94), (0.050, 0.050, 0.44), "metal")
    b.box((0, 0, 3.18), (0.38, 0.38, 0.08), "metal")
    b.gable((0, 0, 3.22), (0.34, 0.34, 0.24), "metal", ridge_along_y=False, overhang=0.06)
    return b.finish("SM_Kit_lampiao", bevel=0.015)


@asset("SM_Kit_menir", "stone", 1.4, 1.1)
def menir():
    """A standing stone. Ruins need something vertical that is not a building,
    and a ring of these is a landmark you can steer by from four hundred
    metres -- which is the whole job of a landmark."""
    b = Build()
    lean = 0.09
    b.add([(-0.52, -0.34, 0.0), (0.52, -0.34, 0.0), (0.58, 0.34, 0.0), (-0.46, 0.34, 0.0),
           (-0.38 + lean, -0.26, 2.30), (0.40 + lean, -0.26, 2.42),
           (0.44 + lean, 0.26, 2.36), (-0.34 + lean, 0.26, 2.24)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], "pedra")
    for i in range(5):
        a = 2 * math.pi * i / 5 + 0.4
        b.box((0.52 * math.cos(a), 0.42 * math.sin(a), 0.10),
              (0.34, 0.26, 0.20), "pedra_esc", yaw=a)
    return b.finish("SM_Kit_menir", bevel=0.05)


@asset("SM_Kit_piso", "paving", 4.0, 4.0)
def piso():
    """One road tile, four metres square, with its stones MODELLED.

    The old road was a flat plane with a stone texture stretched over it, which
    is why the bricks came out the size of a person and the seams between tiles
    showed. Sixteen slabs of real geometry, each a couple of centimetres proud
    of its neighbours, tile seamlessly and never need a texture at all.
    """
    b = Build()
    b.hull((0.0, 0.0, 0.06), (4.0, 4.0, 0.12))
    b.box((0, 0, 0.03), (4.0, 4.0, 0.06), "pedra_esc")
    step = 1.0
    for ix in range(4):
        for iy in range(4):
            x = -1.5 + ix * step
            y = -1.5 + iy * step
            jog = ((ix * 7 + iy * 13) % 5) * 0.006
            b.box((x, y, 0.07 + jog / 2), (step - 0.09, step - 0.09, 0.08 + jog),
                  "pedra" if (ix + iy) % 3 else "pedra_esc")
    return b.finish("SM_Kit_piso", bevel=0.018)


def tree(b, trunk_h, trunk_r, lobes, leaf):
    # THE TRUNK IS THE COLLISION. Leaves are something you walk under, and a
    # convex hull round the canopy is a five-metre invisible cylinder in the
    # middle of a wood -- which is what made the forest unwalkable.
    b.hull((0.0, 0.0, trunk_h * 0.5), (trunk_r * 2.6, trunk_r * 2.6, trunk_h))
    b.cylinder((0, 0, 0.0), trunk_r * 1.5, 0.25, "casca", sides=7)
    b.cylinder((0, 0, 0.2), trunk_r, trunk_h, "casca", sides=7, top_radius=trunk_r * 0.7)
    for dx, dy, dz, r in lobes:
        b.blob((dx, dy, trunk_h + dz), (r, r * 0.95, r * 0.85), leaf, rings=3, sides=7)


@asset("SM_Kit_arvore_a", "tree", 3.2, 3.2)
def arvore_a():
    """A third narrower than it was, and standing on a longer trunk.

    Lucas, looking down at a wood: "por conta da perspectiva tambem nao consigo
    mover o personagem pelas arvores, nao da pra clicar". He is right, and it
    is not a bug you can find in a log. The camera looks down at sixty degrees
    from thirty metres; a canopy four and a half metres across covers about
    twelve square metres of the ground behind it, so a wood becomes a green
    ceiling over the exact ground you are trying to click on. The collision was
    always just the trunk -- what was in the way was the PICTURE.

    Narrower canopies on taller trunks give a wood you see through at this
    angle and still read as a wood from the side. Cheaper, too.
    """
    b = Build()
    tree(b, 3.9, 0.22, [(0, 0, 0.7, 1.45), (0.6, -0.35, 0.15, 0.92),
                        (-0.5, 0.55, 0.28, 0.85)], "folha")
    return b.finish("SM_Kit_arvore_a", bevel=0.0)


@asset("SM_Kit_arvore_b", "tree", 3.8, 3.8)
def arvore_b():
    b = Build()
    tree(b, 6.0, 0.28, [(0, 0, 0.95, 1.75), (0.8, 0.4, 0.05, 1.05),
                        (-0.7, -0.6, 0.35, 1.10)], "folha2")
    return b.finish("SM_Kit_arvore_b", bevel=0.0)


@asset("SM_Kit_arvore_c", "tree", 2.6, 2.6)
def arvore_c():
    b = Build()
    b.hull((0.0, 0.0, 1.2), (0.60, 0.60, 2.4))
    b.cylinder((0, 0, 0.0), 0.32, 0.22, "casca", sides=7)
    b.cylinder((0, 0, 0.18), 0.21, 2.3, "casca", sides=7, top_radius=0.14)
    for z, r in ((2.4, 1.25), (3.2, 0.95), (3.9, 0.60)):
        b.blob((0, 0, z), (r, r, r * 0.55), "folha", rings=2, sides=8)
    return b.finish("SM_Kit_arvore_c", bevel=0.0)


@asset("SM_Kit_grama", "grass", 0.5, 0.5)
def grama():
    """A tuft of grass, knee high, for the Landscape to scatter by the million.

    Our own, because the pack's is a waist-high clump of broad leaves and a
    kilometre of it came out as a field of spinach you wade through. Five
    blades, none over thirty-five centimetres, leaning different ways.

    No collision box at all beyond a token one: grass you have to path around
    is not grass.
    """
    b = Build()
    b.hull((0.0, 0.0, 0.05), (0.12, 0.12, 0.10))
    for blade in range(5):
        a = 2 * math.pi * blade / 5 + 0.7
        lean = 0.055 + (blade % 3) * 0.022
        tall = 0.20 + ((blade * 7) % 4) * 0.045
        root = (0.055 * math.cos(a), 0.055 * math.sin(a), 0.0)
        tip = (root[0] + lean * math.cos(a), root[1] + lean * math.sin(a), tall)
        b.add([(root[0] - 0.026 * math.sin(a), root[1] + 0.026 * math.cos(a), 0.0),
               (root[0] + 0.026 * math.sin(a), root[1] - 0.026 * math.cos(a), 0.0),
               (tip[0] + 0.006 * math.sin(a), tip[1] - 0.006 * math.cos(a), tip[2]),
               (tip[0] - 0.006 * math.sin(a), tip[1] + 0.006 * math.cos(a), tip[2])],
              [(0, 1, 2, 3), (3, 2, 1, 0)],
              "folha" if blade % 2 else "folha2")
    return b.finish("SM_Kit_grama", bevel=0.0)


@asset("SM_Kit_arbusto", "bush", 1.8, 1.8)
def arbusto():
    b = Build()
    # Knee high and narrow: a bush you have to path around is a bush in the way.
    b.hull((0.0, 0.0, 0.22), (0.70, 0.64, 0.44))
    b.blob((0, 0, 0.52), (0.88, 0.80, 0.56), "folha2", rings=2, sides=7)
    b.blob((0.42, 0.30, 0.34), (0.50, 0.46, 0.34), "folha", rings=2, sides=6)
    return b.finish("SM_Kit_arbusto", bevel=0.0)


@asset("SM_Kit_pedra_a", "rock", 2.4, 2.0)
def pedra_a():
    b = Build()
    b.blob((0, 0, 0.62), (1.15, 0.95, 0.70), "pedra", rings=3, sides=7)
    return b.finish("SM_Kit_pedra_a", bevel=0.05)


@asset("SM_Kit_pedra_b", "rock", 1.3, 1.1)
def pedra_b():
    b = Build()
    b.blob((0, 0, 0.34), (0.62, 0.52, 0.40), "pedra_esc", rings=2, sides=6)
    b.blob((0.30, -0.22, 0.18), (0.30, 0.26, 0.22), "pedra", rings=2, sides=6)
    return b.finish("SM_Kit_pedra_b", bevel=0.04)


@asset("SM_Kit_pedra_c", "rock", 3.6, 2.8)
def pedra_c():
    b = Build()
    b.blob((0, 0, 1.05), (1.70, 1.30, 1.15), "pedra", rings=3, sides=8)
    b.blob((-1.0, 0.6, 0.38), (0.72, 0.62, 0.45), "pedra_esc", rings=2, sides=6)
    return b.finish("SM_Kit_pedra_c", bevel=0.06)


@asset("SM_Kit_toco", "stump", 1.2, 1.2)
def toco():
    b = Build()
    b.cylinder((0, 0, 0.0), 0.55, 0.62, "casca", sides=9, top_radius=0.48)
    b.cylinder((0, 0, 0.60), 0.44, 0.06, "tabua", sides=9)
    return b.finish("SM_Kit_toco", bevel=0.03)


@asset("SM_Kit_barril", "prop", 0.8, 0.8)
def barril():
    b = Build()
    b.cylinder((0, 0, 0.0), 0.31, 0.88, "tabua", sides=10)
    for z in (0.14, 0.72):
        b.cylinder((0, 0, z), 0.335, 0.07, "metal", sides=10)
    b.cylinder((0, 0, 0.86), 0.28, 0.04, "madeira", sides=10)
    return b.finish("SM_Kit_barril", bevel=0.02)


@asset("SM_Kit_caixa", "prop", 0.9, 0.9)
def caixa():
    b = Build()
    b.box((0, 0, 0.42), (0.84, 0.84, 0.84), "tabua")
    for sy in (-1, 1):
        b.box((0, sy * 0.43, 0.42), (0.90, 0.04, 0.12), "madeira")
    b.box((0, 0, 0.845), (0.90, 0.90, 0.05), "madeira")
    return b.finish("SM_Kit_caixa")


@asset("SM_Kit_mochila", "prop", 0.52, 0.48)
def mochila():
    """The courier's pack: the one object on the map a player is SENT to find.

    Every other piece in the kit is scenery and is allowed to blend in. This
    one is not. It sits on the floor of the last room of a dungeon, under a
    camera thirty metres up, and it has to be findable at a glance -- so it
    gets the one emissive material in the kit that is not fire: a sliver of
    "runa" round the buckle. Not a glow that pretends to be magic, just enough
    to survive the room's ambient light and read as "that one".

    Standing upright rather than slumped, for the same reason: a silhouette
    with a vertical edge is picked out of a floor of horizontal slabs, and a
    bag lying flat is a stain.
    """
    b = Build()
    # Thin front-to-back and wide across: that is the whole silhouette. The
    # first attempt was 34 by 42 by 42 -- near enough a cube -- and it read as
    # a cardboard box with a brick on it.
    b.box((0.00, 0.00, 0.24), (0.28, 0.40, 0.48), "couro")
    for sy in (-1, 1):                                       # shoulder straps
        b.box((-0.155, sy * 0.115, 0.30), (0.05, 0.07, 0.34), "couro")
        b.box((-0.145, sy * 0.115, 0.07), (0.07, 0.07, 0.05), "couro")
    # The flap, over the top and down the front.
    b.box((0.00, 0.00, 0.495), (0.31, 0.42, 0.05), "tabua")
    b.box((0.155, 0.00, 0.40), (0.05, 0.40, 0.22), "tabua")
    for sy in (-1, 1):
        b.box((0.172, sy * 0.12, 0.40), (0.04, 0.06, 0.26), "couro")
        b.box((0.182, sy * 0.12, 0.30), (0.035, 0.085, 0.07), "metal")
    # The catch, on the TOP of the flap rather than the front of it, because
    # the camera in this game looks down: a detail on a vertical face is a
    # detail nobody will ever see.
    b.box((0.05, 0.00, 0.525), (0.11, 0.10, 0.03), "metal")
    b.box((0.05, 0.00, 0.548), (0.09, 0.08, 0.025), "runa")
    # A bedroll lashed across the top, lying along Y -- cylinder() only stands
    # things up, and a bedroll on its end is a chimney.
    radius, half, sides = 0.075, 0.21, 9
    near, far = [], []
    for i in range(sides):
        a = 2 * math.pi * i / sides
        near.append((-0.06 + radius * math.cos(a), -half, 0.60 + radius * math.sin(a)))
        far.append((-0.06 + radius * math.cos(a), half, 0.60 + radius * math.sin(a)))
    ring = [tuple(range(sides)), tuple(range(2 * sides - 1, sides - 1, -1))]
    for i in range(sides):
        ring.append((i, sides + i, sides + (i + 1) % sides, (i + 1) % sides))
    b.add(near + far, ring, "pano")
    for sy in (-1, 1):
        b.box((-0.06, sy * 0.13, 0.60), (0.19, 0.03, 0.19), "couro")
    b.cylinder((-0.16, 0.15, 0.04), 0.055, 0.09, "metal", sides=8)
    b.hull((0.0, 0.0, 0.34), (0.36, 0.46, 0.68))
    return b.finish("SM_Kit_mochila", bevel=0.02)


# ══════════════════════════════════════════════════════════════════════════
#  The animals
# ══════════════════════════════════════════════════════════════════════════
# Three assets, and the only three in the kit that are meant to MOVE. They are
# static meshes driven by AHBeast.cpp, not skeletons, and everything about the
# way they are modelled follows from that.
#
# The camera in this game sits thirty metres up and looks down at about fifty
# degrees, so what it sees of an animal is its plan view and its silhouette: the
# footprint, the neck line, the tail. Legs are four sticks that will never bend,
# so they are placed apart -- mid-stride rather than standing to attention --
# because a frozen walking pose slides convincingly and a frozen standing pose
# does not.
#
# Collision is declared and then never used: AHBeast turns it off entirely, and
# the box is here so the offline raster can still ask whether an animal was
# placed inside a wall.


@asset("SM_Kit_galinha", "beast", 0.34, 0.46)
def galinha():
    """A hen, pecking about a yard. Thirty-four centimetres of geometry.

    The two details that make it a hen rather than a pebble on legs are the comb
    and the tail: a red notch at one end and a raised fan at the other give it a
    front and a back, which is the whole of what the eye needs to see a bird.
    """
    b = Build()
    # Body: an egg lying along Y is wrong -- a hen is deeper than it is wide and
    # leans forward, so the blob is squashed across X and pitched by placing the
    # breast lower than the tail.
    b.blob((0.00, 0.02, 0.19), (0.105, 0.155, 0.115), "penugem", rings=3, sides=7)
    # Neck and head, forward and up. A hen's head is nearly over its feet.
    b.blob((0.00, -0.15, 0.245), (0.062, 0.062, 0.070), "penugem", rings=2, sides=6)
    b.blob((0.00, -0.215, 0.275), (0.048, 0.055, 0.050), "penugem", rings=2, sides=6)
    b.box((0.00, -0.265, 0.272), (0.030, 0.055, 0.028), "bico")
    # The comb, and the wattle under the beak.
    b.box((0.00, -0.225, 0.318), (0.022, 0.070, 0.035), "pano")
    b.box((0.00, -0.250, 0.245), (0.020, 0.030, 0.035), "pano")
    # The tail: a fan tipped back over the rump.
    b.box((0.00, 0.175, 0.255), (0.055, 0.075, 0.105), "penugem", yaw=0.0)
    b.box((0.00, 0.205, 0.300), (0.040, 0.090, 0.070), "pena")
    # Wings, folded along the sides.
    for sx in (-1, 1):
        b.box((sx * 0.092, 0.015, 0.195), (0.030, 0.185, 0.090), "penugem")
    # Legs, mid-stride: one forward, one back.
    for sx, sy in ((-1, -0.045), (1, 0.055)):
        b.box((sx * 0.045, sy, 0.055), (0.022, 0.022, 0.110), "bico")
        b.box((sx * 0.045, sy - 0.020, 0.008), (0.026, 0.075, 0.016), "bico")
    b.hull((0.0, 0.0, 0.17), (0.30, 0.42, 0.34))
    return b.finish("SM_Kit_galinha", bevel=0.008)


@asset("SM_Kit_veado", "beast", 0.66, 1.70)
def veado():
    """A deer grazing a pasture, which bolts when you come within ten metres.

    THE FIRST VERSION WAS JUDGED FROM ABOVE AND FAILED, which is the only angle
    that matters: this camera sits thirty metres up at sixty-odd degrees, and in
    plan view that deer was a brown lump on two visible sticks. Three things
    were wrong and all three were "modelled for a side view":

    - the neck went UP, so the head sat on top of the body and the silhouette
      had no front. It is nearly horizontal now, and the head projects a clear
      arm's length in front of the shoulders -- which is what makes the plan
      view read as an animal facing somewhere;
    - the antlers were small and vertical, so they projected to nothing. They
      sweep BACK and OUT now, wide enough to draw a V behind the head, which is
      the one shape that says "deer" and not "dog";
    - the legs were at 13 cm either side of the spine, so from above two of the
      four hid behind the other two. Wider, and the pairs are offset front to
      back, so all four read.
    """
    b = Build()
    # Barrel, along Y. Longer than the first attempt and with the chest in
    # front of the shoulders rather than blended into them.
    b.blob((0.00, 0.02, 0.70), (0.190, 0.460, 0.220), "pelo", rings=3, sides=7)
    b.blob((0.00, -0.340, 0.735), (0.170, 0.190, 0.195), "pelo", rings=3, sides=6)
    b.blob((0.00, 0.380, 0.695), (0.150, 0.190, 0.180), "pelo", rings=3, sides=6)
    # Neck: forward, barely rising. This is the change that gives it a front.
    b.box((0.00, -0.520, 0.815), (0.150, 0.260, 0.165), "pelo")
    b.box((0.00, -0.690, 0.860), (0.120, 0.200, 0.135), "pelo")
    # Head and muzzle, a clear 25 cm in front of the neck.
    b.blob((0.00, -0.810, 0.880), (0.085, 0.120, 0.085), "pelo", rings=2, sides=6)
    b.box((0.00, -0.925, 0.855), (0.065, 0.135, 0.072), "pelo")
    b.box((0.00, -0.995, 0.848), (0.050, 0.032, 0.048), "pena")      # the nose
    for sx in (-1, 1):                                               # ears, out
        b.box((sx * 0.105, -0.775, 0.935), (0.075, 0.070, 0.060), "pelo")
    # Antlers: one beam per side, swept back and out on a diagonal, with two
    # short tines standing off it.
    #
    # The first version was three boxes at right angles and from directly above
    # it drew a GRID -- two little scaffolds over the head, which is not a shape
    # any animal has. A single raked beam is fewer boxes and unmistakable,
    # because what says "antler" is the diagonal, not the count of points.
    for sx in (-1, 1):
        b.box((sx * 0.060, -0.790, 0.985), (0.032, 0.032, 0.125), "bico")
        b.box((sx * 0.165, -0.610, 1.060), (0.036, 0.430, 0.032), "bico",
              yaw=sx * math.radians(26.0))
        b.box((sx * 0.215, -0.680, 1.120), (0.028, 0.030, 0.095), "bico")
        b.box((sx * 0.262, -0.470, 1.125), (0.028, 0.030, 0.105), "bico")
    # Tail: a white flash on the rump, which is what you see of a deer running.
    b.box((0.00, 0.560, 0.760), (0.075, 0.095, 0.130), "penugem")
    # Four legs, wide and offset front to back so all four read from above.
    for sx, sy in ((-1, -0.330), (1, -0.250), (-1, 0.250), (1, 0.340)):
        b.box((sx * 0.165, sy, 0.320), (0.058, 0.064, 0.640), "pelo")
        b.box((sx * 0.165, sy, 0.030), (0.052, 0.078, 0.060), "bico")   # hoof
    b.hull((0.0, 0.0, 0.62), (0.56, 1.55, 1.24))
    return b.finish("SM_Kit_veado", bevel=0.010)


@asset("SM_Kit_corvo", "beast", 0.92, 0.46)
def corvo():
    """A crow, in the air, going round in a circle nine to seventeen metres up.

    Modelled with its wings SPREAD, and that is the whole asset: it never lands,
    so it is only ever seen gliding, and a bird seen from underneath is two
    triangles and a tail. Ninety centimetres across, which is the wingspan of a
    real crow and about two pixels of detail at this camera height -- so the
    shape has to carry it and nothing else can.

    The wings are swept BACK as well as out, because a straight cross reads as
    an aeroplane and the swept version reads as a bird from any angle.
    """
    b = Build()
    b.blob((0.00, 0.00, 0.10), (0.058, 0.150, 0.055), "pena", rings=2, sides=6)
    b.blob((0.00, -0.165, 0.112), (0.040, 0.055, 0.040), "pena", rings=2, sides=5)
    b.box((0.00, -0.225, 0.108), (0.024, 0.075, 0.022), "bico")
    # Wings: inner panel out and slightly back, outer panel more of both, and
    # each one tilted a little so the pair makes a shallow V.
    for sx in (-1, 1):
        b.box((sx * 0.145, -0.010, 0.106), (0.230, 0.135, 0.018), "pena")
        b.box((sx * 0.345, 0.045, 0.118), (0.190, 0.100, 0.016), "pena")
        b.box((sx * 0.440, 0.085, 0.126), (0.060, 0.070, 0.014), "pena")
    # Tail: a wedge, spread.
    b.box((0.00, 0.200, 0.104), (0.105, 0.150, 0.016), "pena")
    # Feet tucked up under the body, which is what a gliding bird does with them.
    for sx in (-1, 1):
        b.box((sx * 0.032, 0.060, 0.062), (0.016, 0.060, 0.016), "bico")
    b.hull((0.0, 0.0, 0.10), (0.88, 0.42, 0.20))
    return b.finish("SM_Kit_corvo", bevel=0.006)


@asset("SM_Kit_fardo", "prop", 1.5, 1.1)
def fardo():
    """A round bale lying on its side -- built along Y, since cylinder() only
    stands things up and a bale standing on end is a water tank."""
    b = Build()
    radius, half, sides = 0.55, 0.52, 11
    near, far = [], []
    for i in range(sides):
        a = 2 * math.pi * i / sides
        near.append((radius * math.cos(a), -half, radius + radius * math.sin(a)))
        far.append((radius * math.cos(a), half, radius + radius * math.sin(a)))
    faces = [tuple(range(sides)), tuple(range(2 * sides - 1, sides - 1, -1))]
    for i in range(sides):
        j = (i + 1) % sides
        faces.append((i, sides + i, sides + j, j))
    b.add(near + far, faces, "colmo")
    for at in (-0.20, 0.20):                                        # binding twine
        b.box((0, at, radius), (radius * 2.04, 0.07, radius * 2.04), "colmo2")
    return b.finish("SM_Kit_fardo", bevel=0.04)


@asset("SM_Kit_fogueira", "fire", 1.8, 1.8)
def fogueira():
    b = Build()
    for i in range(9):
        a = 2 * math.pi * i / 9
        b.box((0.72 * math.cos(a), 0.72 * math.sin(a), 0.11),
              (0.46, 0.26, 0.22), "pedra", yaw=a)
    b.box((0, 0, 0.05), (1.10, 1.10, 0.10), "pedra_esc")
    for i, a in enumerate((0.3, 1.4, 2.6, 3.9, 5.1)):
        b.box((0.16 * math.cos(a), 0.16 * math.sin(a), 0.28),
              (1.05, 0.13, 0.13), "casca", yaw=a)
    b.blob((0, 0, 0.30), (0.34, 0.34, 0.26), "brasa", rings=2, sides=6)
    return b.finish("SM_Kit_fogueira", bevel=0.02)


@asset("SM_Kit_carroca", "cart", 2.8, 1.7)
def carroca():
    b = Build()
    b.box((0, 0, 0.78), (2.30, 1.30, 0.16), "tabua")
    for sy in (-1, 1):
        b.box((0, sy * 0.66, 1.02), (2.30, 0.10, 0.62), "tabua")
    b.box((-1.13, 0, 1.02), (0.10, 1.30, 0.62), "tabua")
    for sy in (-1, 1):
        b.cylinder((0.42, sy * 0.72, 0.70), 0.62, 0.14, "casca", sides=12)
        b.cylinder((0.42, sy * 0.72, 0.70), 0.18, 0.16, "madeira", sides=8)
    b.box((1.55, 0, 0.62), (1.30, 0.12, 0.12), "madeira")
    b.box((-1.30, 0, 0.30), (0.14, 0.14, 0.60), "madeira")
    return b.finish("SM_Kit_carroca", bevel=0.03)


@asset("SM_Kit_bandeira", "banner", 0.6, 1.6)
def bandeira():
    """A banner that hangs off a wall. It has no mast, because a mast in the
    square is a mast in somebody's way -- and that was the complaint.

    The cloth is modelled with a slight fall away from the wall and a swallow
    tail at the bottom, so it reads as hanging rather than as a painted stripe.
    """
    b = Build()
    # Nothing at head height and nothing at the feet: cloth hanging on a wall
    # is not an obstacle, and making it one is how a square stops working.
    b.hull((0.0, 0.0, 3.06), (0.36, 1.60, 0.26))
    b.box((-0.12, 0, 3.10), (0.24, 1.60, 0.18), "madeira")          # wall bracket
    b.box((0.10, 0, 3.12), (0.34, 0.16, 0.16), "madeira")
    for step in range(6):
        t = step / 6.0
        b.box((0.16 + t * 0.10, 0, 3.00 - 0.42 - step * 0.42),
              (0.06, 1.34 - t * 0.06, 0.44), "pano")
    b.box((0.30, 0, 1.55), (0.04, 0.44, 2.30), "pano2")             # blazon stripe
    for sy in (-1, 1):                                              # swallow tail
        b.box((0.26, sy * 0.44, 0.34), (0.05, 0.44, 0.46), "pano")
    return b.finish("SM_Kit_bandeira", bevel=0.01)


# ══════════════════════════════════════════════════════════════════════════
def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    MATERIALS.clear()


def collision_box(name, index, centre, size, yaw=0.0):
    """One UCX box. Unreal matches it to the mesh by name and nothing else."""
    cx, cy, cz = centre
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    corners = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
               (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
    cos, sin = math.cos(yaw), math.sin(yaw)
    points = [(cx + x * cos - y * sin, cy + x * sin + y * cos, cz + z)
              for x, y, z in corners]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    mesh = bpy.data.meshes.new("UCX_%s_%02d" % (name, index))
    mesh.from_pydata(points, [], faces)
    mesh.validate(verbose=False)
    obj = bpy.data.objects.new("UCX_%s_%02d" % (name, index), mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def build_all():
    made = []
    for name, kind, foot_x, foot_y, maker in CATALOGUE:
        obj = maker()
        obj.name = name
        low = min((obj.matrix_world @ Vector(c))[2] for c in obj.bound_box)
        high = max((obj.matrix_world @ Vector(c))[2] for c in obj.bound_box)
        # Collision. Declared hulls win; otherwise a box of the declared
        # footprint, which is what the piece was modelled inside.
        hulls = HULLS.get(name) or [((0.0, 0.0, (high - low) / 2),
                                     (foot_x, foot_y, max(high - low, 0.05)), 0.0)]
        shapes = []
        for index, (centre, size, yaw) in enumerate(hulls):
            shapes.append(collision_box(name, index, centre, size, yaw))

        made.append({
            "name": name, "kind": kind,
            "footprint_x_m": foot_x, "footprint_y_m": foot_y,
            "height_m": round(high - low, 3),
            "door_yaw_deg": 0 if kind in ("house", "barn", "tower", "stall") else None,
            "faces": len(obj.data.polygons),
            "collision": [{"centre": [round(v, 3) for v in c],
                           "size": [round(v, 3) for v in z],
                           "yaw_deg": round(math.degrees(y), 1)}
                          for c, z, y in hulls],
            "ucx": [o.name for o in shapes],
        })
    return made


if __name__ == "__main__":
    clear()
    catalogue = build_all()
    total = sum(entry["faces"] for entry in catalogue)
    with open(os.path.join(OUT, "kit.json"), "w", encoding="utf-8") as out:
        json.dump({"unit": "metre", "pivot": "base centre",
                   "door_axis": "+X", "pieces": catalogue}, out,
                  indent=2, ensure_ascii=False)
    print(json.dumps({"pecas": len(catalogue), "faces_total": total,
                      "maior": max(catalogue, key=lambda e: e["faces"])["name"]},
                     ensure_ascii=False))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "kit.blend"))
