"""Apply the authored collision boxes from kit.json in Unreal centimetres.

Interchange can ignore legacy FbxImportUI collision settings and replace UCX
with a single convex hull. A hull around a gateway seals the opening. Use the
same box specification exported with the FBX instead of import defaults.
"""
import unreal


def apply_collision(mesh, piece):
    specs = piece.get('collision')
    if specs is None:
        raise RuntimeError('Missing authored collision: ' + piece['name'])
    body = mesh.get_editor_property('body_setup')
    boxes = []
    for spec in specs:
        box = unreal.KBoxElem()
        box.set_editor_property('center', unreal.Vector(*[v * 100 for v in spec['centre']]))
        # Blender +Y becomes Unreal -Y with this FBX axis conversion.
        centre = box.get_editor_property('center')
        centre.y *= -1
        box.set_editor_property('center', centre)
        box.set_editor_property('rotation', unreal.Rotator(0, -spec.get('yaw_deg', 0), 0))
        for axis, size in zip(('x', 'y', 'z'), spec['size']):
            box.set_editor_property(axis, size * 100)
        boxes.append(box)
    geometry = unreal.KAggregateGeom()
    geometry.set_editor_property('box_elems', boxes)
    body.set_editor_property('agg_geom', geometry)
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    mesh.set_editor_property('body_setup', body)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
