#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * The world: how it is laid out, and the cover it gives.
 *
 * It is a GRID, not a scatter. The valley is divided into ten-metre cells;
 * noise decides what each cell is -- pasture, field, wood, houses, ruins,
 * quarry -- neighbour rules stop a single stray cell of anything, roads are
 * carved between the districts, and everything else is placed relative to its
 * cell and to the road rather than at a rolled angle in a rolled spot.
 *
 * That difference is the whole point. Scattering props with rejection sampling
 * can be tuned forever and will never look designed, because nothing in it
 * relates one object to another. A grid does: a house knows which way the
 * street is, a fence knows where the field ends, and an empty cell is empty
 * because the map says so rather than because the dice missed.
 *
 * The map asset keeps only what has to be baked -- the ground, the navigation
 * bounds, the spawn point and the light actors. Everything you can walk into is
 * rolled per encounter from a seed.
 *
 * The cover rule reads the placed list rather than tracing the physics world,
 * for one practical reason: automation tests have no props in their world, and
 * a rule that can only be checked by playing is a rule nobody checks.
 */

/** SRD 5.1 degrees of cover. Total cover cannot be targeted at all. */
enum class EAHCover : uint8 { None, Half, ThreeQuarters, Total };

/**
 * What one ten-metre cell of the valley is.
 *
 * Road and Plaza are carved after the natural types are rolled, so they always
 * win: a street through a village is a street, not a house with a road in it.
 */
enum class EAHCell : uint8 { Meadow, Field, Wood, Village, Ruins, Quarry, Road, Plaza,
                             /** River or lake. Nothing is built here and nobody wades it. */
                             Water,
                             /** Where a road crosses the water: river below, planks above. */
                             Bridge,
                             Count };

/**
 * A place worth walking to.
 *
 * Ninety per cent of a kilometre of Landscape has nothing built on it, which
 * was the right answer for mobility and the wrong one for everything else: an
 * open world with nothing in it is a field. Landmarks are the answer -- small
 * hand-designed sets scattered across the empty ninety per cent, each one
 * something you can see from a distance, walk to, and find something at.
 *
 * They are chosen in the same early pass as the camps, so everything placed
 * afterwards checks against them, and most of them carry their own fight.
 */
enum class EAHSite : uint8
{
    Acampamento,   ///< bandit camp: tents round a fire behind a palisade
    Circulo,       ///< a ring of standing stones about an altar
    Ruina,         ///< a broken tower, its walls and a gateway arch
    Cemiterio,     ///< graves behind a low wall
    Posto,         ///< an abandoned watchpost, empty and worth looting
    Forte,         ///< a held compound: palisade, gate, keep. The hard one.
    Lenhadores,    ///< a logging camp. Nobody hostile, but somebody lives here
    Pedreira,      ///< a working face, carts and cut stone
    Masmorra,      ///< rooms, corridors and a boss at the far end
    Count
};

/**
 * Where a hostile group stands, and what kind of place it is standing in.
 *
 * It used to be a bare FVector and the game mode worked out the rest by
 * matching positions against the landmark list. Two things forced a struct:
 * the size belongs with the place that decided it, and a camp INSIDE a
 * dungeon plays by different rules -- outdoors a fight needs six and a half
 * metres of clear ground around it, and in a room four metres across the
 * walls ARE the design.
 */
struct FAHCampSpot
{
    FVector Where  = FVector::ZeroVector;
    /** How many hostiles. Never zero: a camp with nobody in it is not a camp. */
    int32   Foes   = 1;
    /** Inside a dungeon, where walls close by are the point. */
    bool    bIndoor = false;
    int32 WildlifeKind = 0; // 0 bandits, 1 wolves, 2 bear
};

struct FAHLandmark
{
    FVector Where = FVector::ZeroVector;
    EAHSite Kind  = EAHSite::Acampamento;
    /** How many hostiles belong here. Zero is a place, not an encounter. */
    int32   Foes  = 0;
};

