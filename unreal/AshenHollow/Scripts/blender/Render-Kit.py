"""Renders the whole kit as one sheet, so it can be judged before it is wired in.

Every version of the world that Lucas rejected was rejected by looking at it.
So nothing from this kit reaches the game before this picture has been looked
at -- by me first, and then by him.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kit as kit                                            # noqa: E402

OUT = kit.OUT

# Laid out in rows, biggest first, so the sheet reads as a catalogue and the
# scale relationships between pieces are obvious at a glance.
ROWS = [
    ["SM_Kit_casa_c", "SM_Kit_casa_g", "SM_Kit_celeiro", "SM_Kit_casa_b"],
    ["SM_Kit_casa_f", "SM_Kit_casa_e", "SM_Kit_casa_a", "SM_Kit_casa_d", "SM_Kit_casa_h"],
    ["SM_Kit_torre", "SM_Kit_torre_ruina", "SM_Kit_arvore_b", "SM_Kit_arvore_a",
     "SM_Kit_arvore_c", "SM_Kit_portao"],
    ["SM_Kit_arco", "SM_Kit_tenda", "SM_Kit_banca", "SM_Kit_poco", "SM_Kit_ponte",
     "SM_Kit_muro"],
    ["SM_Kit_palicada", "SM_Kit_cerca", "SM_Kit_carroca", "SM_Kit_altar",
     "SM_Kit_menir", "SM_Kit_placa", "SM_Kit_bandeira", "SM_Kit_lampiao"],
    ["SM_Kit_piso", "SM_Kit_pedra_c", "SM_Kit_pedra_a", "SM_Kit_arbusto",
     "SM_Kit_lapide", "SM_Kit_fogueira", "SM_Kit_toco", "SM_Kit_pedra_b",
     "SM_Kit_caixa", "SM_Kit_barril", "SM_Kit_fardo"],
]
FACE_CAMERA = {"SM_Kit_bandeira", "SM_Kit_placa", "SM_Kit_banca",
               "SM_Kit_portao", "SM_Kit_arco", "SM_Kit_tenda"}
GAP = 3.0
ROW_GAP = 7.4


def label(text, at, size=0.62):
    curve = bpy.data.curves.new(type="FONT", name="lbl")
    curve.body = text
    curve.size = size
    curve.align_x = "CENTER"
    obj = bpy.data.objects.new("lbl_" + text, curve)
    obj.location = at
    obj.rotation_euler = (math.radians(62), 0, 0)
    bpy.context.collection.objects.link(obj)
    dark = bpy.data.materials.new("M_label")
    dark.use_nodes = True
    dark.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.09, 0.08, 0.07, 1)
    obj.data.materials.append(dark)
    return obj


def person(at):
    """A 1.8 m figure beside the buildings. Scale is a thing you check by eye."""
    body = kit.Build()
    body.cylinder((0, 0, 0.0), 0.16, 0.86, "madeira", sides=8)
    body.box((0, 0, 1.18), (0.42, 0.28, 0.66), "pano")
    body.blob((0, 0, 1.64), (0.15, 0.13, 0.17), "tabua", rings=2, sides=6)
    made = body.finish("referencia_1m80", bevel=0.02)
    made.location = at
    return made


def main():
    kit.clear()
    kit.build_all()

    by_name = {o.name: o for o in bpy.context.scene.objects}
    # Anything not on the sheet has to GO. Every asset is built at the origin,
    # so a partial sheet leaves the rest of the kit in one heap in the middle
    # of the picture -- which is exactly what the first partial render showed,
    # and it read as a bug in the assets rather than a bug in the sheet.
    wanted = {name for row in ROWS for name in row}
    for name, obj in list(by_name.items()):
        # The UCX collision boxes go too -- ALL of them. They are built at the
        # origin like everything else, and a sheet that leaves them in shows a
        # white pile in the middle of the picture that reads as a broken asset.
        if name.startswith("UCX_") or (name.startswith("SM_Kit_") and name not in wanted):
            bpy.data.objects.remove(obj, do_unlink=True)
    by_name = {o.name: o for o in bpy.context.scene.objects}
    placed, y = [], 0.0
    for row in ROWS:
        spans = []
        for name in row:
            obj = by_name[name]
            xs = [(obj.matrix_world @ Vector(c))[0] for c in obj.bound_box]
            spans.append(max(xs) - min(xs))
        width = sum(spans) + GAP * (len(row) - 1)
        x = -width / 2
        tallest = 0.0
        for name, span in zip(row, spans):
            obj = by_name[name]
            # Flat things are modelled facing +X, which is edge-on to this
            # camera. Turned a quarter for the sheet only -- in the game the
            # generator decides which way they face.
            if name in FACE_CAMERA:
                obj.rotation_euler = (0, 0, math.radians(90))
            obj.location = (x + span / 2, y, 0)
            zs = [(obj.matrix_world @ Vector(c))[2] for c in obj.bound_box]
            tallest = max(tallest, max(zs))
            label(name.replace("SM_Kit_", ""), (x + span / 2, y - span / 2 - 1.15, 0.02))
            placed.append(obj)
            x += span + GAP
        y -= max(tallest * 0.55, 3.0) + ROW_GAP

    # The human goes at the END of the last row, inside the frame. The first
    # sheet put it above the top row, off camera -- a scale reference nobody
    # can see is not a scale reference.
    edge = max(o.location.x for o in placed) + 3.4
    person((edge, y + ROW_GAP + 2.2, 0))
    label("pessoa 1,80 m", (edge, y + ROW_GAP + 0.4, 0.02), 0.52)

    # Ground, sun, sky.
    bpy.ops.mesh.primitive_plane_add(size=260, location=(0, y / 2, -0.01))
    floor = bpy.context.object
    grass = bpy.data.materials.new("M_sheet_floor")
    grass.use_nodes = True
    grass.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.474, 0.443, 0.376, 1)
    grass.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    floor.data.materials.append(grass)

    bpy.ops.object.light_add(type="SUN", location=(24, 30, 42))
    sun = bpy.context.object
    sun.data.energy = 4.0
    sun.data.angle = math.radians(2.2)
    sun.rotation_euler = (math.radians(46), math.radians(6), math.radians(-34))
    sun.data.color = (1.0, 0.957, 0.886)

    world = bpy.context.scene.world or bpy.data.worlds.new("W")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.47, 0.58, 0.72, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.55

    # An orthographic camera framed from the actual contents.
    #
    # Placed by arithmetic rather than by eye, because the first two attempts
    # were placed by eye and both pointed at the sky. With the camera rotated
    # `tilt` about X, it looks along (0, sin tilt, -cos tilt) and its up vector
    # is (0, cos tilt, sin tilt) -- so standing it off the middle of the sheet
    # means walking BACK along that view direction, and the frame it needs is
    # the sheet's depth and height projected onto that up vector.
    corners = []
    for obj in placed:
        for c in obj.bound_box:
            corners.append(obj.matrix_world @ Vector(c))
    low_x, high_x = min(v.x for v in corners), max(v.x for v in corners)
    low_y, high_y = min(v.y for v in corners) - 2.0, max(v.y for v in corners)
    high_z = max(v.z for v in corners)
    mid = Vector(((low_x + high_x) / 2, (low_y + high_y) / 2, high_z * 0.30))

    tilt = math.radians(68)
    view = Vector((0.0, math.sin(tilt), -math.cos(tilt)))
    stand = mid - view * 220.0

    bpy.ops.object.camera_add(location=stand)
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.rotation_euler = (tilt, 0, 0)

    scene = bpy.context.scene
    aspect = scene.render.resolution_x / scene.render.resolution_y
    across = (high_x - low_x) + 3.0
    down = (high_y - low_y) * math.cos(tilt) + high_z * math.sin(tilt) + 3.0
    camera.data.ortho_scale = max(across, down * aspect)
    camera.data.clip_start = 1.0
    camera.data.clip_end = 600.0
    scene.camera = camera

    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1320
    scene.render.film_transparent = False
    scene.render.filepath = os.path.join(OUT, "kit_folha.png")
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    bpy.ops.render.render(write_still=True)
    print("rendered", scene.render.filepath)


if __name__ == "__main__":
    main()
