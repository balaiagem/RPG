"""Create the project-owned emissive dart material; safe to run repeatedly."""
import unreal

path = '/Game/AshenHollow/FX/M_ArcaneDart'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ArcaneDart', '/Game/AshenHollow/FX', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -200, 0)
    color.set_editor_property('constant', unreal.LinearColor(0.3, 3.5, 7.0, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Arcane dart material was not saved')
unreal.log('AH_MAGIC_MATERIAL_READY')
