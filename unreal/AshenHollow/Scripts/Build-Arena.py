"""Builds the stage the arena is played on, as its own map.

Run through UnrealEditor-Cmd -run=pythonscript -script=<absolute path>, or via
Scripts/Build-Arena.ps1.

What this builds is only what cannot be rolled per encounter: the ground, the
navigation bounds, the spawn point and the light actors. Houses, walls, braziers,
stalls, the raised deck and the loose cover are generated at runtime by
AHArena::Build, so no two encounters share a layout.

Two decisions shape this file.

It writes a NEW map and never touches Courtyard, so the grey-box arena stays as
a known-good fallback: if this one is wrong, opening the old map gets the game
back with no repair work.

And it MEASURES every asset instead of assuming sizes. Nothing here hardcodes how
long a fence piece is, how wide a house is, or how tall a barrel is -- each actor
is spawned, asked for its bounds, and then placed from that answer. Guessing an
art pack's dimensions from a file listing costs a full editor run to find out it
was wrong, and the guesses are never right the first time.

Idempotent: re-running rebuilds the map from zero, so it is safe to iterate.
"""
import unreal

MAP      = '/Game/AshenHollow/Maps/ArenaVillage'
PACK     = '/Game/Fantastic_Village_Pack'

# The playable square, in centimetres from the centre. 26 x 26 m, with the two
# spawns ten metres apart at (0,-500) and (0,+500) -- AHGameMode::HeroSpawn and
# FoeSpawn hold the same two numbers and must agree with these.
#
# Ten metres is a deliberate compromise. A bigger square makes room to flank, to
# fall back and to use range, but spawning the two sides at opposite ends would
# spend two or three turns walking before anything happened, and a turn-based
# fight cannot afford dead turns. Scenery stays outside RING so the navmesh is
# still one clean square.
HALF     = 1300.0
RING     = 1360.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

PROBLEMS = []

def note(message):
    PROBLEMS.append(message)

def setp(target, name, value):
    """Set a property, recording rather than raising when the name is unknown."""
    try:
        target.set_editor_property(name, value)
        return True
    except Exception as error:                                   # noqa: BLE001
        note('%s.%s (%s)' % (type(target).__name__, name, error))
        return False

def component(actor, attr_names):
    """Components are exposed under different property names between versions."""
    for name in attr_names:
        try:
            found = actor.get_editor_property(name)
            if found:
                return found
        except Exception:                                        # noqa: BLE001
            continue
    note('no component on %s from %s' % (type(actor).__name__, attr_names))
    return None

def asset(path):
    try:
        loaded = unreal.EditorAssetLibrary.load_asset(path)
    except Exception as error:                                   # noqa: BLE001
        note('load %s (%s)' % (path, error))
        return None
    if loaded is None:
        note('missing asset %s' % path)
    return loaded

# ── Measuring helpers ────────────────────────────────────────────────────────
# Everything below asks the actor how big it actually is. get_actor_bounds
# returns the world-space centre and half-size after scaling, so it already
# accounts for whatever pivot the artist used.

def measure(actor):
    origin, extent = actor.get_actor_bounds(False)
    return origin, extent

def sit_on_ground(actor, ground=0.0):
    """Drop the actor so its lowest point rests on the ground plane."""
    origin, extent = measure(actor)
    location = actor.get_actor_location()
    actor.set_actor_location(
        unreal.Vector(location.x, location.y, location.z + (ground - (origin.z - extent.z))),
        False, False)

def align_top(actor, top=0.0):
    origin, extent = measure(actor)
    location = actor.get_actor_location()
    actor.set_actor_location(
        unreal.Vector(location.x, location.y, location.z + (top - (origin.z + extent.z))),
        False, False)

def scale_to(actor, size_x, size_y, size_z=None):
    """
    Rescale a primitive to an exact world size, whatever its authored size.
    size_z of None leaves the vertical scale alone, which is what a plane needs:
    it has no thickness, so solving for a Z size would divide by nearly zero.
    """
    origin, extent = measure(actor)
    scale = actor.get_actor_scale3d()
    actor.set_actor_scale3d(unreal.Vector(
        scale.x * (size_x * 0.5) / max(extent.x, 0.01),
        scale.y * (size_y * 0.5) / max(extent.y, 0.01),
        scale.z if size_z is None else scale.z * (size_z * 0.5) / max(extent.z, 0.01)))

