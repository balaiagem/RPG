"""Fits the existing map to the 1 km Landscape, without rebuilding it.

Run through UnrealEditor-Cmd -run=pythonscript -script=<absolute path>, or from
the editor's Output Log console with:  py "<absolute path>"

WHY THIS IS NOT Build-Arena.py
-------------------------------
Build-Arena.py duplicates a template and rebuilds the whole map from nothing.
Running it now would delete the Landscape, which is the one thing in the map
that cannot be regenerated from a script. So this file is surgical: it removes
exactly the actors the old flat world needed, resizes what has to grow, and
touches nothing else.

What it does, and why each one matters:

* **Deletes the old ground.** One 320 m slab with collision and 256 grass
  planes on top of it. They are now buried inside a kilometre of real terrain,
  and the slab in particular is a second walkable surface for Recast to
  rasterise -- a flat floor at Z=0 running underneath the hills.
* **Grows the navigation bounds** to cover the whole Landscape and its full
  height. This is the one that has been quietly breaking the game: the volume
  was sized for a 250 m world, the navmesh baked into the map stopped fitting
  it, and the log has been saying so at every load --
  "Recreating dtNavMesh instance ... serialized maxTiles 588 vs required 1176".
* **Moves the PlayerStart** to the arrival shelf. The code no longer trusts it
  -- AAHGameMode::PlaceHero stands the pawn on the traced terrain itself --
  but leaving a PlayerStart sixty metres from where the game actually starts
  is a trap for whoever reads the map next.
* **Traces the six sites and prints what it found.** This is the check that
  matters: the heightmap was authored with image rows as +Y and columns as +X,
  and if Unreal imported it the other way round every village in the game is
  on the wrong hill. The expected heights are printed beside the measured ones
  so a transposition is obvious instead of subtle.

Idempotent: safe to run again.
"""
import unreal

MAP  = '/Game/AshenHollow/Maps/ArenaVillage'

# The Landscape is 1009 x 1009 vertices at 100 uu per quad, imported at the
# origin: 100 800 uu across, so it runs from -50 400 to +50 400 on both axes.
HALF     = 50400.0
# Terrain runs 0 .. 4 900 uu. The bounds reach well under and well over it, so
# navigation covers the valley floors and nothing is clipped off a summit.
FLOOR    = -600.0
CEILING  = 6000.0

# What Scripts/Make-Heightmap.py levelled, and how high it says the ground is.
# X, Y, metres.
SITES = [
    ('CHEGADA',        -8100.0, -41400.0, 11.6),
    ('VALE DO NORTE', -30300.0, -20200.0, 12.4),
    ('PASSO ALTO',     20200.0, -23200.0, 20.5),
    ('FEIRA DO MEIO', -10100.0,   6100.0, 21.9),
    ('CORTE LESTE',    29300.0,  14100.0, 30.0),
    ('PEDREIRA',      -27200.0,  29300.0, 29.9),
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

PROBLEMS = []


def note(message):
    PROBLEMS.append(message)
    unreal.log_warning('AH_FIT %s' % message)


def measure(actor):
    origin, extent = actor.get_actor_bounds(False)
    return origin, extent


levels.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# ── The Landscape has to be there before anything else is touched ───────────
landscapes = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy)]
if not landscapes:
    note('nao ha Landscape no mapa -- importe o heightmap antes de rodar isto')
    unreal.log_warning('AH_FIT ABORTADO')
    raise SystemExit

land = landscapes[0]
_, land_extent = measure(land)
unreal.log_warning('AH_FIT landscape %.0f x %.0f x %.0f uu'
                   % (land_extent.x * 2, land_extent.y * 2, land_extent.z * 2))

# The one number that says whether the heightmap on disk is the heightmap in
# the map. The current terrain is 48.6 m of relief; the one before it was 116.
# Nothing else in this script can tell them apart, and importing a heightmap is
# the one step here that a person has to do by hand -- so it is the step that
# gets skipped, and it did get skipped.
TALL_UU = 6000.0
if land_extent.z * 2 > TALL_UU:
    note('o Landscape tem %.0f m de relevo -- o terreno NOVO tem 48,6 m. '
         'Isso e o heightmap antigo: reimporte Terrain/heightmap_ashen_1km.png '
         'antes de seguir' % (land_extent.z * 2 / 100.0))

# ── Out with the old flat world ─────────────────────────────────────────────
# By label, so nothing else can be caught by accident. The slab is the one that
# matters: a flat collision surface at Z=0 under a kilometre of hills is a
# second floor for Recast to find, and the navmesh would have holes in it
# wherever the real ground dips below zero.
removed = 0
for existing in list(actors.get_all_level_actors()):
    try:
        label = existing.get_actor_label()
    except Exception:                                            # noqa: BLE001
        continue
    if label == 'Arena Ground' or label.startswith('Ground_'):
        try:
            actors.destroy_actor(existing)
            removed += 1
        except Exception as error:                               # noqa: BLE001
            note('nao consegui apagar %s (%s)' % (label, error))
unreal.log_warning('AH_FIT apaguei %d atores do chao antigo' % removed)

