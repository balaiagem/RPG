"""Read-only audit of imported kit collision and material bindings."""
import json
import os
import unreal

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
with open(os.path.join(root, 'Art', 'Kit', 'kit.json'), encoding='utf-8') as f:
    manifest = json.load(f)
rows = []
for piece in manifest['pieces']:
    mesh = unreal.load_asset('/Game/AshenHollow/Kit/Meshes/' + piece['name'])
    row = {'name': piece['name'], 'expected_collision': piece.get('collision')}
    if mesh:
        row['materials'] = []
        for slot in mesh.get_editor_property('static_materials'):
            mat = slot.material_interface
            row['materials'].append({'slot': str(slot.material_slot_name),
                                     'material': mat.get_path_name() if mat else None,
                                     'instanced': mat.get_editor_property('used_with_instanced_static_meshes') if isinstance(mat, unreal.Material) else None})
        body = mesh.get_editor_property('body_setup')
        geom = body.get_editor_property('agg_geom')
        row['collision_mode'] = str(body.get_editor_property('collision_trace_flag'))
        row['geometry'] = {}
        for kind in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems'):
            row['geometry'][kind] = []
            for elem in geom.get_editor_property(kind):
                detail = {'value': str(elem)}
                for prop in ('vertex_data', 'elem_box', 'center', 'x', 'y', 'z'):
                    try:
                        detail[prop] = str(elem.get_editor_property(prop))
                    except Exception:
                        pass
                row['geometry'][kind].append(detail)
        row['bounds'] = str(mesh.get_bounding_box())
    rows.append(row)
with open(os.path.join(root, 'Saved', 'KitAudit.json'), 'w', encoding='utf-8') as f:
    json.dump(rows, f, indent=2)
unreal.log('AH_AUDIT saved KitAudit.json')
