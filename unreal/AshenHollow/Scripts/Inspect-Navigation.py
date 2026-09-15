"""Inspect courtyard navigation and fix the project's instanced FX material usage."""
import unreal,json
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/AshenHollow/Maps/Courtyard')
report=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a,(unreal.NavMeshBoundsVolume,unreal.RecastNavMesh,unreal.StaticMeshActor)):
        item={'name':a.get_name(),'class':a.get_class().get_name(),'bounds':str(a.get_actor_bounds(False))}
        if isinstance(a,unreal.RecastNavMesh): item['runtime']=str(a.get_editor_property('runtime_generation'))
        report.append(item)
m=unreal.load_asset('/Game/AshenHollow/FX/M_CombatGlow')
if m:
    m.set_editor_property('used_with_instanced_static_meshes',True)
    unreal.MaterialEditingLibrary.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
with open(unreal.Paths.project_saved_dir()+'NavigationInspect.json','w') as f: json.dump(report,f,indent=2)