/**
 * The errand: somebody to talk to, somewhere to go, something to bring back.
 *
 * A world full of places is still not a world full of REASONS. Nineteen
 * landmarks and twenty-eight camps give the player somewhere to walk; none of
 * them gives him a motive to walk THERE rather than anywhere else, and a
 * dungeon you happen across is a room with monsters in it, while a dungeon
 * somebody asked you to enter is a story.
 *
 * So one thread, rolled with the map so it can never point at a place that is
 * not there: a carter waiting near the arrival road, his pack in the last
 * room of the nearest dungeon, and a walk of two hundred-odd metres between
 * them. Every field here is decided by the generator and proved by the
 * harness, because a quest marker pointing at a wall is worse than no quest.
 */
struct FAHErrand
{
    /** False when the roll could not honestly place one. Nothing is spawned. */
    bool    bValid   = false;
    /** Where the man stands, on clear ground beside the road out of spawn. */
    FVector Giver    = FVector::ZeroVector;
    /** Yaw that has him looking at the road rather than into a hedge. */
    float   GiverYaw = 0.f;
    /** The dungeon's doorway: what the marker on the minimap points at. */
    FVector Door     = FVector::ZeroVector;
    /** The pack itself, in the boss room. Walk onto it and the errand is done. */
    FVector Prize    = FVector::ZeroVector;
    /** Index into Landmarks of the dungeon the pack is in, or INDEX_NONE. */
    int32   Dungeon  = INDEX_NONE;
};

/**
 * What somebody in the valley does for a living.
 *
 * It decides his name, what he says, how far he strays from where he was put,
 * and nothing else. Trades rather than personalities on purpose: a woodcutter
 * standing among stumps with an axe reads as a woodcutter from thirty metres
 * up, which is the only distance this camera ever looks from.
 */
enum class EAHFolk : uint8
{
    Aldeao,      ///< lives in one of the houses; strolls his own street
    Mercador,    ///< stands at a stall on the square
    Lenhador,    ///< the logging camp
    Pedreiro,    ///< the quarry
    Guarda,      ///< stands a post and does not wander
    Carroceiro,  ///< the one with the errand
    Count
};

/**
 * Somebody alive who is not trying to kill you.
 *
 * The valley had nineteen landmarks, twenty-eight camps, four thousand props
 * and not one person in it who was not a fight. Lucas's words: "quero deixar o
 * mapa com vida". A world where every living thing attacks on sight is not a
 * world, it is a shooting range with scenery.
 *
 * These are decided by the generator, exactly like the camps, so the harness
 * can prove that nobody is standing inside a wall or off the Landscape, and so
 * the same seed puts the same woman outside the same house. The game mode
 * builds the ones you are near and forgets the rest, same as the camps.
 */
struct FAHFolkSpot
{
    FVector Where = FVector::ZeroVector;
    /** Which way he is looking when you find him: at the road, at his work. */
    float   Yaw   = 0.f;
    EAHFolk Trade = EAHFolk::Aldeao;
    /**
     * How far he may stray from Where, in centimetres. Zero for somebody
     * standing a post: a guard who wanders is not a guard.
     */
    float   Range = 600.f;
    /** True for the one who hands out the errand. At most one per valley. */
    bool    bGiver = false;
};

/** What kind of animal. Their behaviour is in AHBeast.cpp, not here. */
enum class EAHBeast : uint8
{
    Galinha,   ///< pecks about the yards of a village
    Veado,     ///< grazes the meadows and bolts when you come near
    Corvo,     ///< circles over the woods and the ruins, never lands
    Count
};

/**
 * An animal, and the patch of world it belongs to.
 *
 * Deliberately not characters: an animal has no sheet, no turn, no navmesh
 * query and no skeleton. It is a static mesh that moves, which is what makes
 * it affordable to have forty of them -- and movement is most of what tells
 * the eye a thing is alive.
 */
struct FAHBeastSpot
{
    FVector  Where = FVector::ZeroVector;
    EAHBeast Kind  = EAHBeast::Galinha;
    /** How far it strays. A crow's is its circle; a deer's is its pasture. */
    float    Range = 400.f;
};

