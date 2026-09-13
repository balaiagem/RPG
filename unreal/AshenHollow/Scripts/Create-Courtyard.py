"""Run once through UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
import unreal

MAP = '/Game/AshenHollow/Maps/Courtyard'
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Courtyard already exists; preserve manual edits instead of overwriting it.')
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.new_level(MAP)
cube = unreal.load_asset('/Engine/BasicShapes/Cube')

def block(name, position, scale):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor

block('Courtyard Floor', (0, 0, -50), (32, 32, 1))
for x in (-1600, 1600):
    block('Boundary Wall', (x, 0, 150), (1, 32, 3))
for y in (-1600, 1600):
    block('Boundary Wall', (0, y, 150), (32, 1, 3))
for x in (-650, 650):
    for y in (-550, 550):
        block('Pillar Base', (x, y, 25), (2, 2, 0.5))
        block('Courtyard Pillar', (x, y, 190), (1.2, 1.2, 3.3))
block('Navigation Obstacle', (0, 600, 80), (6, 1.5, 1.6))
start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -600, 110))
start.set_actor_label('Hero Spawn')
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(-48, -35, 0))
sun.light_component.set_editor_property('intensity', 3.0)
sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 150))
nav.set_actor_scale3d(unreal.Vector(18, 18, 4))
nav.set_actor_label('Courtyard Navigation')
origin, extent = nav.get_actor_bounds(False)
unreal.log('AH_NAV_BOUNDS ' + str(extent))
if extent.x < 1600 or extent.y < 1600:
    raise RuntimeError('Navigation volume has no usable brush geometry')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, '/Script/AshenHollow.AHGameMode'))
levels.save_current_level()
assert unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
assert unreal.load_class(None, '/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C')
unreal.log('AH_COURTYARD_CREATED')