# ── Navigation bounds ───────────────────────────────────────────────────────
volumes = [a for a in actors.get_all_level_actors()
           if isinstance(a, unreal.NavMeshBoundsVolume)]
if not volumes:
    volumes = [actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume,
                                             unreal.Vector(0.0, 0.0, 0.0))]
    volumes = [v for v in volumes if v]
    if not volumes:
        note('nao consegui criar o NavMeshBoundsVolume')

for index, volume in enumerate(volumes):
    if index > 0:
        # More than one is how the bounds end up disagreeing with themselves.
        actors.destroy_actor(volume)
        continue
    volume.set_actor_label('Arena Navigation')
    volume.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    volume.set_actor_location(unreal.Vector(0.0, 0.0, (FLOOR + CEILING) * 0.5),
                              False, False)
    _, extent = measure(volume)
    volume.set_actor_scale3d(unreal.Vector(
        HALF / max(extent.x, 1.0),
        HALF / max(extent.y, 1.0),
        (CEILING - FLOOR) * 0.5 / max(extent.z, 1.0)))
    _, grown = measure(volume)
    unreal.log_warning('AH_FIT navegacao %.0f x %.0f x %.0f uu'
                       % (grown.x * 2, grown.y * 2, grown.z * 2))
    if grown.x * 2 < HALF * 1.9:
        note('o volume de navegacao nao cresceu como devia')

# ── Making the navmesh possible at all ──────────────────────────────────────
"""
The log said the same thing for days: `AH_MOVE sem navmesh ... andando no
braco`. The character walks by the blind fallback, cannot path round anything,
and cannot get into a dungeon -- which is what "nao consigo entrar na dungeon"
was. It is not the dungeon. There was no navmesh anywhere, and the AI walking
at walls was the same bug wearing a different coat.

TWO causes, and the first fix only got one of them.

1. Arithmetic. Recast voxelises the world and the defaults are meant for a
   room: a 1000 uu tile over 100 800 uu is 10 000 tiles, and a 19 x 10 uu
   voxel over a 66 m column is hundreds of layers each. Billions of voxels.
   It never finishes and nothing says so.

2. Dynamic generation dirties EVERY tile in the bounds at load. Even with
   sane voxels, sixteen hundred tiles get chewed through in an order nobody
   chose, so the game has a navmesh in arbitrary places and none in others --
   which is exactly what the log showed: `caminho valido parcial` here,
   `sem navmesh` two steps away.

Both are fixed in Config/DefaultEngine.ini, and the hero carries a
NavigationInvokerComponent (AAHGameMode::PlaceHero) so only the tiles near
him are ever built -- about a hundred instead of sixteen hundred.

Why none of it is set from here: cell_size, cell_height and
agent_max_step_height are PROTECTED in the engine Python and cannot be set,
which this script found out the expensive way. See the block below.
"""
recasts = [a for a in actors.get_all_level_actors()
           if a.get_class().get_name().startswith('RecastNavMesh')]
if not recasts:
    note('nao ha RecastNavMesh no mapa -- Build > Build Paths precisa rodar uma vez')

# The values themselves live in Config/DefaultEngine.ini. This block only
# makes the MAP agree with them, and then says out loud what it ended up with.
#
# Two things were learned the expensive way here.
#
# First, cell_size, cell_height and agent_max_step_height are PROTECTED in
# the engine's Python and throw on every attempt -- and they are exactly the
# three that decide whether Recast finishes. The old version set nine
# properties, swallowed three failures, and then printed 'recast ajustado:
# tile 4000, voxel 25x15' regardless. The map went out with the rasterizer
# still at 19 x 10 uu over a sixty-six-metre column and the log had already
# said, twice, that there was no navmesh. Those three now come from the ini,
# which reaches them because the map never had them edited and so has nothing
# serialised over the top.
#
# Second, the six that DO go through are the reason this loop still exists.
# Setting them writes them into the map, and a value serialised in the map
# beats the ini for ever after. So they have to be set to the same numbers
# the ini asks for, or the two disagree and the ini silently loses.
WANTED = (('tile_size_uu', 1800.0), ('agent_radius', 42.0),
          ('agent_height', 192.0), ('agent_max_slope', 44.0),
          ('min_region_area', 400.0), ('merge_region_size', 1600.0))
# Set from the ini only. Reported here so a mismatch is visible instead of
# being a thing nobody ever looks at.
FROM_INI = (('cell_size', 32.0), ('cell_height', 25.0),
            ('agent_max_step_height', 35.0))
