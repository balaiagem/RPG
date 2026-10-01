"""Renders the characters as turnaround sheets, three views each.

The same discipline as the building kit: nothing goes near the game until the
picture has been looked at. Laid out the way Lucas's reference sheets are --
front, side, back -- so the two can be compared side by side rather than from
memory.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kit as kit                                            # noqa: E402
import make_chars as chars                                        # noqa: E402

VIEWS = ((0, "frente"), (90, "lado"), (180, "costas"))
STEP_X, GROUP_GAP = 0.95, 0.75


def label(text, at, size=0.10):
    curve = bpy.data.curves.new(type="FONT", name="lbl")
    curve.body = text
    curve.size = size
    curve.align_x = "CENTER"
    obj = bpy.data.objects.new("lbl_" + text, curve)
    obj.location = at
    obj.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.collection.objects.link(obj)
    dark = bpy.data.materials.new("M_label")
    dark.use_nodes = True
    dark.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.10, 0.10, 0.11, 1)
    obj.data.materials.append(dark)


def main():
    kit.clear()

    built = []
    for name, maker in chars.CATALOGUE:
        obj = maker()
        obj.name = name
        built.append(obj)

    # Laid out along X, all on one line. The first sheet put each character on
    # its own row in Y with a nearly-level camera -- and a nearly-level camera
    # foreshortens Y to almost nothing, so all three landed on top of each
    # other. Rows only work when you are looking down.
    placed = []
    x = 0.0
    for source in built:
        first = x
        for yaw, view in VIEWS:
            copy = source.copy()
            copy.data = source.data
            bpy.context.collection.objects.link(copy)
            copy.location = (x, 0, 0)
            copy.rotation_euler = (0, 0, math.radians(yaw))
            placed.append(copy)
            label(view, (x, -0.42, 0.02), 0.058)
            x += STEP_X
        label(source.name.replace("SM_Char_", "").upper(),
              ((first + x - STEP_X) / 2, -0.42, 0.135), 0.085)
        bpy.data.objects.remove(source, do_unlink=True)
        x += GROUP_GAP

    # A 1.80 m rule, because "low poly" and "wrong scale" look identical in a
    # render with nothing to compare against.
    rule = kit.Build()
    for mark in range(9):
        rule.box((0, 0, 0.10 + mark * 0.20), (0.030, 0.030, 0.20),
                 "aco_esc" if mark % 2 else "reboco")
    bar = rule.finish("regua", bevel=0.0)
    bar.location = (x, 0, 0)
    placed.append(bar)
    label("1,80 m", (x, -0.42, 0.02), 0.058)

    ground = bpy.ops.mesh.primitive_plane_add(size=60, location=(0, 0, 0))
    floor = bpy.context.object
    grey = bpy.data.materials.new("M_floor")
    grey.use_nodes = True
    grey.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.52, 0.51, 0.48, 1)
    grey.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    floor.data.materials.append(grey)

    bpy.ops.object.light_add(type="SUN", location=(4, -6, 9))
    sun = bpy.context.object
    sun.data.energy = 4.2
    sun.data.angle = math.radians(3.0)
    sun.data.color = (1.0, 0.965, 0.906)
    sun.rotation_euler = (math.radians(44), math.radians(6), math.radians(-28))

    world = bpy.context.scene.world or bpy.data.worlds.new("W")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.50, 0.58, 0.70, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.62

    corners = []
    for obj in placed:
        for c in obj.bound_box:
            corners.append(obj.matrix_world @ Vector(c))
    low_x, high_x = min(v.x for v in corners) - 0.4, max(v.x for v in corners) + 0.4
    low_y, high_y = min(v.y for v in corners) - 0.5, max(v.y for v in corners) + 0.3
    high_z = max(v.z for v in corners) + 0.2
    mid = Vector(((low_x + high_x) / 2, (low_y + high_y) / 2, high_z * 0.45))

    tilt = math.radians(78)                 # nearly level: this is a turnaround
    view = Vector((0.0, math.sin(tilt), -math.cos(tilt)))
    bpy.ops.object.camera_add(location=mid - view * 60.0)
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.rotation_euler = (tilt, 0, 0)

    scene = bpy.context.scene
    scene.render.resolution_x = 1900
    scene.render.resolution_y = 950
    aspect = scene.render.resolution_x / scene.render.resolution_y
    across = high_x - low_x
    down = (high_y - low_y) * math.cos(tilt) + high_z * math.sin(tilt) + 0.3
    camera.data.ortho_scale = max(across, down * aspect)
    camera.data.clip_start = 1.0
    camera.data.clip_end = 200.0
    scene.camera = camera

    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "Standard"
    scene.render.filepath = os.path.join(kit.OUT, "personagens.png")
    bpy.ops.render.render(write_still=True)
    print("rendered", scene.render.filepath)


if __name__ == "__main__":
    main()
