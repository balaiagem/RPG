import unreal
import json
import os
edit=unreal.MaterialEditingLibrary
rows=[]
for path in unreal.EditorAssetLibrary.list_assets('/Game/AshenHollow/Kit/Materials'):
    mat=unreal.load_asset(path)
    if not isinstance(mat,unreal.Material): continue
    row={'path':path,'color':str(edit.get_material_default_vector_parameter_value(mat,'Cor')),'nodes':[]}
    row['base']=str(edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR))
    for exp in edit.get_material_expressions(mat):
        row['nodes'].append({'type':exp.get_class().get_name(),'inputs':[str(x) for x in edit.get_inputs_for_material_expression(mat,exp)]})
    rows.append(row)
out=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'Saved','MaterialAudit.json')
with open(out,'w') as f: json.dump(rows,f,indent=2)