/**
 * One placed thing.
 *
 * MeshPath may name a static mesh or a blueprint; the game mode works out which.
 * Radius and TopZ start as the generator's estimate and are replaced with the
 * real measured bounds once the thing exists, so the cover maths runs against
 * the prop's actual size and never against a number somebody typed.
 */
struct FAHArenaPiece
{
    FString  MeshPath;
    FVector  Location     = FVector::ZeroVector;
    FRotator Rotation     = FRotator::ZeroRotator;
    FVector  Scale        = FVector::OneVector;
    float    Radius       = 60.f;
    /**
     * World height of this piece's top surface -- not its height above its own
     * origin. A deck's origin is its middle, so "origin plus height" overshot
     * the top by half; where the top is has one meaning and the game mode
     * measures it.
     */
    float    TopZ         = 90.f;
    /** Counts towards cover. False for ramps, ground decoration and grass. */
    bool     bCover       = true;
    /** Dropped so its base rests on the floor. False for boxes placed by maths. */
    bool     bSitOnGround = true;
    /** A warm point light is spawned here too (braziers, campfires, lamps). */
    bool     bLight       = false;
    /**
     * Paving: a flat sheet lying on the ground. Collision is switched off, and
     * with it any effect on the navmesh -- a road must not be a step you have to
     * path around, and a hundred little slabs with collision is exactly the kind
     * of seam Recast falls over.
     */
    bool     bFlat        = false;
    /**
     * Collision without a model: the blocker that keeps you out of the river.
     *
     * The water itself is a flat sheet with no collision, because a sheet you
     * can stand on is a floor. What stops you walking into the river is an
     * unseen box in the same cell -- and it deliberately ignores the visibility
     * channel, so a river never counts as cover and never blocks an arrow.
     */
    bool     bInvisible   = false;
    /** A chimney, a bonfire: spawns the pack's smoke plume here. */
    bool     bSmoke       = false;
    /**
     * The errand's pack: the one piece in the valley that can be picked up.
     *
     * It is spawned as its own actor rather than folded into an instanced
     * batch, for one reason: an instance cannot be taken away. Removing one
     * instance out of a batch means rebuilding the batch, and a quest item
     * that stays lying there after you have collected it is a quest item
     * nobody believes they collected.
     */
    bool     bPrize       = false;
    /** Keep the asset's own material rather than overriding it. */
    FString  MaterialPath;
};

/** A whole valley, rolled together so its districts read as one landscape. */
struct FAHArenaPlan
{
    FString Name;                          // shown in the HUD
    /** GridSide * GridSide cells, indexed Cx * GridSide + Cy. */
    TArray<EAHCell>       Cells;
    /**
     * Where hostile groups stand. The game mode spawns one to three foes at each
     * and keeps them asleep until the player comes near, which is what turns a
     * single arena into somewhere to walk around.
     */
    TArray<FAHCampSpot>   Camps;
    /**
     * Everything worth walking to that is not one of the six settlements.
     *
     * Read by the game mode for the camps that belong to them, and by the
     * harness, which checks that they are on flat ground, far enough apart to
     * read as separate places, and never on top of a village or a road.
     */
    TArray<FAHLandmark>   Landmarks;
    /** The one errand this valley carries. See FAHErrand. */
    FAHErrand             Errand;
    /**
     * Everybody in the valley who is not a fight. See FAHFolkSpot.
     *
     * The giver of the errand is the first entry when there is one, so the
     * game mode can find him without searching for a flag.
     */
    TArray<FAHFolkSpot>   Folk;
    /** The animals. See FAHBeastSpot. */
    TArray<FAHBeastSpot>  Beasts;
    TArray<FAHArenaPiece> Pieces;
    /**
     * Which edges of this comarca have a road running out of them.
     *
     * Values are side indices in the +X, +Y, -X, -Y order the generator uses
     * everywhere. Walking onto one of these is how you leave for the neighbour,
     * so they are not decoration: the game mode reads this list every frame.
     */
    TArray<int32>         Gates;
    // Light varies with the valley, so two encounters are not the same hour of
    // the same day. Far cheaper than new geometry and it changes the mood more.
    float SunPitch       = -34.f;
    float SunYaw         = -55.f;
    float SunTemperature = 5200.f;
    float SkyIntensity   = 1.7f;

