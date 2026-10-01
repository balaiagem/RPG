"""Import the locally authored armour kit; preserve slot names and save bindings."""
import json
import os
import unreal

root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
art=os.path.join(root,'Art','LifeKit')
with open(os.path.join(art,'life.json'),encoding='utf-8') as f:
    data=json.load(f)
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.EditorAssetLibrary
edit=unreal.MaterialEditingLibrary
folder='/Game/AshenHollow/LifeKit'
materials={}
for name,(rgb,rough,metal) in data['palette'].items():
    asset_name='M_Life_'+name
    path=folder+'/Materials/'+asset_name
    mat=lib.load_asset(path) if lib.does_asset_exist(path) else None
    if not mat: mat=assets.create_asset(asset_name,folder+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(mat)
    color=edit.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-400,0)
    color.set_editor_property('parameter_name','Tint')
    color.set_editor_property('default_value',unreal.LinearColor(*rgb,1))
    edit.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,rough),(unreal.MaterialProperty.MP_METALLIC,metal)]:
        node=edit.create_material_expression(mat,unreal.MaterialExpressionConstant,-200,200)
        node.set_editor_property('r',value); edit.connect_material_property(node,'',prop)
    if name=='Glow': edit.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('used_with_skeletal_mesh',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    edit.recompile_material(mat)
    lib.save_loaded_asset(mat,only_if_is_dirty=False)
    materials[asset_name]=mat
tasks=[]
for piece in data['pieces']:
    options=unreal.FbxImportUI()
    options.import_mesh=True; options.import_materials=False; options.import_textures=False
    options.import_as_skeletal=False
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    task=unreal.AssetImportTask()
    task.filename=os.path.join(art,piece['name']+'.fbx'); task.destination_path=folder+'/Meshes'
    task.destination_name=piece['name']; task.automated=True; task.replace_existing=True; task.save=True
    task.options=options; tasks.append(task)
assets.import_asset_tasks(tasks)
for piece in data['pieces']:
    mesh=lib.load_asset(folder+'/Meshes/'+piece['name'])
    if not mesh: raise RuntimeError('Missing '+piece['name'])
    for index,slot in enumerate(mesh.get_editor_property('static_materials')):
        wanted=str(slot.material_slot_name).split('.')[0]
        if wanted not in materials: raise RuntimeError('Unbound material '+wanted)
        mesh.set_material(index,materials[wanted])
    # All of this kit is cosmetic. No collision or navigation geometry is needed.
    mesh.get_editor_property('body_setup').set_editor_property('agg_geom',unreal.KAggregateGeom())
    lib.save_loaded_asset(mesh,only_if_is_dirty=False)
unreal.log('AH_LIFE saved %d meshes and %d materials' % (len(tasks),len(materials)))
