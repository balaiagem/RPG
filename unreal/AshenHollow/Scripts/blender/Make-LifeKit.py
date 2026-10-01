"""Original faceted armour, masks and mace. Run in local Blender, no addons.
Metres, +X forward, +Z up; attachment origins are the named mannequin bones.
"""
import bpy
import math
import json
import os
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'Art', 'LifeKit')
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
PALETTE = {
    'Steel': ([.19,.27,.31], .34, .65),
    'Edge': ([.49,.62,.64], .28, .75),
    'Gold': ([.66,.36,.10], .32, .65),
    'Leather': ([.105,.052,.029], .83, 0),
    'Accent': ([.08,.32,.30], .8, 0),
    'Bone': ([.73,.64,.43], .68, 0),
    'Dark': ([.018,.027,.036], .85, 0),
    'Glow': ([.10,.68,.54], .3, 0),
    'Underarmor': ([.065,.095,.115], .65, .18),
}
MATS = {}
for name, (rgb, rough, metal) in PALETTE.items():
    m = bpy.data.materials.new('M_Life_' + name)
    m.diffuse_color = (*rgb,1)
    m.use_nodes = True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*rgb,1)
    p.inputs['Roughness'].default_value=rough
    p.inputs['Metallic'].default_value=metal
    if name=='Glow':
        p.inputs['Emission Color'].default_value=(*rgb,1)
        p.inputs['Emission Strength'].default_value=2
    MATS[name]=m

parts=[]
def finish_part(obj,mat,bevel=0):
    obj.data.materials.append(MATS[mat])
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=obj.modifiers.new('Forged edges','BEVEL'); mod.width=bevel; mod.segments=1
        bpy.context.view_layer.objects.active=obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    parts.append(obj)
    return obj

def box(loc,size,mat,bevel=.003,rotation=(0,0,0)):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc,rotation=rotation)
    o=bpy.context.object; o.dimensions=size
    return finish_part(o,mat,bevel)

def gem(loc,size,mat,sub=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub,radius=1,location=loc)
    o=bpy.context.object; o.scale=size
    return finish_part(o,mat)

def rod(a,b,r1,r2,mat,vertices=8):
    span=Vector(b)-Vector(a)
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r1,radius2=r2,depth=span.length,location=(Vector(a)+Vector(b))/2)
    o=bpy.context.object; o.rotation_euler=span.to_track_quat('Z','Y').to_euler()
    return finish_part(o,mat,.0015)

def band(z,rx,ry,thick,mat):
    for i in range(12):
        a=i*math.tau/12; b=(i+1)*math.tau/12
        rod((math.cos(a)*rx,math.sin(a)*ry,z),(math.cos(b)*rx,math.sin(b)*ry,z),thick,thick,mat,6)

def eye_pair(x,z):
    for s in (-1,1):
        box((x,s*.053,z),(.012,.055,.018),'Dark',.002)
        box((x+.008,s*.053,z),(.007,.035,.008),'Glow',.001)

def helm(style):
    if style=='Iron':
        gem((0,0,.085),(.137,.128,.163),'Steel',2)
        band(.015,.131,.12,.008,'Gold')
        box((.125,0,.065),(.032,.198,.099),'Steel',.008)
        box((.145,0,.047),(.017,.026,.102),'Gold',.003)
        eye_pair(.146,.097)
        for s in (-1,1):
            for z in (.018,.043):
                box((.145,s*.052,z),(.008,.04,.006),'Dark',0)
            box((.02,s*.127,-.006),(.112,.026,.093),'Steel',.005)
            gem((.064,s*.146,.022),(.008,.008,.008),'Gold')
        rod((-.045,0,.215),(-.08,0,.33),.045,.008,'Accent')
    elif style=='Hunter':
        gem((-.018,0,.095),(.155,.144,.185),'Accent',2)
        box((.111,0,.06),(.045,.177,.146),'Leather',.014)
        eye_pair(.139,.10)
        for s in (-1,1):
            box((.115,s*.08,-.025),(.055,.033,.102),'Leather',.006)
            rod((.142,s*.07,.008),(.16,s*.05,-.04),.008,.003,'Bone')
        for i in range(5):
            box((.143,(i-2)*.025,.02),(.009,.012,.008),'Gold',.001)
    else:
        gem((-.01,0,.09),(.139,.124,.155),'Dark',2)
        gem((.103,0,.064),(.074,.115,.135),'Bone',1)
        eye_pair(.165,.099)
        rod((.166,0,.071),(.177,0,-.067),.025,.005,'Bone')
        for s in (-1,1):
            rod((-.025,s*.10,.17),(-.045,s*.19,.29),.035,.023,'Bone')
            rod((-.045,s*.19,.29),(.006,s*.25,.41),.023,.002,'Bone')
            rod((-.04,s*.16,.26),(.07,s*.20,.32),.017,.001,'Bone')
        gem((.105,0,.20),(.025,.022,.034),'Glow')