    /**
     * The ground, as a height per sub-tile: HeightSide * HeightSide of them,
     * indexed Tx * HeightSide + Ty, in centimetres above the valley's floor.
     *
     * The world used to be one flat slab, and no amount of scenery fixed that
     * -- flat ground has no shape to read, so a valley full of props still
     * looked like props on a table. The relief comes from the same noise the
     * cell types come from, then gets clamped so no two neighbouring tiles
     * differ by more than MaxStep: that single constraint is what guarantees
     * the whole map stays walkable and the navmesh stays in one piece, and it
     * is what turns noise into terraced hillsides rather than cliffs.
     */
    TArray<float> Heights;

    /** The cell at these coordinates, or Meadow for anything off the map. */
    EAHCell At(int32 Cx, int32 Cy) const;
    /** Height of one sub-tile, clamped to the map at the edges. */
    float   HeightAt(int32 Tx, int32 Ty) const;
    /** Height of the ground under a world position. */
    float   GroundAt(const FVector& Where) const;
};

namespace AHArena
{
    /**
     * Cells per side. A hundred of them, ten metres each, so the grid covers
     * the whole kilometre of Landscape.
     *
     * The grid is no longer the world -- the Landscape is. This is the lattice
     * the generator reasons on: which cells are a village, where a road runs,
     * which cell is too steep to build on. Most of it stays empty on purpose,
     * because the ground between the settlements is scenery that already
     * exists and does not need props scattered over it.
     */
    static constexpr int32 GridSide = 100;

    /** Ten metres. Big enough for a house and its yard, small enough that a
     *  fight spills across three or four of them. */
    static constexpr float CellSize = 1000.f;

    /** Half-width of the walkable world in centimetres: 250 x 250 metres. */
    static constexpr float PlayHalfSize = GridSide * CellSize * .5f;

    /**
     * Ground tiles per cell side, so the terrain can bend inside a cell.
     *
     * Three gave 7.5 m of relief and 5625 colliding boxes; two gives 5.8 m and
     * 2500. Backed off deliberately: the editor was already warning that
     * skeletal meshes needed more memory to compile than it had, and the
     * navmesh has to be rebuilt over every one of these every time the valley
     * is rolled. Relief is worth having; relief nobody can walk on is not.
     */
    static constexpr int32 SubSteps   = 2;
    static constexpr float TileSize   = CellSize / SubSteps;      // 5 m
    static constexpr int32 HeightSide = GridSide * SubSteps;      // 50

    /**
     * The most two neighbouring ground tiles may differ, in centimetres.
     *
     * This is the number the whole terrain hangs on. Recast's walking agent
     * climbs 35 cm and the character movement component climbs 45, so 30 is
     * under both with room to spare -- which means every tile on the map is
     * reachable from every other one and the navmesh cannot come out in
     * islands. Raise it and the world gets more dramatic and starts trapping
     * people behind ledges they cannot climb.
     */
    static constexpr float MaxStep     = 30.f;
    /** Gentler along a road, so paving never buries itself in a step. */
    static constexpr float MaxRoadStep = 10.f;
    /** What the noise asks for before the step limit shapes it. */
    static constexpr float MaxRelief   = 1400.f;
    /** How far the riverbed is cut below its banks. */
    static constexpr float RiverDepth  = 220.f;

