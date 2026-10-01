"""Low-poly characters in the kit's language, built to Mannequin proportions.

WHY THESE MEASUREMENTS AND NOT ANY OTHERS
------------------------------------------
Every animation in Ashen Hollow -- all thirteen of them -- is authored on Epic's
`SKM_Manny_Simple` skeleton. So a character is not free to be any shape it
likes: it has to be skinnable to that skeleton, which means its joints have to
sit where that skeleton's joints sit. The numbers below are the Mannequin's,
in centimetres from the floor:

    tornozelo  10    joelho   47    quadril  92    cintura 104
    peito     134    ombro   148    pescoco 152    topo    180

This file is the LOOK, proposed before any rigging work is done, for the same
reason the building kit was rendered before it was wired into the game: three
scenery passes were rejected after the fact, and a picture costs one message.
What it does not do yet is skin to the skeleton -- that step needs the real
`SKM_Manny_Simple` exported out of the project, and it is worth doing only once
the shapes are agreed.

WHAT THIS TECHNIQUE IS GOOD AT, AND WHERE IT STOPS
---------------------------------------------------
Faceted armour, hard plates, strong silhouettes, non-human heads: a script is
excellent at all of these, because they are flat planes meeting at angles.
A bare human face is the opposite -- brow, nose bridge, cheekbone and lip are
smooth transitions that somebody sculpts. So the human here wears a helmet, the
elf's face is kept small and simple under a heavy hair mass, and the orc leans
into the jaw and tusks. That is not a workaround, it is choosing subjects the
technique is good at.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kit as kit                                            # noqa: E402

kit.PALETTE.update({
    "pele":      ((0.808, 0.596, 0.463), 0.72),
    "pele_orc":  ((0.404, 0.588, 0.298), 0.74),
    "pele_orc2": ((0.322, 0.478, 0.239), 0.74),
    "cabelo":    ((0.114, 0.094, 0.090), 0.82),
    "couro":     ((0.259, 0.169, 0.110), 0.78),
    "couro2":    ((0.400, 0.278, 0.176), 0.80),
    "aco":       ((0.502, 0.518, 0.545), 0.38),
    "aco_esc":   ((0.298, 0.314, 0.341), 0.44),
    "presa":     ((0.902, 0.878, 0.792), 0.48),
    "olho":      ((0.071, 0.078, 0.086), 0.20),
    "olho_orc":  ((0.878, 0.400, 0.110), 0.25),
    "tecido":    ((0.310, 0.216, 0.235), 0.90),
})

# The Mannequin's joints, in metres from the floor.
ANKLE, KNEE, HIP, WAIST, CHEST, SHOULDER, NECK, TOP = (
    0.10, 0.47, 0.92, 1.02, 1.40, 1.46, 1.52, 1.78)
CATALOGUE = []


def asset(name):
    def wrap(maker):
        CATALOGUE.append((name, maker))
        return maker
    return wrap


# ── The parts every body shares ────────────────────────────────────────────
def tube(b, start, end, wide_at_start, wide_at_end, name, sides=6, roll=0.0):
    """A tapered prism between two points -- a limb.

    The first attempt built arms and legs as STACKS of little boxes, and the
    result looked like a string of beads: every box had its own bevel, so every
    segment had its own highlight. A limb is one solid that narrows, and six
    sides is enough to read as round while staying faceted.
    """
    start, end = Vector(start), Vector(end)
    axis = end - start
    if axis.length < 1e-6:
        return
    axis.normalize()
    guide = Vector((0.0, 0.0, 1.0)) if abs(axis.z) < 0.9 else Vector((1.0, 0.0, 0.0))
    across = axis.cross(guide)
    across.normalize()
    other = axis.cross(across)
    other.normalize()

    lower, upper = [], []
    for i in range(sides):
        angle = 2 * math.pi * i / sides + roll
        offset = across * math.cos(angle) + other * math.sin(angle)
        lower.append(tuple(start + offset * wide_at_start))
        upper.append(tuple(end + offset * wide_at_end))
    faces = [tuple(range(sides - 1, -1, -1)), tuple(range(sides, 2 * sides))]
    for i in range(sides):
        j = (i + 1) % sides
        faces.append((i, j, sides + j, sides + i))
    b.add(lower + upper, faces, name)


def legs(b, skin, boot, plate, hip_w=0.105, thigh=0.098, calf=0.072):
    for side in (-1, 1):
        x = side * hip_w
        tube(b, (x, 0, HIP + 0.02), (x * 1.08, 0, KNEE), thigh, thigh * 0.74, skin)
        tube(b, (x * 1.08, 0, KNEE), (x * 1.12, 0, ANKLE + 0.02),
             calf * 1.02, calf * 0.80, boot)
        b.box((x * 1.12, 0.045, 0.036), (calf * 2.2, 0.245, 0.072), boot)   # sole
        b.box((x * 1.12, 0.060, 0.100), (calf * 2.1, 0.195, 0.095), boot)   # upper
        b.box((x * 1.12, 0, ANKLE + 0.09), (calf * 2.4, calf * 2.3, 0.075), plate)
        # Knee plate: a wedge, the shape that says "armour" fastest.
        b.add([(x - 0.085, -0.085, KNEE - 0.075), (x + 0.085, -0.085, KNEE - 0.075),
               (x + 0.085, -0.085, KNEE + 0.085), (x - 0.085, -0.085, KNEE + 0.085),
               (x - 0.062, -0.150, KNEE + 0.012), (x + 0.062, -0.150, KNEE + 0.012)],
              [(0, 3, 2, 1), (0, 1, 4), (2, 3, 5), (1, 2, 5, 4), (3, 0, 4, 5)], plate)


def torso(b, body, belt, chest_w=0.168, waist_w=0.128):
    """Hips, waist and ribcage as one continuous taper, and the shoulders as a
    yoke on top of it. The gap between chest and neck in the first attempt is
    what made the head look like it was floating."""
    b.add([(-waist_w * 1.06, -0.088, HIP - 0.02), (waist_w * 1.06, -0.088, HIP - 0.02),
           (waist_w * 1.06, 0.088, HIP - 0.02), (-waist_w * 1.06, 0.088, HIP - 0.02),
           (-waist_w, -0.080, WAIST), (waist_w, -0.080, WAIST),
           (waist_w, 0.080, WAIST), (-waist_w, 0.080, WAIST)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], body)
    b.add([(-waist_w, -0.080, WAIST), (waist_w, -0.080, WAIST),
           (waist_w, 0.080, WAIST), (-waist_w, 0.080, WAIST),
           (-chest_w, -0.098, CHEST), (chest_w, -0.098, CHEST),
           (chest_w, 0.098, CHEST), (-chest_w, 0.098, CHEST)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], body)
    # The yoke: shoulders are wider than the ribcage and sit ON it.
    b.add([(-chest_w, -0.098, CHEST), (chest_w, -0.098, CHEST),
           (chest_w, 0.098, CHEST), (-chest_w, 0.098, CHEST),
           (-chest_w * 1.10, -0.078, SHOULDER), (chest_w * 1.10, -0.078, SHOULDER),
           (chest_w * 1.10, 0.078, SHOULDER), (-chest_w * 1.10, 0.078, SHOULDER)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], body)
    b.box((0, 0, WAIST - 0.025), (waist_w * 2.10, 0.180, 0.080), belt)


def tassets(b, plate, trim, drop=0.34):
    """The pointed skirt plates -- the most recognisable silhouette in the
    reference sheets, and four solids."""
    top = WAIST - 0.075
    for out in (-0.098, 0.098):
        b.add([(-0.128, out - 0.013, top), (0.128, out - 0.013, top),
               (0.128, out + 0.013, top), (-0.128, out + 0.013, top),
               (-0.072, out, top - drop), (0.072, out, top - drop)],
              [(0, 3, 2, 1), (0, 1, 4), (2, 3, 5), (1, 2, 5, 4), (3, 0, 4, 5)], plate)
        b.box((0, out, top - 0.048), (0.272, 0.042, 0.050), trim)
    for side in (-1, 1):
        b.box((side * 0.168, 0, top - 0.10), (0.068, 0.176, 0.215), plate)


def arms(b, skin, sleeve, bracer, out=0.185, lean=0.055):
    """An A-pose built from three tapered solids per arm, not from beads."""
    for side in (-1, 1):
        shoulder = (side * out, 0.0, SHOULDER - 0.035)
        elbow = (side * (out + lean), 0.0, CHEST - 0.235)
        wrist = (side * (out + lean * 1.9), 0.0, WAIST - 0.135)
        tube(b, shoulder, elbow, 0.058, 0.044, sleeve)
        tube(b, elbow, wrist, 0.046, 0.036, skin)
        tube(b, ((elbow[0] + wrist[0]) / 2, 0, (elbow[2] + wrist[2]) / 2 + 0.02),
             (wrist[0], 0, wrist[2] + 0.015), 0.054, 0.046, bracer)
        b.box((wrist[0], 0.008, wrist[2] - 0.075), (0.070, 0.092, 0.135), skin)


def pauldrons(b, plate, trim):
    for side in (-1, 1):
        sx = side * 0.196
        b.add([(sx - 0.092, -0.098, SHOULDER - 0.085), (sx + 0.092, -0.098, SHOULDER - 0.085),
               (sx + 0.092, 0.098, SHOULDER - 0.085), (sx - 0.092, 0.098, SHOULDER - 0.085),
               (sx - 0.048, -0.046, SHOULDER + 0.052), (sx + 0.048, -0.046, SHOULDER + 0.052),
               (sx + 0.048, 0.046, SHOULDER + 0.052), (sx - 0.048, 0.046, SHOULDER + 0.052)],
              [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
               (2, 3, 7, 6), (3, 0, 4, 7)], plate)
        b.box((sx + side * 0.048, 0, SHOULDER - 0.115), (0.062, 0.168, 0.062), trim)


def head_base(b, skin, width=0.076, depth=0.086):
    """A head is a quarter of a metre tall on a 1.8 m figure, not a third.

    The first attempt gave it 30 cm and a long bare neck, and the result read
    as a mascot. Cranium, brow and jaw, tightly stacked, over a neck that is
    barely visible: that is human proportion.
    """
    mid = 1.655
    tube(b, (0, 0, SHOULDER - 0.01), (0, 0, NECK + 0.035), 0.050, 0.044, skin, sides=6)
    b.box((0, -0.004, mid + 0.058), (width * 2, depth * 2, 0.106), skin)      # cranium
    b.box((0, -0.014, mid - 0.006), (width * 1.90, depth * 1.94, 0.066), skin)  # brow
    b.box((0, -0.006, mid - 0.066), (width * 1.62, depth * 1.74, 0.062), skin)  # jaw
    return mid


def eyes(b, mid, colour, depth=0.086, spread=0.036, size=0.021):
    for side in (-1, 1):
        b.box((side * spread, -depth * 0.98, mid - 0.002), (size, 0.014, size * 0.58), colour)


# ══════════════════════════════════════════════════════════════════════════
@asset("SM_Char_guerreiro")
def guerreiro():
    """Human fighter. Helmeted on purpose: a bare human face is the one thing
    this technique cannot do, and a closed helm is the oldest answer in the
    business."""
    b = kit.Build()
    legs(b, "couro", "couro", "aco")
    torso(b, "couro", "couro2")
    # Breastplate: a chevron, which is what makes a torso read as armoured.
    b.add([(-0.178, -0.108, CHEST - 0.30), (0.178, -0.108, CHEST - 0.30),
           (0.178, -0.108, CHEST + 0.005), (-0.178, -0.108, CHEST + 0.005),
           (-0.132, -0.185, CHEST - 0.22), (0.132, -0.185, CHEST - 0.22),
           (0.132, -0.165, CHEST - 0.01), (-0.132, -0.165, CHEST - 0.01)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], "aco")
    b.box((0, -0.150, CHEST - 0.155), (0.055, 0.075, 0.24), "aco_esc")
    b.box((0, 0.098, CHEST - 0.12), (0.330, 0.055, 0.30), "aco")
    tassets(b, "aco", "aco_esc")
    arms(b, "pele", "couro", "aco")
    pauldrons(b, "aco", "couro2")

    mid = head_base(b, "pele")
    # Helm: skull, nasal bar, cheek guards, crest.
    b.box((0, -0.010, mid + 0.062), (0.196, 0.212, 0.150), "aco")
    b.box((0, -0.010, mid + 0.150), (0.055, 0.215, 0.055), "aco_esc")     # crest
    b.box((0, -0.112, mid - 0.018), (0.034, 0.055, 0.115), "aco_esc")     # nasal
    for side in (-1, 1):
        b.box((side * 0.092, -0.020, mid - 0.055), (0.030, 0.185, 0.130), "aco")
    eyes(b, mid, "olho")
    b.box((0, 0.105, CHEST - 0.02), (0.300, 0.060, 0.060), "tecido")
    return b.finish("SM_Char_guerreiro", bevel=0.008)


@asset("SM_Char_elfa")
def elfa():
    """Elf ranger, straight off the reference: light plate over leather, one
    heavy pauldron, a long hair mass that carries the silhouette."""
    b = kit.Build()
    legs(b, "couro", "couro", "aco", hip_w=0.098, thigh=0.088, calf=0.066)
    torso(b, "couro", "couro2", chest_w=0.150, waist_w=0.112)
    b.add([(-0.150, -0.092, CHEST - 0.28), (0.150, -0.092, CHEST - 0.28),
           (0.150, -0.092, CHEST + 0.005), (-0.150, -0.092, CHEST + 0.005),
           (-0.104, -0.152, CHEST - 0.20), (0.104, -0.152, CHEST - 0.20),
           (0.104, -0.138, CHEST - 0.015), (-0.104, -0.138, CHEST - 0.015)],
          [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
           (2, 3, 7, 6), (3, 0, 4, 7)], "aco_esc")
    b.box((0, -0.140, CHEST - 0.055), (0.062, 0.062, 0.062), "aco")       # brooch
    tassets(b, "couro", "aco", drop=0.42)
    arms(b, "pele", "pele", "aco", out=0.172)
    # One pauldron only, like the reference: asymmetry is what makes a
    # silhouette look designed rather than generated.
    for side in (-1, 1):
        sx = side * 0.196
        if side < 0:
            b.add([(sx - 0.098, -0.108, SHOULDER - 0.10), (sx + 0.098, -0.108, SHOULDER - 0.10),
                   (sx + 0.098, 0.108, SHOULDER - 0.10), (sx - 0.098, 0.108, SHOULDER - 0.10),
                   (sx - 0.052, -0.050, SHOULDER + 0.072), (sx + 0.052, -0.050, SHOULDER + 0.072),
                   (sx + 0.052, 0.050, SHOULDER + 0.072), (sx - 0.052, 0.050, SHOULDER + 0.072)],
                  [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5),
                   (2, 3, 7, 6), (3, 0, 4, 7)], "aco")
        else:
            b.box((sx, 0, SHOULDER - 0.115), (0.115, 0.180, 0.055), "couro2")

    mid = head_base(b, "pele", width=0.070, depth=0.080)
    for side in (-1, 1):                                     # ears
        b.add([(side * 0.078, 0.010, mid + 0.005), (side * 0.078, -0.030, mid + 0.010),
               (side * 0.078, -0.020, mid + 0.060), (side * 0.128, 0.030, mid + 0.085),
               (side * 0.086, 0.028, mid - 0.010)],
              [(0, 1, 2), (0, 2, 3), (0, 3, 4), (1, 4, 3, 2)], "pele")
    # Hair: a single mass down the back plus a fringe. Modelled as one lump,
    # which is exactly how the reference does it.
    b.box((0, 0.020, mid + 0.088), (0.200, 0.215, 0.115), "cabelo")
    b.box((0, 0.098, mid - 0.130), (0.175, 0.080, 0.330), "cabelo")
    for side in (-1, 1):
        b.box((side * 0.086, 0.010, mid - 0.060), (0.045, 0.185, 0.230), "cabelo")
    b.box((0, -0.088, mid + 0.088), (0.185, 0.070, 0.075), "cabelo")
    eyes(b, mid, "olho", depth=0.080, spread=0.032, size=0.019)
    return b.finish("SM_Char_elfa", bevel=0.008)


@asset("SM_Char_orc")
def orc():
    """Orc. The head is where this technique wins: a heavy jaw, a flat nose and
    two tusks are all hard planes, so the face can actually be modelled rather
    than suggested."""
    b = kit.Build()
    legs(b, "pele_orc", "couro", "aco_esc", hip_w=0.118, thigh=0.114, calf=0.084)
    torso(b, "pele_orc", "couro", chest_w=0.202, waist_w=0.152)
    b.box((0, -0.115, CHEST - 0.14), (0.330, 0.085, 0.215), "couro")
    b.box((0, -0.150, CHEST - 0.13), (0.085, 0.060, 0.085), "aco")
    tassets(b, "couro", "aco_esc", drop=0.40)
    arms(b, "pele_orc", "pele_orc", "couro", out=0.216, lean=0.072)
    for side in (-1, 1):
        b.box((side * 0.245, 0, SHOULDER - 0.10), (0.130, 0.180, 0.070), "couro2")
        b.box((side * 0.262, 0, SHOULDER - 0.34), (0.115, 0.135, 0.075), "presa")

    mid = head_base(b, "pele_orc", width=0.094, depth=0.100)
    b.box((0, -0.010, mid - 0.098), (0.232, 0.230, 0.105), "pele_orc")    # heavy jaw
    b.box((0, -0.126, mid - 0.030), (0.086, 0.060, 0.058), "pele_orc2")   # flat nose
    b.box((0, -0.012, mid + 0.040), (0.226, 0.210, 0.050), "pele_orc2")   # brow ridge
    for side in (-1, 1):                                      # tusks
        b.add([(side * 0.068, -0.120, mid - 0.142), (side * 0.100, -0.120, mid - 0.142),
               (side * 0.100, -0.082, mid - 0.142), (side * 0.068, -0.082, mid - 0.142),
               (side * 0.082, -0.101, mid - 0.020)],
              [(0, 3, 2, 1), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], "presa")
        b.add([(side * 0.128, 0.020, mid + 0.060), (side * 0.128, -0.020, mid + 0.062),
               (side * 0.128, -0.010, mid + 0.098), (side * 0.176, 0.030, mid + 0.076)],
              [(0, 1, 2), (0, 2, 3), (0, 3, 1), (1, 3, 2)], "pele_orc")   # ears
    b.box((0, 0.038, mid + 0.082), (0.238, 0.235, 0.125), "cabelo")
    b.box((0, 0.128, mid - 0.185), (0.190, 0.085, 0.430), "cabelo")
    eyes(b, mid, "olho_orc", depth=0.100, spread=0.044, size=0.024)
    return b.finish("SM_Char_orc", bevel=0.008)
