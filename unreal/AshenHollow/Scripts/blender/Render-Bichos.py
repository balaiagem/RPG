"""Uma folha so com os bichos, com a camera posta na mao.

O render_novos.py enquadra pelo conteudo e com tres pecas pequenas e uma
grande ele apontou pro chao. Aqui a camera e fixa e o enquadramento foi
escolhido olhando: e uma folha de conferencia, nao um sistema.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kit as kit                                            # noqa: E402

OUT = kit.OUT
PECAS = ["SM_Kit_veado", "SM_Kit_galinha", "SM_Kit_corvo"]


def label(text, at, size=0.19):
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


def person(at):
    body = kit.Build()
    body.cylinder((0, 0, 0.0), 0.16, 0.86, "madeira", sides=8)
    body.box((0, 0, 1.18), (0.42, 0.28, 0.66), "pano")
    body.blob((0, 0, 1.64), (0.15, 0.13, 0.17), "tabua", rings=2, sides=6)
    made = body.finish("referencia_1m80", bevel=0.02)
    made.location = at


def main():
    kit.clear()
    kit.build_all()
    # TUDO que nao esta na folha sai -- inclusive as caixas UCX_, que nao
    # comecam com SM_Kit_ e foi isso que encheu o primeiro render de um plano
    # branco gigante na frente dos bichos.
    for obj in list(bpy.context.scene.objects):
        if obj.name not in PECAS:
            bpy.data.objects.remove(obj, do_unlink=True)

    x = -2.2
    for name in PECAS:
        obj = bpy.data.objects[name]
        obj.location = (x, 0, 1.3 if name == "SM_Kit_corvo" else 0.0)
        # Turned three-eighths, because head-on from above is the one angle at
        # which an animal has no silhouette, and it is also the angle the first
        # two sheets happened to use.
        obj.rotation_euler = (0, 0, math.radians(-125))
        label(name.replace("SM_Kit_", ""), (x, -1.25, 0.02))
        x += 2.2
    person((x + 0.2, 0, 0))
    label("pessoa 1,80 m", (x + 0.2, -1.25, 0.02))

    bpy.ops.mesh.primitive_plane_add(size=40, location=(1.0, 2.0, -0.01))
    floor = bpy.context.object
    grass = bpy.data.materials.new("M_floor")
    grass.use_nodes = True
    grass.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.36, 0.40, 0.29, 1)
    grass.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    floor.data.materials.append(grass)

    bpy.ops.object.light_add(type="SUN", location=(6, 8, 12))
    sun = bpy.context.object
    sun.data.energy = 4.0
    sun.data.angle = math.radians(2.2)
    sun.rotation_euler = (math.radians(46), math.radians(6), math.radians(-34))

    world = bpy.context.scene.world or bpy.data.worlds.new("W")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.47, 0.58, 0.72, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6

    tilt = math.radians(66)
    mid = Vector((1.0, 0.0, 0.55))
    view = Vector((0.0, math.sin(tilt), -math.cos(tilt)))
    bpy.ops.object.camera_add(location=mid - view * 30.0)
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 9.2
    camera.rotation_euler = (tilt, 0, 0)
    camera.data.clip_start = 1.0
    camera.data.clip_end = 120.0

    scene = bpy.context.scene
    scene.camera = camera
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 40
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1500
    scene.render.resolution_y = 900
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.render.filepath = os.path.join(OUT, "bichos.png")
    bpy.ops.render.render(write_still=True)
    print("rendered", scene.render.filepath)


if __name__ == "__main__":
    main()
