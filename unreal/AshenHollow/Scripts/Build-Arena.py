"""Builds the village-square arena as its own map.

Run through UnrealEditor-Cmd -run=pythonscript -script=<absolute path>, or via
Scripts/Build-Arena.ps1.

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
import math
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

def push_outside(actor, axis, sign, limit):
    """Slide the actor outward until the face nearest the centre sits at limit."""
    origin, extent = measure(actor)
    location = actor.get_actor_location()
    here   = getattr(origin, axis)
    reach  = getattr(extent, axis)
    delta  = (sign * limit) - (here - sign * reach)
    actor.set_actor_location(
        unreal.Vector(location.x + (delta if axis == 'x' else 0.0),
                      location.y + (delta if axis == 'y' else 0.0),
                      location.z), False, False)

# ── Spawning ─────────────────────────────────────────────────────────────────

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

def fence_run(path, x0, y0, x1, y1, label):
    """Tile a fence piece from one point to another, however long the piece is."""
    source = asset(path)
    if source is None:
        return
    probe = place(path, 0.0, 0.0, -50000.0, ground=False)
    if probe is None:
        note('fence probe failed')
        return
    _, extent = measure(probe)
    along_x = extent.x >= extent.y
    piece   = 2.0 * (extent.x if along_x else extent.y)
    actors.destroy_actor(probe)
    if piece < 1.0:
        note('fence piece measured as zero length')
        return
    span  = math.hypot(x1 - x0, y1 - y0)
    count = max(1, int(round(span / piece)))
    angle = math.degrees(math.atan2(y1 - y0, x1 - x0))
    # Turn the piece so its long side runs along the line, whichever axis that is.
    yaw   = angle if along_x else angle - 90.0
    for index in range(count):
        t = (index + 0.5) / count
        place(path, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.0, yaw,
              label='%s_%d' % (label, index))

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
for existing in actors.get_all_level_actors():
    try:
        actors.destroy_actor(existing)
    except Exception:                                            # noqa: BLE001
        pass            # world settings and the default brush cannot be destroyed

# ── Ground ───────────────────────────────────────────────────────────────────
# The ground reaches far past the fight. The playable square is only the middle
# of it; houses, fences, braziers and trees all stand outside that square, and
# without ground under them they would hang over the void.
#
# Three layers, each doing one job. A single solid slab carries collision, and
# therefore the navmesh, so walkable ground is one unbroken surface with no tile
# seams for Recast to trip over. On top of it, two grids of planes are pure
# paint: each plane gets the material's full 0-1 UV, so stone tiles at a
# believable size instead of being stretched fifty metres across one cube. The
# tighter, paler grid marks the square you actually fight on.
GROUND_HALF = 3400.0
stone  = asset(PACK + '/materials/MI_stonebrick_01')
ground = asset(PACK + '/materials/MI_landscape') or asset(PACK + '/materials/MI_stonebrick_02')

slab = place('/Engine/BasicShapes/Cube', 0.0, 0.0, 0.0, label='Arena Ground', ground=False)
if slab:
    scale_to(slab, 2.0 * GROUND_HALF, 2.0 * GROUND_HALF, 60.0)
    align_top(slab, 0.0)
    dress(slab, ground, collide=True)

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

paving(stone,  HALF,        300.0, 0.0,  'Paving')
paving(ground, GROUND_HALF, 600.0, HALF, 'Outskirt', lift=0.5)

# ── The square's walls: houses on four sides, fences closing the gaps ─────────
HOUSES = [
    (PACK + '/blueprints/buildings/BP_BLD_house_2',  'y',  1.0, 180.0, -260.0),
    (PACK + '/blueprints/buildings/BP_BLD_house_7',  'y',  1.0, 180.0,  420.0),
    (PACK + '/blueprints/buildings/BP_BLD_house_5',  'y', -1.0,   0.0,  260.0),
    (PACK + '/blueprints/buildings/BP_BLD_house_11', 'y', -1.0,   0.0, -430.0),
    (PACK + '/blueprints/buildings/BP_BLD_house_3',  'x',  1.0, 270.0,  120.0),
    (PACK + '/blueprints/buildings/BP_BLD_house_9',  'x', -1.0,  90.0, -160.0),
]
for path, axis, sign, yaw, offset in HOUSES:
    x = sign * (RING + 500.0) if axis == 'x' else offset
    y = sign * (RING + 500.0) if axis == 'y' else offset
    house = place(path, x, y, 0.0, yaw, label='House_' + path.rsplit('_', 1)[-1])
    if house:
        # However wide this particular house turns out to be, its inner wall ends
        # up on the same line as every other one.
        push_outside(house, axis, sign, RING + 120.0)
        seat, span = measure(house)
        unreal.log_warning('AH_ARENA_HOUSE %s at %.0f,%.0f size %.0f x %.0f x %.0f'
                   % (house.get_actor_label(), seat.x, seat.y, 2*span.x, 2*span.y, 2*span.z))

FENCE = PACK + '/meshes/props/construction/SM_PROP_fence_v01_01'
EDGE  = RING + 90.0
fence_run(FENCE, -EDGE, -EDGE,  EDGE, -EDGE, 'Fence_S')
fence_run(FENCE, -EDGE,  EDGE,  EDGE,  EDGE, 'Fence_N')
fence_run(FENCE, -EDGE, -EDGE, -EDGE,  EDGE, 'Fence_W')
fence_run(FENCE,  EDGE, -EDGE,  EDGE,  EDGE, 'Fence_E')

# ── Corner braziers ──────────────────────────────────────────────────────────
# Just outside the playable square: they light the fight and frame it without
# becoming four obstacles to path around.
for cx, cy in ((-RING, -RING), (RING, -RING), (-RING, RING), (RING, RING)):
    place(PACK + '/blueprints/props/BP_PROP_brazier_01', cx, cy, 0.0,
          label='Brazier_%d_%d' % (cx, cy))
    lamp = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(cx, cy, 190.0))
    if lamp:
        lamp.set_actor_label('BrazierLight_%d_%d' % (cx, cy))
        glow = component(lamp, ['point_light_component', 'light_component'])
        if glow:
            setp(glow, 'mobility', unreal.ComponentMobility.MOVABLE)
            setp(glow, 'intensity_units', unreal.LightUnits.LUMENS)
            setp(glow, 'intensity', 1600.0)
            setp(glow, 'attenuation_radius', 1100.0)
            setp(glow, 'light_color', unreal.Color(255, 176, 96, 255))
            setp(glow, 'cast_shadows', False)
            setp(glow, 'volumetric_scattering_intensity', 1.4)

# ── The raised deck ──────────────────────────────────────────────────────────
# High ground has to exist somewhere for the rule to mean anything. One deck,
# off to one side and clear of the line between the spawns, reached by two ramps
# shallow enough for Recast to walk up (about 21 degrees, well under the 44 the
# agent allows).
#
# These numbers are mirrored in AHArena.h as DeckCentreX / DeckKeepOut / DeckTop,
# which is how the runtime prop generator knows not to bury a barrel inside the
# deck. Change them here and change them there.
DECK_X, DECK_HALF_X, DECK_HALF_Y, DECK_TOP = 780.0, 260.0, 380.0, 130.0

deck = place('/Engine/BasicShapes/Cube', DECK_X, 0.0, 0.0, label='High Ground', ground=False)
if deck:
    scale_to(deck, 2.0 * DECK_HALF_X, 2.0 * DECK_HALF_Y, DECK_TOP)
    align_top(deck, DECK_TOP)
    dress(deck, stone, collide=True)

def ramp(tag, from_x, from_y, to_x, to_y, width):
    """
    A sloped slab bridging the floor and the deck.

    Scaled square first and rotated second: get_actor_bounds reports an
    axis-aligned box, so measuring a slab that is already tilted would size it
    against its shadow rather than against itself.
    """
    run  = math.hypot(to_x - from_x, to_y - from_y)
    slab = place('/Engine/BasicShapes/Cube', 0.0, 0.0, 0.0, label=tag, ground=False)
    if not slab:
        return
    scale_to(slab, math.hypot(run, DECK_TOP), width, 34.0)
    slab.set_actor_rotation(unreal.Rotator(
        roll=0.0,
        pitch=math.degrees(math.atan2(DECK_TOP, run)),
        yaw=math.degrees(math.atan2(to_y - from_y, to_x - from_x))), False)
    # Sunk slightly, so the ramp overlaps floor and deck instead of leaving a
    # lip at either end for a character to catch on.
    slab.set_actor_location(unreal.Vector((from_x + to_x) * .5, (from_y + to_y) * .5,
                                          DECK_TOP * .5 - 22.0), False, False)
    dress(slab, stone, collide=True)

ramp('Ramp_West',  DECK_X - DECK_HALF_X - 380.0, 0.0,    DECK_X - DECK_HALF_X, 0.0,    300.0)
ramp('Ramp_North', DECK_X, DECK_HALF_Y + 380.0,          DECK_X, DECK_HALF_Y,          280.0)

# ── Cover inside the square ──────────────────────────────────────────────────
# Nothing here any more. The obstacles are rolled at runtime by AHArena::Generate
# and spawned by AHGameMode::BuildArena, so every encounter gets a fresh layout
# and the map itself stays the permanent shell: ground, walls, deck, lighting.

# ── Dressing outside the fight ───────────────────────────────────────────────
place(PACK + '/blueprints/props/BP_PROP_well',        -RING - 260.0,  120.0, 0.0,  30.0, label='Well')
place(PACK + '/meshes/props/vehicles/SM_PROP_cart_02', RING + 300.0, -420.0, 0.0, 200.0, label='Cart_Idle')
place(PACK + '/meshes/props/construction/SM_PROP_market_v01_01', 180.0, RING + 240.0, 0.0, 180.0, label='Market')
place(PACK + '/blueprints/props/BP_PROP_campfire_01', RING + 260.0,  520.0, 0.0, 0.0, label='Campfire')

for index, (lx, ly) in enumerate(((-RING - 60.0, -520.0), (RING + 60.0, 480.0))):
    place(PACK + '/meshes/props/light/SM_PROP_streetlamp_v01_01', lx, ly, 0.0, 0.0,
          label='Streetlamp_%d' % index)

for index, (tx, ty) in enumerate(((-1750.0, -1500.0), (1850.0, -1250.0), (-1600.0, 1700.0),
                                  (1700.0, 1650.0), (300.0, -2000.0), (-400.0, 2050.0))):
    tree = place(PACK + '/meshes/environment/SM_ENV_TREE_village_LOD0', tx, ty, 0.0,
                 (index * 63) % 360, label='Tree_%d' % index)
    if tree:
        tree.set_actor_scale3d(unreal.Vector(1.0 + .12 * ((index % 3) - 1),
                                             1.0 + .12 * ((index % 3) - 1),
                                             1.0 + .18 * ((index % 2) - .5)))
        sit_on_ground(tree, 0.0)

# Grass hugging the paving edge, purely to soften the line where stone stops.
for index in range(24):
    angle = (index / 24.0) * math.tau
    gx, gy = math.cos(angle) * (HALF + 40.0), math.sin(angle) * (HALF + 40.0)
    grass = place(PACK + '/meshes/environment/SM_ENV_PLANT_grass_village', gx, gy, 0.0,
                  (index * 37) % 360, label='Grass_%d' % index)
    if grass:
        dress(grass, None, collide=False)

for index, (fx, fy, fyaw) in enumerate(((-RING - 30.0, 300.0, 90.0), (RING + 30.0, -100.0, 270.0))):
    banner = place(PACK + '/meshes/props/deco/SM_PROP_flag_01', fx, fy, 0.0, fyaw,
                   label='Banner_%d' % index)
    if banner:
        dress(banner, None, collide=False)

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
