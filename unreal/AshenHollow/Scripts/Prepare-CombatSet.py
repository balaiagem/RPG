"""Author project-owned, editable combat clips and equipment materials."""
import math
import unreal

lib=unreal.AnimationLibrary
idle=unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle')
base_pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(idle,0.0,unreal.AnimPoseEvaluationOptions())
notify=unreal.load_class(None,'/Script/AshenHollow.AHNotify_MeleeImpact')

def material(name,color,metallic=0.0,glow=False):
    path='/Game/AshenHollow/FX/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/AshenHollow/FX',unreal.Material,unreal.MaterialFactoryNew())
    c=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionVectorParameter,-300,0)
    c.set_editor_property('parameter_name','Tint')
    c.set_editor_property('default_value',unreal.LinearColor(*color,1))
    if glow:
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    unreal.MaterialEditingLibrary.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(unreal.MaterialProperty.MP_METALLIC,metallic),(unreal.MaterialProperty.MP_ROUGHNESS,.32 if metallic else .7)]:
        n=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-300,100)
        n.set_editor_property('r',value)
        unreal.MaterialEditingLibrary.connect_material_property(n,'',prop)
    unreal.MaterialEditingLibrary.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)

material('M_WeaponSteel',(.32,.40,.49),.85)
material('M_WeaponGold',(.58,.32,.07),.75)
material('M_WeaponLeather',(.065,.025,.012))
material('M_CombatGlow',(.3,3.5,7.0),glow=True)

def envelope(t,peak,hit):
    points=[(0,0),(.12,0),(peak,1),(hit,.45),(1,0)]
    for (a,x),(b,y) in zip(points,points[1:]):
        if t<=b:
            u=max(0,min(1,(t-a)/(b-a))); u=u*u*(3-2*u)
            return x+(y-x)*u
    return 0

# Upper-arm rotations are authored in mesh space, then converted to local tracks.
sets=[('AH_SwordSlash',1.1,.48,'strike'),('AH_AxeCleave',1.45,.7,'heavy'),
      ('AH_MaceStrike',1.2,.55,'strike'),('AH_StaffStrike',1.2,.55,'staff'),
      ('AH_Cast',1.35,.8,'cast'),('AH_Heal',1.1,None,'heal'),
      ('AH_Rage',1.2,None,'rage'),('AH_Guard',.65,None,'guard'),('AH_Evade',.55,None,'evade')]
for name,length,contact,kind in sets:
    path='/Game/AshenHollow/Animation/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): continue
    asset=unreal.EditorAssetLibrary.duplicate_asset(idle.get_path_name(),path)
    controller=asset.controller
    controller.set_frame_rate(unreal.FrameRate(30,1),False)
    frames=round(length*30); length=frames/30
    controller.set_number_of_frames(unreal.FrameNumber(frames),False)
    for bone,parent in [('upperarm_r','clavicle_r'),('upperarm_l','clavicle_l'),('spine_03','spine_02')]:
        local=lib.get_bone_pose_for_time(idle,bone,0,False)
        world=unreal.AnimPoseExtensions.get_bone_pose(base_pose,bone,unreal.AnimPoseSpaces.WORLD)
        parent_world=unreal.AnimPoseExtensions.get_bone_pose(base_pose,parent,unreal.AnimPoseSpaces.WORLD)
        rotations=[]
        for f in range(frames+1):
            t=f/frames; w=envelope(t,.38,.65)
            right=bone.endswith('_r'); spine=bone.startswith('spine')
            if spine:
                angles=(0,-12*w if kind=='evade' else 0,(-8 if kind=='heavy' else 5)*w)
            elif kind in ('cast','heal'):
                angles=(0,(-12 if right else 12)*w,(80 if kind=='cast' else 60)*w)
            elif kind=='rage': angles=((45 if right else -45)*w,0,25*w)
            elif kind=='guard': angles=(0,(-25 if right else 25)*w,65*w)
            elif kind=='evade': angles=(0,0,25*w)
            else:
                # Raised weapon winds up, cuts through the contact frame, then recovers.
                strike=envelope(t,.28,contact/length)
                angles=(0,(-25 if right else 8)*strike,(105 if right else 20)*strike)
            delta=unreal.Rotator(*angles).quaternion()
            q=parent_world.rotation
            inverse=unreal.Quat(-q.x,-q.y,-q.z,q.w)
            rotations.append(inverse*(delta*world.rotation))
        assert controller.set_bone_track_keys(bone,[local.translation]*(frames+1),rotations,[local.scale3d]*(frames+1),False)
    if contact is not None:
        lib.add_animation_notify_track(asset,'AH_Contact')
        assert lib.add_animation_notify_event(asset,'AH_Contact',contact,notify)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
    unreal.log('AH_COMBAT_AUTHORED '+name)
