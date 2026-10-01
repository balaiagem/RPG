import os
import unreal

root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tasks=[]
for name in ('Footstep1','Footstep2','Footstep3','Impact','Forest'):
    task=unreal.AssetImportTask()
    task.filename=os.path.join(root,'Art','Audio',name+'.wav')
    task.destination_path='/Game/AshenHollow/Audio'
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    sound=unreal.load_asset(task.destination_path+'/'+task.destination_name)
    assert isinstance(sound,unreal.SoundWave), task.destination_name
    sound.set_editor_property('looping',task.destination_name=='Forest')
    unreal.EditorAssetLibrary.save_loaded_asset(sound,only_if_is_dirty=False)
unreal.log('AH_AUDIO saved 5 original sounds')
