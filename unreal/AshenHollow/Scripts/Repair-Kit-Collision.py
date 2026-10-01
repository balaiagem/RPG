"""Repair collision only, preserving mesh geometry and material bindings."""
import importlib.util
import json
import os
import unreal

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('kit_collision', os.path.join(here, 'Kit-Collision.py'))
collision = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collision)
with open(os.path.join(here, '..', 'Art', 'Kit', 'kit.json'), encoding='utf-8') as f:
    manifest = json.load(f)
for piece in manifest['pieces']:
    mesh = unreal.load_asset('/Game/AshenHollow/Kit/Meshes/' + piece['name'])
    if not mesh:
        raise RuntimeError('Missing mesh: ' + piece['name'])
    collision.apply_collision(mesh, piece)
unreal.log('AH_KIT repaired authored collision for %d meshes' % len(manifest['pieces']))
