import json
from pathlib import Path
import unreal
asset=unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run')
report={'class':asset.get_class().get_name(),'samples':[]}
for sample in asset.get_editor_property('sample_data'):
    animation=sample.get_editor_property('animation')
    position=sample.get_editor_property('sample_value')
    report['samples'].append({'animation':animation.get_path_name() if animation else None,'position':[position.x,position.y,position.z]})
Path(unreal.Paths.project_saved_dir(),'LocomotionAssets.json').write_text(json.dumps(report,indent=2))