def blueprint_class(path):
    """The spawnable class behind a Blueprint asset, by whichever API exists."""
    try:
        found = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if found:
            return found
    except Exception:                                            # noqa: BLE001
        pass
    try:
        return unreal.load_class(None, path + '.' + path.rsplit('/', 1)[-1] + '_C')
    except Exception as error:                                   # noqa: BLE001
        note('no blueprint class for %s (%s)' % (path, error))
        return None

def place(path, x, y, z=0.0, yaw=0.0, scale=1.0, label=None, ground=True):
    """
    Spawn one asset into the level.

    Deliberately NOT spawn_actor_from_object. That call routes through the
    editor's actor factories, and a headless commandlet never populates them: it
    returns None for every asset, including the engine's own cube, and the level
    comes out empty with no error anywhere. Spawning a class and then assigning
    the mesh goes nowhere near the factories and behaves the same in the full
    editor and in the commandlet.
    """
    source = asset(path)
    if source is None:
        return None
    location = unreal.Vector(x, y, z)
    rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
    try:
        if isinstance(source, unreal.Blueprint):
            generated = blueprint_class(path)
            if generated is None:
                return None
            actor = actors.spawn_actor_from_class(generated, location, rotation)
        else:
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
            if actor is not None:
                body = actor.get_editor_property('static_mesh_component')
                if body is not None:
                    # Movable first: assigning a mesh to, or moving, a component
                    # left on Static mobility trips the engine's own guard.
                    setp(body, 'mobility', unreal.ComponentMobility.MOVABLE)
                    # Property first, setter as backup. One empty level caused by
                    # a silently-failing spawn was enough; a renamed property must
                    # not be able to do it again.
                    if not setp(body, 'static_mesh', source):
                        try:
                            body.set_static_mesh(source)
                        except Exception as error:               # noqa: BLE001
                            note('set_static_mesh %s (%s)' % (path, error))
                            return None
    except Exception as error:                                   # noqa: BLE001
        note('spawn %s (%s)' % (path, error))
        return None
    if actor is None:
        note('spawn returned nothing for %s' % path)
        return None
    if scale != 1.0:
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if ground and mesh_component(actor) is not None:
        # Only plain static meshes get dropped onto the ground. A blueprint prop
        # can carry particle systems, and their bounds reach well past the model,
        # so "measure it and sit it down" would bury a brazier in the dirt. Pack
        # blueprints are authored with the pivot at the base; trust that instead.
        sit_on_ground(actor, z)
    if label:
        actor.set_actor_label(label)
    return actor

def mesh_component(actor):
    try:
        return actor.get_editor_property('static_mesh_component')
    except Exception:                                            # noqa: BLE001
        return None

def dress(actor, material=None, collide=True):
    component = mesh_component(actor)
    if component is None:
        return
    if material is not None:
        try:
            component.set_material(0, material)
        except Exception as error:                               # noqa: BLE001
            note('set_material (%s)' % error)
    if not collide:
        # Decoration must not reach the navmesh. Navigation is built from
        # collision, so switching collision off is also what keeps these out of it.
        try:
            component.set_collision_profile_name('NoCollision')
        except Exception as error:                               # noqa: BLE001
            note('set_collision_profile_name (%s)' % error)

def paving(material, half, tile, skip_half, tag, lift=1.0):
    steps = int(round(2.0 * half / tile))
    for ix in range(steps):
        for iy in range(steps):
            px = -half + tile * (ix + 0.5)
            py = -half + tile * (iy + 0.5)
            if skip_half and abs(px) < skip_half and abs(py) < skip_half:
                continue        # the inner grid already covers this
            slate = place('/Engine/BasicShapes/Plane', px, py, lift,
                          label='%s_%d_%d' % (tag, ix, iy), ground=False)
            if slate:
                scale_to(slate, tile, tile)
                slate.set_actor_location(unreal.Vector(px, py, lift), False, False)
                dress(slate, material, collide=False)

# ── Start from an empty level ────────────────────────────────────────────────
SOURCE = '/Game/AshenHollow/Maps/Courtyard'
if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    # Copy the grey-box map and empty it, rather than asking for a brand new
    # level. Duplicating an asset is dependable when the editor runs headless as
    # a commandlet, and it leaves Courtyard itself untouched either way.
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE):
        raise RuntimeError('Courtyard nao existe; nao ha mapa base para copiar.')
    unreal.EditorAssetLibrary.duplicate_asset(SOURCE, MAP)
levels.load_level(MAP)