for mesh in recasts:
    for name, want in WANTED:
        try:
            mesh.set_editor_property(name, want)
        except Exception as error:                               # noqa: BLE001
            note('nao consegui ajustar %s (%s)' % (name, error))
    try:
        mesh.set_editor_property(
            'runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
    except Exception as error:                                   # noqa: BLE001
        note('nao consegui ajustar runtime_generation (%s)' % error)

    # And read every one of them BACK, including the three nobody here can
    # write. What a script wanted is not evidence; what the map has is.
    got, wrong = [], []
    for name, want in WANTED + FROM_INI:
        try:
            have = mesh.get_editor_property(name)
        except Exception as error:                               # noqa: BLE001
            note('recast nao deu nem pra ler %s (%s)' % (name, error))
            continue
        got.append('%s=%s' % (name, have))
        try:
            if abs(float(have) - want) > 0.51:
                wrong.append('%s: precisa de %g, o mapa tem %g'
                             % (name, want, float(have)))
        except (TypeError, ValueError):
            pass
    unreal.log_warning('AH_FIT recast: %s' % ', '.join(got))
    for line in wrong:
        note('recast %s -- confira Config/DefaultEngine.ini' % line)

# ── Where the player starts ─────────────────────────────────────────────────
# Reading the height out of a trace has now failed twice, with two different
# guesses at the API: `hit.impact_point` does not exist in this build, and
# neither does `SystemLibrary.break_hit_result`. A third guess would be the
# same mistake again.
#
# So: every plausible spelling is TRIED, the first that works is remembered for
# the rest of the run, and if they all fail the log prints what the HitResult
# actually offers. Either the script works or it tells us exactly how to make
# it work -- which is the only acceptable outcome for something that cannot be
# tested anywhere but on Lucas's machine.
_READER = None
_REPORTED = False


def _candidates():
    return [
        ('hit.impact_point',
         lambda h: h.impact_point.z),
        ('hit.get_editor_property("impact_point")',
         lambda h: h.get_editor_property('impact_point').z),
        ('hit.location',
         lambda h: h.location.z),
        ('hit.get_editor_property("location")',
         lambda h: h.get_editor_property('location').z),
        ('GameplayStatics.break_hit_result[5]',
         lambda h: unreal.GameplayStatics.break_hit_result(h)[5].z),
        ('SystemLibrary.break_hit_result[5]',
         lambda h: unreal.SystemLibrary.break_hit_result(h)[5].z),
        ('MathLibrary.break_hit_result[5]',
         lambda h: unreal.MathLibrary.break_hit_result(h)[5].z),
    ]


def _height_of(hit):
    """The Z of a trace's impact, whatever this engine build calls it."""
    global _READER, _REPORTED
    if _READER is not None:
        return _READER(hit)
    for label, reader in _candidates():
        try:
            found = float(reader(hit))
        except Exception:                                        # noqa: BLE001
            continue
        _READER = reader
        unreal.log_warning('AH_FIT altura do traco lida por %s' % label)
        return found
    if not _REPORTED:
        _REPORTED = True
        offers = [name for name in dir(hit) if not name.startswith('_')]
        unreal.log_warning('AH_FIT nenhuma forma conhecida de ler o traco. '
                           'O HitResult oferece: %s' % ', '.join(offers))
    raise AttributeError('nenhum acessor de HitResult funcionou')


def ground_at(x, y):
    """Trace the Landscape. Returns None when the trace cannot be made."""
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -20000.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [],
            unreal.DrawDebugTrace.NONE, True)
        if not hit:
            return None
        return _height_of(hit)
    except Exception as error:                                   # noqa: BLE001
        note('o traco no terreno falhou (%s)' % error)
        return None


# The check that would catch a transposed import: if the heightmap went in with
# its rows and columns the other way round, every one of these is wrong and
# every village in the game is on somebody else's hill.
worst = 0.0
for name, x, y, expected_m in SITES:
    found = ground_at(x, y)
    if found is None:
        unreal.log_warning('AH_SITE %-14s X=%8.0f Y=%8.0f  sem traco' % (name, x, y))
        continue
    off = abs(found / 100.0 - expected_m)
    worst = max(worst, off)
    unreal.log_warning('AH_SITE %-14s X=%8.0f Y=%8.0f  esperado %5.1f m  medido %5.1f m  erro %4.1f m'
                       % (name, x, y, expected_m, found / 100.0, off))
if worst > 4.0:
    note('os sitios nao batem com o heightmap (pior erro %.1f m) -- '
         'o mapa pode ter entrado transposto ou com escala diferente de 100' % worst)

starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
if not starts:
    made = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 0.0))
    starts = [made] if made else []
for index, start in enumerate(starts):
    if index > 0:
        # A second PlayerStart is how the game ends up beginning somewhere the
        # code never heard of. That is exactly what happened before: one was
        # left behind at the old world's coordinates and it won.
        actors.destroy_actor(start)
        continue
    x, y, _e = SITES[0][1], SITES[0][2], SITES[0][3]
    floor = ground_at(x, y)
    z = (floor if floor is not None else 1200.0) + 110.0
    start.set_actor_label('Arena Start')
    start.set_actor_location(unreal.Vector(x, y, z), False, False)
    unreal.log_warning('AH_FIT PlayerStart em (%.0f, %.0f, %.0f)' % (x, y, z))

levels.save_current_level()

if PROBLEMS:
    unreal.log_warning('AH_FIT terminou com %d problema(s):' % len(PROBLEMS))
    for problem in PROBLEMS:
        unreal.log_warning('AH_FIT   - %s' % problem)
else:
    unreal.log_warning('AH_FIT tudo certo. Agora: Build > Build Paths, e salve.')
