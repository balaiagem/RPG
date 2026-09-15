import unreal, json
asset=unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle')
controller=asset.controller
report={'controller':str(type(controller))}
for name in ['set_bone_track_keys','set_number_of_frames','set_frame_rate']:
    report[name]=str(getattr(controller,name).__doc__)
report['pose_doc']=str(unreal.AnimationLibrary.get_bone_pose_for_time.__doc__)
for bone in ['upperarm_r','lowerarm_r','hand_r','upperarm_l','lowerarm_l','hand_l','spine_03']:
    pose=unreal.AnimationLibrary.get_bone_pose_for_time(asset,bone,0.0,False)
    report[bone]=str(pose)
with open(unreal.Paths.project_saved_dir()+'Authoring.json','w') as f: json.dump(report,f,indent=2)
