"""Stylized lighting and grade for the Courtyard.

Run once through UnrealEditor-Cmd -run=pythonscript -script=<absolute path>,
or via Scripts/Light-Courtyard.ps1.

Idempotent: actors are found by label and updated, so re-running while tuning
does not litter the level with duplicates. Every property write goes through
setp(), which logs and continues if a name is missing -- property names drift
between engine versions and one bad name should not abandon a half-applied
level.
"""
import unreal

MAP = '/Game/AshenHollow/Maps/Courtyard'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Courtyard nao existe; rode Create-Courtyard.py antes.')
levels.load_level(MAP)

FAILED = []

def setp(target, name, value):
    """Set a property, recording rather than raising when the name is unknown."""
    try:
        target.set_editor_property(name, value)
        return True
    except Exception as error:                      # noqa: BLE001
        FAILED.append('%s.%s (%s)' % (type(target).__name__, name, error))
        return False

def find(label):
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None

def ensure(cls, label, location=unreal.Vector(), rotation=unreal.Rotator()):
    existing = find(label)
    if existing:
        return existing, False
    spawned = actors.spawn_actor_from_class(cls, location, rotation)
    spawned.set_actor_label(label)
    return spawned, True

def component(actor, cls, attr_names):
    """Components are exposed inconsistently; try the typed lookup then names."""
    try:
        found = actor.get_component_by_class(cls)
        if found:
            return found
    except Exception:                               # noqa: BLE001
        pass
    for name in attr_names:
        try:
            found = actor.get_editor_property(name)
            if found:
                return found
        except Exception:                           # noqa: BLE001
            continue
    return None

# ── Key light ────────────────────────────────────────────────────────────────
# Low warm sun. The steep angle the map shipped with flattened the pillars;
# dropping it lengthens shadows and gives the courtyard some read.
sun = find('Sun') or find('DirectionalLight')
if sun is None:
    sun, _ = ensure(unreal.DirectionalLight, 'Sun', unreal.Vector(0, 0, 1200))
sun.set_actor_label('Sun')
sun.set_actor_rotation(unreal.Rotator(-32.0, -50.0, 0.0), False)
light = component(sun, unreal.DirectionalLightComponent, ['directional_light_component', 'light_component'])
if light:
    setp(light, 'mobility', unreal.ComponentMobility.MOVABLE)
    setp(light, 'intensity', 6.0)
    setp(light, 'use_temperature', True)
    setp(light, 'temperature', 5200.0)
    setp(light, 'light_color', unreal.Color(255, 244, 224, 255))
    setp(light, 'cast_shadows', True)
    setp(light, 'dynamic_shadow_distance_movable_light', 12000.0)
    setp(light, 'enable_light_shaft_occlusion', True)
    setp(light, 'occlusion_mask_darkness', 0.35)

# ── Fill ─────────────────────────────────────────────────────────────────────
# Cool sky fill against the warm key. This contrast is most of what makes
# stylized art read as deliberate rather than flat.
sky = find('SkyLight') or find('Sky')
if sky is None:
    sky, _ = ensure(unreal.SkyLight, 'SkyLight', unreal.Vector(0, 0, 900))
sky.set_actor_label('SkyLight')
sky_component = component(sky, unreal.SkyLightComponent, ['sky_light_component', 'light_component'])
if sky_component:
    setp(sky_component, 'mobility', unreal.ComponentMobility.MOVABLE)
    setp(sky_component, 'real_time_capture', True)
    setp(sky_component, 'intensity', 1.8)
    setp(sky_component, 'light_color', unreal.Color(198, 216, 255, 255))
    setp(sky_component, 'volumetric_scattering_intensity', 1.0)

# ── Atmosphere and haze ──────────────────────────────────────────────────────
ensure(unreal.SkyAtmosphere, 'SkyAtmosphere')

fog, _ = ensure(unreal.ExponentialHeightFog, 'Haze', unreal.Vector(0, 0, 150))
fog_component = component(fog, unreal.ExponentialHeightFogComponent,
                          ['exponential_height_fog_component', 'component'])
if fog_component:
    setp(fog_component, 'mobility', unreal.ComponentMobility.MOVABLE)
    setp(fog_component, 'fog_density', 0.018)
    setp(fog_component, 'fog_height_falloff', 0.35)
    setp(fog_component, 'fog_inscattering_luminance', unreal.LinearColor(0.34, 0.42, 0.58, 1.0))
    setp(fog_component, 'start_distance', 900.0)
    # Volumetric fog is what sells depth here, and it is also the single most
    # expensive thing in this script. It scales with sg.EffectsQuality, so the
    # Desempenho profile on F6 already turns it down.
    setp(fog_component, 'enable_volumetric_fog', True)
    setp(fog_component, 'volumetric_fog_scattering_distribution', 0.4)
    setp(fog_component, 'volumetric_fog_albedo', unreal.Color(220, 226, 240, 255))
    setp(fog_component, 'volumetric_fog_extinction_scale', 0.8)
    setp(fog_component, 'volumetric_fog_distance', 6000.0)

# ── Grade ────────────────────────────────────────────────────────────────────
grade, _ = ensure(unreal.PostProcessVolume, 'Grade', unreal.Vector(0, 0, 300))
setp(grade, 'unbound', True)
setp(grade, 'priority', 1.0)
settings = grade.get_editor_property('settings')

def graded(name, value):
    """Post process settings need their override flag set to take effect."""
    if setp(settings, 'override_' + name, True):
        setp(settings, name, value)

# Locked exposure. Auto exposure is the main reason a stylized scene looks
# washed out and drifts as the camera pans.
#
# Pinning min == max is the safe way to lock it. Do NOT switch to AEM_MANUAL and
# lean on auto_exposure_bias: that value is EV compensation, so a figure like 10
# is about 1500x brightness and blows the frame to solid white.
graded('auto_exposure_min_brightness', 1.0)
graded('auto_exposure_max_brightness', 1.0)
graded('auto_exposure_bias', 1.0)

graded('bloom_intensity', 0.55)
graded('bloom_threshold', 0.6)
graded('vignette_intensity', 0.32)

# Split tone: warm highlights, cool shadows.
graded('color_saturation', unreal.Vector4(1.06, 1.04, 1.02, 1.0))
graded('color_contrast', unreal.Vector4(1.05, 1.05, 1.06, 1.0))
graded('color_gain_highlights', unreal.Vector4(1.04, 1.01, 0.95, 1.0))
graded('color_gain_shadows', unreal.Vector4(0.95, 0.98, 1.08, 1.0))

# Filmic curve with a little more shoulder than default, so highlights on the
# gold trim roll off instead of clipping.
graded('film_slope', 0.88)
graded('film_toe', 0.55)
graded('film_shoulder', 0.28)

graded('ambient_occlusion_intensity', 0.6)
graded('ambient_occlusion_radius', 120.0)

grade.set_editor_property('settings', settings)

levels.save_current_level()

unreal.log('AH_LIGHTING_APPLIED')
if FAILED:
    unreal.log_warning('AH_LIGHTING_SKIPPED %d property names:' % len(FAILED))
    for entry in FAILED:
        unreal.log_warning('  ' + entry)
else:
    unreal.log('AH_LIGHTING_CLEAN all properties accepted')