# Some actors in a level are not the level's to delete -- world settings, the
# default brush, the navigation data the engine maintains itself. Asking anyway
# does no harm to the map, but each refusal is logged as an Error, and a
# commandlet returns the number of errors it logged. Dozens of harmless refusals
# therefore came back as a failed build on a map that had built perfectly.
PERMANENT = tuple(Kind for Kind in (getattr(unreal, Name, None) for Name in (
    'WorldSettings', 'Brush', 'AbstractNavData', 'NavigationData',
    'DefaultPhysicsVolume', 'WorldDataLayers', 'LevelBounds', 'GameModeBase',
)) if Kind is not None)

for existing in actors.get_all_level_actors():
    if isinstance(existing, PERMANENT):
        continue
    try:
        actors.destroy_actor(existing)
    except Exception:                                            # noqa: BLE001
        pass

# ── Ground ───────────────────────────────────────────────────────────────────
# The ground reaches far past the fight, because the houses, fences and trees the
# game spawns at runtime all stand outside the playable square and would otherwise
# hang over the void.
#
# Three layers, each doing one job. A single solid slab carries collision, and
# therefore the navmesh, so walkable ground is one unbroken surface with no tile
# seams for Recast to trip over. On top of it, two grids of planes are pure paint:
# each plane gets the material's full 0-1 UV, so stone tiles at a believable size
# instead of being stretched sixty-eight metres across one cube. The tighter,
# paler grid marks the square you actually fight on.
GROUND_HALF = 3400.0
stone  = asset(PACK + '/materials/MI_stonebrick_01')
ground = asset(PACK + '/materials/MI_landscape') or asset(PACK + '/materials/MI_stonebrick_02')

slab = place('/Engine/BasicShapes/Cube', 0.0, 0.0, 0.0, label='Arena Ground', ground=False)
if slab:
    scale_to(slab, 2.0 * GROUND_HALF, 2.0 * GROUND_HALF, 60.0)
    align_top(slab, 0.0)
    dress(slab, ground, collide=True)

paving(stone,  HALF,        300.0, 0.0,  'Paving')
paving(ground, GROUND_HALF, 600.0, HALF, 'Outskirt', lift=0.5)

# ── Everything else is built at runtime ──────────────────────────────────────
# Houses, walls, braziers, market stalls, carts, trees, the raised deck and its
# ramp, and the loose cover are all rolled per encounter by AHArena::Build and
# spawned by AHGameMode::BuildArena. They deliberately do not live in the map.
#
# What stays baked is only what cannot be rolled: the ground everyone walks on,
# the navigation bounds, the spawn point, and the light actors themselves. The
# lights stay because a light spawned at runtime cannot be captured by the sky
# light the way a placed one can -- the game mode reaches in and changes their
# angle, temperature and intensity per seed instead, so each arena has its own
# hour of the day.

# ── Navigation ───────────────────────────────────────────────────────────────
# Spawn first, measure, then scale to the size we actually want. The volume's
# brush size is an engine detail that has changed between versions; deriving the
# scale from its own bounds means this never needs to know it.
nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0.0, 0.0, 200.0))
if nav:
    nav.set_actor_label('Arena Navigation')
    _, extent = measure(nav)
    nav.set_actor_scale3d(unreal.Vector(HALF / max(extent.x, 1.0),
                                        HALF / max(extent.y, 1.0),
                                        400.0 / max(extent.z, 1.0)))
    origin, checked = measure(nav)
    unreal.log_warning('AH_ARENA_NAV extent %.0f x %.0f x %.0f' % (checked.x, checked.y, checked.z))
else:
    note('NavMeshBoundsVolume failed to spawn')

start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, -500.0, 110.0),
                                      unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
if start:
    start.set_actor_label('Hero Spawn')
else:
    note('PlayerStart failed to spawn')

# ── Light and grade ──────────────────────────────────────────────────────────
# Same rig as Light-Courtyard, with one correction: rotators are built by keyword
# here. unreal.Rotator takes (roll, pitch, yaw) positionally, which is not the
# order anyone writing "-32 degrees of pitch" expects.
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1400.0))
if sun:
    sun.set_actor_label('Sun')
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-34.0, yaw=-55.0), False)
    key = component(sun, ['directional_light_component', 'light_component'])
    if key:
        setp(key, 'mobility', unreal.ComponentMobility.MOVABLE)
        setp(key, 'intensity', 5.5)
        setp(key, 'use_temperature', True)
        setp(key, 'temperature', 5200.0)
        setp(key, 'light_color', unreal.Color(255, 244, 224, 255))
        setp(key, 'cast_shadows', True)
        setp(key, 'dynamic_shadow_distance_movable_light', 14000.0)
        setp(key, 'enable_light_shaft_occlusion', True)
        setp(key, 'occlusion_mask_darkness', 0.35)

sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 900.0))
if sky:
    sky.set_actor_label('SkyLight')
    fill = component(sky, ['sky_light_component', 'light_component'])
    if fill:
        setp(fill, 'mobility', unreal.ComponentMobility.MOVABLE)
        setp(fill, 'real_time_capture', True)
        setp(fill, 'intensity', 1.7)
        setp(fill, 'light_color', unreal.Color(198, 216, 255, 255))
        setp(fill, 'volumetric_scattering_intensity', 1.0)

atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
if atmosphere:
    atmosphere.set_actor_label('SkyAtmosphere')

fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 150.0))
if fog:
    fog.set_actor_label('Haze')
    haze = component(fog, ['exponential_height_fog_component', 'component'])
    if haze:
        setp(haze, 'mobility', unreal.ComponentMobility.MOVABLE)
        setp(haze, 'fog_density', 0.016)
        setp(haze, 'fog_height_falloff', 0.35)
        setp(haze, 'fog_inscattering_luminance', unreal.LinearColor(0.34, 0.42, 0.58, 1.0))
        setp(haze, 'start_distance', 1200.0)
        setp(haze, 'enable_volumetric_fog', True)
        setp(haze, 'volumetric_fog_scattering_distribution', 0.4)
        setp(haze, 'volumetric_fog_albedo', unreal.Color(220, 226, 240, 255))
        setp(haze, 'volumetric_fog_extinction_scale', 0.8)
        setp(haze, 'volumetric_fog_distance', 6000.0)

grade = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 300.0))
if grade:
    grade.set_actor_label('Grade')
    setp(grade, 'unbound', True)
    setp(grade, 'priority', 1.0)
    settings = grade.get_editor_property('settings')

    def graded(name, value):
        if setp(settings, 'override_' + name, True):
            setp(settings, name, value)

    # Exposure is pinned by min == max. Never reach for auto_exposure_bias to do
    # this: that field is EV compensation, so a value like 10 is roughly 1500x and
    # renders a solid white screen.
    graded('auto_exposure_min_brightness', 1.0)
    graded('auto_exposure_max_brightness', 1.0)
    graded('auto_exposure_bias', 1.0)
    graded('bloom_intensity', 0.55)
    graded('bloom_threshold', 0.6)
    graded('vignette_intensity', 0.32)
    graded('color_saturation', unreal.Vector4(1.06, 1.04, 1.02, 1.0))
    graded('color_contrast', unreal.Vector4(1.05, 1.05, 1.06, 1.0))
    graded('color_gain_highlights', unreal.Vector4(1.04, 1.01, 0.95, 1.0))
    graded('color_gain_shadows', unreal.Vector4(0.95, 0.98, 1.08, 1.0))
    graded('film_slope', 0.88)
    graded('film_toe', 0.55)
    graded('film_shoulder', 0.28)
    graded('ambient_occlusion_intensity', 0.6)
    graded('ambient_occlusion_radius', 120.0)
    grade.set_editor_property('settings', settings)

# ── Nothing here is baked ────────────────────────────────────────────────────
# Not a single light in this project is static, so no primitive should be
# claiming it wants a lightmap. One actor left on Static mobility is enough for
# the editor to print "LIGHTING NEEDS TO BE REBUILT" across the screen in play,
# and rebuilding would bake nothing useful -- the answer is to stop asking.
DYNAMIC = 0
for built in actors.get_all_level_actors():
    try:
        pieces = built.get_components_by_class(unreal.SceneComponent)
    except Exception:                                            # noqa: BLE001
        continue
    for piece in pieces:
        try:
            if piece.get_editor_property('mobility') != unreal.ComponentMobility.MOVABLE:
                piece.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
                DYNAMIC += 1
        except Exception:                                        # noqa: BLE001
            pass        # volumes and a few engine components have no mobility
unreal.log_warning('AH_ARENA_DYNAMIC %d componentes trocados para movable' % DYNAMIC)

# ── Save ─────────────────────────────────────────────────────────────────────
levels.save_current_level()
unreal.EditorAssetLibrary.save_asset(MAP)

# Warning level on purpose: the console capture the .ps1 checks drops Display
# lines, so a success marker logged at Display is a success nobody can see.
unreal.log_warning('AH_ARENA_BUILT %s with %d actors' % (MAP, len(actors.get_all_level_actors())))
if PROBLEMS:
    unreal.log_warning('AH_ARENA_PROBLEMS %d:' % len(PROBLEMS))
    for entry in PROBLEMS:
        unreal.log_warning('  ' + entry)
else:
    unreal.log_warning('AH_ARENA_CLEAN every asset found and every property accepted')