    /**
     * The six places the heightmap guarantees are level.
     *
     * They are not rolled. Scripts/Make-Heightmap.py eases a shelf flat at
     * each of these spots and MEASURES that every one of them is reachable on
     * foot from the arrival apron before it writes the map out, so the
     * generator can put a village on any of them and know the ground is there.
     * A settlement on a hillside is the "estruturas bugadas" complaint, and
     * this is the half of the answer that comes from the terrain rather than
     * from the placement rules.
     *
     * X, Y in world units, matching the Landscape imported at 0,0,0 with scale
     * 100 -- so these are the coordinates the editor shows.
     */
    struct FAHSite { const TCHAR* Name; float X; float Y; EAHCell Kind; };
    static constexpr int32 SiteCount = 6;
    const FAHSite& Site(int32 Which);

    /** Where the player arrives: the first of the sites. */
    static constexpr float SpawnX = -8100.f;
    static constexpr float SpawnY = -41400.f;
    /** The same spot as a cell, which is what most of the generator asks for. */
    static constexpr int32 SpawnCellX = static_cast<int32>((SpawnX + PlayHalfSize) / CellSize);
    static constexpr int32 SpawnCellY = static_cast<int32>((SpawnY + PlayHalfSize) / CellSize);

    /** Width of a carriageway. Two people can pass; it still reads as a road. */
    static constexpr float RoadWidth = 520.f;

    /**
     * How far every house on a street stands from that street's centreline.
     *
     * It was 760, which put the near wall of a big house about a metre from
     * the carriageway and the wall of a very big one THROUGH it -- "as casas
     * aparecendo no meio do caminho". The line was measured to the house's
     * middle and the house's own depth was never in the sum. A thousand is one
     * whole cell, so a house sits in the middle of its own lot with three or
     * four metres of verge in front of it and its back inside its own fence.
     */
    static constexpr float Frontage = 1000.f;

    /** No scenery within this of where the player starts. */
    /**
     * Half the Landscape, in unreal units.
     *
     * The playable grid is 100 cells of 10 m, so it ends at 50 000; the
     * Landscape in the map is 100 800 uu across, so it ends at 50 400. Those
     * four hundred units of difference are a real place -- the rim is built
     * in them -- and anything placed beyond them is standing over nothing.
     */
    static constexpr double LandscapeEdge = 50400.0;

    static constexpr float SpawnClearance = 700.f;

    /** A camp needs room around it, because a fight will happen there. */
    static constexpr float CampClearance = 650.f;

    /** Feet-to-eye height used when asking whether an obstacle hides a target. */
    static constexpr float BodyHeight = 180.f;

    /** A shooter this far above a target shoots down on it. Above head height,
     *  so standing on a crate is not enough; the raised deck is. */
    static constexpr float HighGroundStep = 100.f;

    /** Walking surface of the raised deck. */
    static constexpr float DeckTop = 130.f;

    /**
     * Steepest ground anything may be built on, as a rise in centimetres over
     * the ten metres of one cell.
     *
     * Measured from the Landscape itself rather than from a height the
     * generator made up. Two hundred over a thousand is about eleven degrees:
     * gentle enough that a house sits on it without a corner in the air and a
     * street reads as a street.
     */
    static constexpr float BuildableRise = 200.f;

    /**
     * How many things the valley may contain.
     *
     * It used to be 760, because every piece was its own actor with its own
     * draw call. Measuring killed that assumption: a comarca of 616 pieces uses
     * 37 DISTINCT meshes, so 96% of it is the same handful of models over and
     * over. Instanced, the count stops mattering -- three thousand props are
     * still forty components -- and the number that was holding the world small
     * turned out to be holding nothing at all.
     */
    static constexpr int32 PieceBudget = 11000;

    /**
     * How many people and how many animals a valley may hold.
     *
     * Neither number is a hardware limit -- what is spawned at any moment is
     * whatever is near the player, and that is capped separately in the game
     * mode. These cap the DESCRIPTION, so a world with eleven villages cannot
     * quietly end up with a hundred strollers to walk over every frame.
     *
     * Twenty-eight is about one person per settlement plus a pair at each
     * working camp; forty-four animals is roughly one bird or one hen per
     * second of walking, which is the rate at which a place stops feeling
     * abandoned without becoming a farmyard.
     */
    static constexpr int32 FolkBudget  = 28;
    static constexpr int32 BeastBudget = 44;

