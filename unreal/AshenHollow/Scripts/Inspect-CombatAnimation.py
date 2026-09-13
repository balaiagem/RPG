"""Read-only animation sampling; writes a small report under Saved."""
import json
from pathlib import Path
import unreal

report = []
for name in ('MM_Attack_01', 'MM_Attack_02', 'MM_Attack_03'):
    asset = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/Attack/' + name)
    length = unreal.AnimationLibrary.get_sequence_length(asset)
    samples = []
    for frame in range(round(length * 30) + 1):
        time = min(frame / 30.0, length)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(asset, time, unreal.AnimPoseEvaluationOptions())
        points = {}
        for bone in ('pelvis', 'hand_l', 'hand_r'):
            p = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
            points[bone] = [p.x, p.y, p.z]
        samples.append(dict(time=time, points=points))
    report.append(dict(asset=asset.get_path_name(), length=length, samples=samples))
out = Path(unreal.Paths.project_saved_dir()) / 'CombatAnimationSamples.json'
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('AH_ANIMATION_REPORT ' + str(out))
