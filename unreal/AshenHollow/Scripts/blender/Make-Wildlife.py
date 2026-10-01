"""Original faceted wolf and brown bear. Metres, +X forward, +Z up.
Body and hip-pivot leg meshes support the runtime quadruped gait.
"""
import bpy, math, json, os
from mathutils import Vector
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT=os.path.join(ROOT,'Art','Wildlife'); os.makedirs(OUT,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
palette={'Fur':([.22,.27,.3],.94,0),'Ruff':([.43,.47,.44],.95,0),
         'Brown':([.23,.09,.034],.95,0),'Honey':([.43,.23,.086],.95,0),
         'Nose':([.018,.021,.024],.8,0),'Amber':([.85,.42,.035],.35,0),
         'Claw':([.65,.57,.4],.75,0)}
mats={}
for name,(rgb,r,m) in palette.items():
    mat=bpy.data.materials.new('M_Wild_'+name); mat.diffuse_color=(*rgb,1); mat.use_nodes=True
    node=mat.node_tree.nodes.get('Principled BSDF'); node.inputs['Base Color'].default_value=(*rgb,1)
    node.inputs['Roughness'].default_value=r; mats[name]=mat
parts=[]
def ell(pos,scale,mat,sub=2):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub,radius=1,location=pos)
    ob=bpy.context.object; ob.scale=scale; ob.data.materials.append(mats[mat]); parts.append(ob); return ob
def cone(a,b,r1,r2,mat):
    d=Vector(b)-Vector(a)
    bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=r1,radius2=r2,depth=d.length,location=(Vector(a)+Vector(b))/2)
    ob=bpy.context.object; ob.rotation_euler=d.to_track_quat('Z','Y').to_euler()
    ob.data.materials.append(mats[mat]); parts.append(ob)
def body(bear):
    fur='Brown' if bear else 'Fur'; light='Honey' if bear else 'Ruff'
    z=.83 if bear else .66
    ell((-.04,0,z),(.79,.37,.4) if bear else (.62,.235,.265),fur)
    ell((.41,0,z+.12),(.4,.38,.41) if bear else (.32,.27,.34),fur)
    ell((.68,0,z+.13),(.34,.285,.285) if bear else (.27,.2,.225),fur)
    ell((.89,0,z+.045),(.26,.185,.13) if bear else (.285,.122,.105),light)
    ell((1.105,0,z+.065),(.065,.11,.07) if bear else (.045,.079,.05),'Nose')
    for s in (-1,1):
        ell((.82,s*(.226 if bear else .165),z+.22),(.037,.022,.025),'Nose',1)
        ell((.83,s*(.245 if bear else .183),z+.222),(.017,.009,.012),'Amber',1)
        if bear: ell((.55,s*.23,z+.365),(.115,.085,.115),fur)
        else:
            cone((.53,s*.135,z+.29),(.49,s*.18,z+.54),.098,.009,fur)
            cone((.56,s*.135,z+.31),(.525,s*.18,z+.5),.048,.002,light)
    if bear: ell((-.8,0,z),(.115,.1,.1),fur,1)
    else:
        cone((-.55,0,.69),(-1.0,0,.37),.13,.08,fur)
        cone((-1,0,.37),(-1.15,0,.24),.08,.015,light)
        # Layered neck ruff, readable from the overhead camera.
        for s in (-1,1):
            for i in range(3): cone((.3-i*.07,s*.19,.74-i*.04),(.13-i*.06,s*.29,.56-i*.03),.09,.008,light)
def leg(bear):
    h=.74 if bear else .58; fur='Brown' if bear else 'Fur'
    cone((0,0,0),(-.035,0,-h*.55),.125 if bear else .07,.09 if bear else .045,fur)
    cone((-.035,0,-h*.55),(.045,0,-h+.075),.09 if bear else .045,.065 if bear else .035,fur)
    ell((.08,0,-h+.055),(.16,.12,.085) if bear else (.115,.065,.055),fur)
    for s in (-1,0,1): cone((.17,s*(.052 if bear else .027),-h+.05),(.235 if bear else .195,s*(.052 if bear else .027),-h+.01),.013,.003,'Claw')
catalog=[]
def export(name,build):
    global parts
    parts=[]; build(); bpy.ops.object.select_all(action='DESELECT')
    for ob in parts: ob.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join(); ob=bpy.context.object; ob.name=name
    bpy.context.scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,name+'.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,bake_anim=False,mesh_smooth_type='FACE',use_triangles=True)
    catalog.append({'name':name,'triangles':sum(len(p.vertices)-2 for p in ob.data.polygons)})
    ob.hide_render=True; ob.hide_set(True)
for bear,name in [(False,'Wolf'),(True,'Bear')]:
    export('SM_'+name+'_Body',lambda b=bear:body(b))
    export('SM_'+name+'_Leg',lambda b=bear:leg(b))
with open(os.path.join(OUT,'wildlife.json'),'w',encoding='utf-8') as f: json.dump({'palette':palette,'pieces':catalog},f,indent=2)
for bear,name,y in [(False,'Wolf',-.9),(True,'Bear',.9)]:
    ob=bpy.data.objects['SM_'+name+'_Body']; ob.hide_set(False); ob.hide_render=False; ob.location.y=y
    for x in (-1,1):
        for side in (-1,1):
            legob=bpy.data.objects['SM_'+name+'_Leg'].copy(); bpy.context.collection.objects.link(legob)
            legob.hide_set(False); legob.hide_render=False
            legob.location=(x*(.45 if bear else .37),y+side*(.29 if bear else .19),.74 if bear else .58)
bpy.ops.object.camera_add(location=(3.8,-4.8,3.0)); cam=bpy.context.object
cam.rotation_euler=(Vector((0,0,.6))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO'; cam.data.ortho_scale=3.9; bpy.context.scene.camera=cam
for loc,power,color in [((2,-3,5),650,(1,.86,.69)),((-2,2,4),800,(.65,.8,1))]:
    bpy.ops.object.light_add(type='AREA',location=loc); ob=bpy.context.object; ob.data.energy=power; ob.data.size=4; ob.data.color=color
    ob.rotation_euler=(Vector((0,0,.5))-ob.location).to_track_quat('-Z','Y').to_euler()
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=16
scene.world=bpy.data.worlds.new('Studio'); scene.world.color=(.15,.15,.15)
scene.render.resolution_x=1200; scene.render.resolution_y=900; scene.render.resolution_percentage=100
scene.render.filepath=os.path.join(OUT,'Wildlife-preview.png')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'Wildlife.blend')); bpy.ops.render.render(write_still=True)
print('AH_WILDLIFE exported',len(catalog),'meshes')