    /**
     * How much elbow room a person needs where he stands.
     *
     * Smaller than a prop's, because a person is 84 cm across and the navmesh
     * agent that has to reach him is 42 -- but not zero: two villagers rolled
     * into the same spot would spend the game pushing each other, and a
     * villager placed where a barrel is about to go is a villager inside a
     * barrel.
     */
    static constexpr double FolkRoom = 150.0;

    /**
     * The gate cell on one side: always the middle of that edge.
     *
     * Fixed rather than wherever a road happened to reach, because the arrival
     * point in the NEXT comarca has to be known before that comarca is
     * generated -- and a player who learns "the way out is the middle of the
     * edge" can navigate a world they have never seen.
     */
    static constexpr int32 GateCell = GridSide / 2;
    void    GateCellOf(int32 Side, int32& OutX, int32& OutY);
    /** Where you stand when you arrive through, or leave by, that gate. */
    FVector GatePoint(int32 Side);
    /** The side you come out of, having gone in by Side. */
    inline int32 OppositeSide(int32 Side) { return (Side + 2) % 4; }

    /** World position of the middle of a cell. */
    FVector CellCentre(int32 Cx, int32 Cy);
    /** Which cell a world position falls in, clamped to the map. */
    void    CellOf(const FVector& Where, int32& OutX, int32& OutY);
    /** True while both coordinates are on the map. */
    bool    InGrid(int32 Cx, int32 Cy);

    int32        ArmorBonus(EAHCover Cover);
    const TCHAR* CoverName(EAHCover Cover);
    const TCHAR* CellName(EAHCell Cell);

    /**
     * Cover the target at To has from a shooter at From. Both are feet positions.
     * Only obstacles genuinely between the two count, and an obstacle is measured
     * against the TARGET's feet: a crate that hides someone standing on the
     * ground hides nobody standing on the raised deck.
     *
     * Caps at ThreeQuarters on purpose. Whether a shot is blocked outright is
     * already decided by the line-of-sight trace in TryRangedAttack, which also
     * sees walls and houses that were never in this list; two systems answering
     * the same question is how they end up disagreeing.
     */
    EAHCover CoverBetween(const TArray<FAHArenaPiece>& Pieces, const FVector& From, const FVector& To);

    /** True when the shooter is high enough above the target for advantage. */
    bool HasHighGround(const FVector& From, const FVector& To);

    /**
     * Builds one whole valley.
     *
     * Deterministic in the seed. Measure is asked for a mesh's half-extent and
     * must answer for any path the generator names: a fence can only be tiled
     * without gaps or overlaps by someone who knows how long a panel actually
     * is, and that is a question about the asset, not a constant.
     *
     * Guarantees, all of them checked by the automation test: every district is
     * reachable from the arrival cell along the roads, nothing solid stands on
     * the arrival point, and every camp keeps a clear ring to fight in.
     */
    /**
     * Builds the world that stands ON the Landscape.
     *
     * Ground answers "how high is the terrain at this X and Y", in world units,
     * and it is the whole reason this rewrite happened. The generator used to
     * invent its own ground -- first a flat slab, then two and a half thousand
     * boxes spawned at runtime -- and a navmesh cannot be baked over geometry
     * that does not exist until the game runs. So the terrain moved into the
     * map as a real Landscape, navigation is baked over it once, and the
     * generator asks it how high things are instead of deciding.
     *
     * In the game Ground is a line trace against the Landscape. In the tests
     * and the offline harness it is the same heightmap the Landscape was built
     * from, read from a file -- so what the harness checks is the terrain the
     * player actually walks on, not an imitation of it.
     *
     * Deterministic in the seed. Measure is asked for a mesh's half-extent and
     * must answer for any path the generator names.
     */
    FAHArenaPlan Build(FRandomStream& Dice, const FVector& HeroSpawn,
                       TFunctionRef<FVector(const FString&)> Measure,
                       TFunctionRef<float(float, float)> Ground,
                       int32 GateMask = 0);
}