def chest(style):
    base='Steel' if style=='Iron' else 'Leather' if style=='Hunter' else 'Dark'
    # Tapered cuirass with separately modelled ribs, seams, straps and rivets.
    verts=[]
    for z,rx,ry in [(-.30,.125,.17),(-.16,.16,.215),(.07,.17,.24),(.145,.105,.14)]:
        for i in range(8):
            a=math.tau*i/8; verts.append((rx*math.cos(a),ry*math.sin(a),z))
    faces=[]
    for j in range(3):
        for i in range(8): faces.append((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i))
    mesh=bpy.data.meshes.new('Cuirass'); mesh.from_pydata(verts,[],faces); mesh.update()
    o=bpy.data.objects.new('Cuirass',mesh); bpy.context.collection.objects.link(o)
    bpy.context.view_layer.objects.active=o; o.select_set(True); finish_part(o,base)
    for row in range(3):
        z=-.22+row*.067
        box((.148,0,z),(.037,.31+row*.02,.049),base,.009)
        for s in (-1,1): gem((.174,s*(.13+row*.008),z),(.007,.007,.007),'Gold')
    box((.182,0,-.025),(.015,.068,.28),'Accent',.003)
    gem((.197,0,.058),(.028,.045,.046),'Gold')
    gem((.222,0,.059),(.014,.023,.025),'Glow' if style=='Seer' else 'Edge')
    band(-.285,.131,.18,.012,'Leather')
    box((.15,0,-.285),(.035,.067,.045),'Gold',.005)
    if style=='Seer':
        for i in range(7):
            y=(i-3)*.043
            rod((.15,y,.095),(.19,y,.027-abs(i-3)*.009),.011,.004,'Bone')
    if style=='Hunter':
        for s in (-1,1):
            box((.133,s*.13,-.315),(.073,.098,.103),'Leather',.008)
            box((.175,s*.13,-.302),(.008,.014,.07),'Gold',.001)

def shoulder(style):
    gem((0,0,-.025),(.14,.13,.10),'Steel' if style=='Iron' else 'Leather',1)
    for i in range(3): box((0,0,-.055-i*.032),(.20-i*.02,.20-i*.015,.042),'Steel' if style=='Iron' else 'Accent',.008)
    for s in (-1,1): gem((.094,s*.057,-.019),(.009,.009,.009),'Gold')
    if style=='Seer':
        for i in range(3): rod((0,(i-1)*.06,.02),(-.03,(i-1)*.085,.13),.025,.002,'Bone')

def mace():
    rod((0,0,-.12),(0,0,.49),.024,.029,'Leather',10)
    for z in range(9): band(-.09+z*.033,.027,.027,.003,'Gold')
    rod((0,0,.39),(0,0,.63),.064,.055,'Steel',8)
    for i in range(6):
        a=i*math.tau/6
        box((math.cos(a)*.075,math.sin(a)*.075,.53),(.12,.026,.22),'Edge',.006,(0,0,a))
    gem((0,0,.68),(.05,.05,.066),'Gold')
    gem((0,0,-.155),(.037,.037,.046),'Gold')

catalogue=[]
def export(name,build):
    global parts
    bpy.ops.object.select_all(action='DESELECT'); parts=[]; build()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts: obj.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    bpy.context.scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,name+'.fbx'),use_selection=True,
        object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,
        bake_anim=False,mesh_smooth_type='FACE',use_triangles=True)
    catalogue.append({'name':name,'materials':[s.material.name for s in obj.material_slots],
                      'triangles':sum(len(p.vertices)-2 for p in obj.data.polygons)})
    obj.hide_render=True; obj.hide_set(True)
    return obj

for style in ('Iron','Hunter','Seer'):
    export('SM_Life_Helm_'+style,lambda s=style:helm(s))
    export('SM_Life_Chest_'+style,lambda s=style:chest(s))
    export('SM_Life_Shoulder_'+style,lambda s=style:shoulder(s))
export('SM_Life_Mace',mace)
with open(os.path.join(OUT,'life.json'),'w',encoding='utf-8') as f:
    json.dump({'palette':PALETTE,'pieces':catalogue},f,indent=2)

# Reusable studio proof sheet, kept with the editable source scene.
for n,style in enumerate(('Iron','Hunter','Seer')):
    for typ,z in [('Chest',.55),('Helm',.82),('Shoulder',.6)]:
        ob=bpy.data.objects['SM_Life_'+typ+'_'+style]
        ob.hide_set(False); ob.hide_render=False
        ob.location=(0,(n-1)*.75,z)
        if typ=='Shoulder': ob.location.y+=.26
bpy.ops.object.camera_add(location=(3,-3,2.4))
camera=bpy.context.object; camera.rotation_euler=(Vector((0,0,.62))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'; camera.data.ortho_scale=2.8; bpy.context.scene.camera=camera
for loc,power,size,color in [((2,-2,4),450,3,(1,.85,.65)),((-1,2,3),550,2,(.45,.7,1))]:
    bpy.ops.object.light_add(type='AREA',location=loc); lamp=bpy.context.object
    lamp.data.energy=power; lamp.data.shape='DISK'; lamp.data.size=size; lamp.data.color=color
    lamp.rotation_euler=(Vector((0,0,.6))-lamp.location).to_track_quat('-Z','Y').to_euler()
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
if not scene.world: scene.world=bpy.data.worlds.new('Studio')
scene.world.color=(.16,.16,.16); scene.render.resolution_x=1400; scene.render.resolution_y=900
scene.render.resolution_percentage=100; scene.render.image_settings.file_format='PNG'
scene.render.filepath=os.path.join(OUT,'LifeKit-preview.png')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'LifeKit.blend'))
bpy.ops.render.render(write_still=True)
print('AH_LIFE exported',len(catalogue),'assets',sum(p['triangles'] for p in catalogue),'triangles')
