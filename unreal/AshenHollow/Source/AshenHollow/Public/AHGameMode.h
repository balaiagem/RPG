#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AHArena.h"
#include "AHGameMode.generated.h"

/**
 * A group of foes that sleeps, wakes and fights together.
 *
 * Camps are what turn one arena into somewhere to walk around: they sit still
 * until the player comes near, and only the group that was roused joins the
 * fight, so the rest of the world stays where it is.
 */
struct FAHCamp
{
    TArray<TWeakObjectPtr<class AAHCharacter>> Foes;
    FVector Centre   = FVector::ZeroVector;
    float   Alert    = 1100.f;
    bool    bCleared = false;
    bool    bIndoor = false;
    int32 WildlifeKind = 0;
    /**
     * How many foes belong here, and the seed that decides who they are.
     *
     * A camp is now a DESCRIPTION rather than a set of actors. The world has
     * about twenty of them -- it used to have five -- and spawning every one
     * at load would put sixty skeletal meshes with animation instances and
     * equipment rigs on a 6 GB card, which is a slideshow. So the actors are
     * made when you come near and taken away when you leave, and the seed is
     * what makes the same camp hold the same three bandits each time.
     */
    int32 Many = 1;
    int32 Seed = 0;
    /** Whether its actors exist right now. */
    bool  bAwake = false;
    /**
     * Whether anybody has ever fought here.
     *
     * A camp that has been fought is never put back to sleep, because waking
     * it again would rebuild its dead at full health -- and a player who
     * retreats from a hard fight would come back to find it undone.
     */
    bool  bTouched = false;
};

struct FAHSideQuest
{
    FString Title, Objective;
    FVector Giver=FVector::ZeroVector, Target=FVector::ZeroVector;
    int32 CampIndex=INDEX_NONE, Stage=0, RewardXP=150;
    FName RewardItem=NAME_None;
    bool bSurvey=false;
};

struct FAHLoot
{
    FVector Where=FVector::ZeroVector;
    int32 Gold=0;
    FString Item;
    bool bTaken=false;
    TWeakObjectPtr<AActor> Marker;
};

UCLASS()
class ASHENHOLLOW_API AAHGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAHGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY() TArray<TObjectPtr<class AAHCharacter>> Order;
    int32 Round = 1;
    int32 ActiveIndex = 0;
    bool bStarted = false;
    bool bFinished = false;
    float TurnStarted = 0.f;
    AAHCharacter* ActiveCharacter() const;

    // ── Arena ─────────────────────────────────────────────────────────────────
    /** Where the player appears in the world. */
    static const FVector HeroSpawn;
    /**
     * Stands the player on the arrival point, at the height the ground
     * actually is there.
     *
     * The game mode does this itself rather than trusting the map's
     * PlayerStart, and that is not belt and braces -- the PlayerStart was
     * baked at (0, -6000) when the valley was 130 m across, the valley grew to
     * 250 m, and the PlayerStart stayed where it was. On flat ground nobody
     * could tell: the player simply started sixty metres from where every
     * clearance rule thought he was. The moment the ground had hills he
     * started INSIDE one and could not move.
     *
     * A baked coordinate that has to agree with a constant in the code is a
     * promise nothing checks. This removes the promise.
     */
    /**
     * How high the Landscape is at a spot, by tracing it.
     *
     * The single source of truth for where the ground is. The generator used
     * to decide it, then a field of boxes was spawned to match, and the two
     * drifted -- which is how things ended up buried and how a navmesh over
     * runtime geometry never got baked at all. There is one answer now and
     * this is it.
     */
    float TerrainZ(float X, float Y, bool* bFound = nullptr) const;
    /**
     * The Landscape actor, found once at BeginPlay and never guessed again.
     *
     * TerrainZ has to answer "how high is the GROUND", and for weeks it
     * answered "how high is the first thing a downward ray hits" -- which is a
     * roof, a palisade, a cart or a tent whenever one is in the way. Every
     * caller of it was therefore quietly wrong on top of any building: foes
     * spawned on roofs (where there is no navmesh, so they stood still all
     * fight), and each prop was traced onto whatever prop had been spawned
     * before it, so the dressing climbed the buildings.
     *
     * With the Landscape known, the trace can pick the hit that is actually
     * the ground out of the whole list. Found by class name rather than by
     * including the Landscape module, because a new module dependency is a
     * build risk and this is a pointer comparison.
     */
    UPROPERTY() TObjectPtr<AActor> GroundActor;
    void FindGround();
    /**
     * The highest ground under a piece's whole footprint, not under its pivot.
     *
     * A house is eight metres across and the pivot is one point in the middle
     * of it. On any slope at all, pinning that point to the ground drives the
     * uphill half of the building INTO the hill -- which is what Lucas saw as
     * "casas que aparecem embaixo da terra", and which no amount of correcting
     * the pivot height can fix, because the error is the width of the thing.
     *
     * Taking the HIGHEST of the footprint is the asymmetric choice on purpose:
     * a wall floating a few centimetres over the grass on its downhill corner
     * is a seam, and a wall buried in the hillside is a broken building.
     */
    float FootprintZ(const FVector& Where, float Radius, bool* bFound = nullptr) const;
    void PlaceHero();
    /**
     * Makes sure there is a navmesh, and says what it found.
     *
     * Every step the player takes is a navmesh query, so no navmesh is a game
     * where the character stands still and nothing anywhere says why. The
     * editor prints one warning at load -- "Recreating dtNavMesh instance ...
     * serialized maxTiles 588 vs calculated required 1176" -- which means the
     * baked navmesh was thrown away because the valley outgrew it. Whether
     * anything rebuilt it afterwards depends on a property serialised into the
     * map, not on the ini, so it is asked rather than assumed.
     */
    void EnsureNavigation();
    /**
     * Spawns the camps you are near and removes the ones you have left.
     *
     * The open world's answer to "more enemies": twenty camps exist, at most
     * a handful are made of actors at any moment. Called once per second from
     * the exploring tick, never during a fight.
     */
    void StreamCamps();
    /** Builds one camp's foes from its own seed. Returns how many stood up. */
    int32 WakeCamp(FAHCamp& Camp);
    /** Removes one camp's foes. Returns how many were taken away. */
    int32 SleepCamp(FAHCamp& Camp);
    /** How near you have to be for a camp to exist, and how far to forget it. */
    static constexpr float WakeRange  = 16000.f;
    static constexpr float SleepRange = 26000.f;
    /**
     * The most foes that may exist at once, anywhere.
     *
     * Not a design choice, a hardware one: each one is a skeletal mesh with an
     * animation instance and an equipment rig. This is the number that used to
     * cap the whole world; now it caps only what is awake, which is what lets
     * the world hold twenty camps instead of five.
     */
    static constexpr int32 LiveFoes = 18;
    /** Seconds between streaming passes. Cheap, but not free, and not urgent. */
    float NextStream = 0.f;

    // ── The living ────────────────────────────────────────────────────────────
    /**
     * The people and the animals that are standing up right now.
     *
     * Streamed exactly like the camps and for the same reason: the generator
     * describes twenty-odd villagers and forty-odd animals, and a villager is
     * a skeletal mesh with an animation instance. What differs is that these
     * are not a threat, so the ranges are shorter -- you have to be able to SEE
     * a village to want it to have people in it, and you cannot see one from
     * two hundred metres.
     */
    UPROPERTY() TArray<TObjectPtr<class AAHVillager>> Folk;
    UPROPERTY() TArray<TObjectPtr<class AAHBeast>>    Beasts;
    /** One flag per entry of Plan.Folk / Plan.Beasts: is that one standing? */
    TArray<bool> FolkUp, BeastUp;
    /** Builds the people and animals you are near, forgets the rest. */
    void StreamLiving();
    static constexpr float LivingWake  = 15000.f;
    static constexpr float LivingSleep = 23000.f;
    /** How many may exist at once. A villager costs what a foe costs. */
    static constexpr int32 LiveFolk   = 10;
    static constexpr int32 LiveBeasts = 16;

    // ── The errand ────────────────────────────────────────────────────────────
    /**
     * How far along the one quest in the game is.
     *
     * 0 nobody has spoken to the carter, 1 he has asked, 2 the pack is in hand,
     * 3 it is back with him. Kept on the game mode rather than on the carter
     * because he is STREAMED: walk away and he is destroyed, walk back and a
     * new actor is built from the same seed, and a quest whose state lived on
     * him would reset itself every time the player went to do it.
     */
    int32 ErrandStage = 0;
    /** The pack, as its own actor, so it can be taken away when it is taken. */
    UPROPERTY() TObjectPtr<AActor> PrizeActor;
    bool bPrizeTaken = false;
    /** Picks the pack up: called from the exploring tick when you walk onto it. */
    void TakePrize();
    /** The one line the HUD writes about the errand, or empty when there is none. */
    FString ErrandLine() const;
    TArray<FAHSideQuest> SideQuests;
    TArray<FAHLoot> Loot;
    void DropLoot(class AAHCharacter* Foe);
    bool TakeNearbyLoot(class AAHCharacter* Hero);
    void InitializeSideQuests();
    void UpdateSideQuests();
    bool TalkSideQuest(class AAHVillager* Villager, class AAHCharacter* Hero);
    /** Whoever the player is talking to right now, and until when. */
    TWeakObjectPtr<class AAHVillager> TalkingTo;
    float TalkUntil = 0.f;
    /** Starts a conversation with the nearest villager. False when none is near. */
    bool TalkToNearest();
    /** Cleared on a rebuild; set once the pawn has been stood on the ground. */
    bool bHeroPlaced = false;
    /** Last AH_READY state written, so the gate is logged on change only. */
    int32 LastGateLogged = -1;
    /** Cleared on a rebuild; the navmesh is checked again once play begins. */
    bool bNavChecked = false;
    /**
     * Whether the navmesh has ever answered yes under the player, and when to
     * ask again.
     *
     * The one-shot check was an alarm that could only be false. It ran three
     * MILLISECONDS after the hero became a navigation invoker, so Recast had
     * not built a single tile yet, and it printed `ponto de chegada NAO esta
     * na malha` on a world whose navmesh was about to be perfectly fine. Two
     * rounds of debugging went at that line. A check that runs before the
     * thing it checks can possibly be true is worse than no check.
     */
    bool  bNavConfirmed = false;
    float NextNavPoll   = 0.f;
    float NavWaitedFor  = 0.f;

    /**
     * Where to go next, in words: the nearest dungeon and the nearest fight,
     * each as a distance and a compass point from where the player is standing.
     *
     * Eighteen camps and sixteen landmarks over a kilometre means you can walk
     * for two minutes and meet nothing, which from inside the game is
     * indistinguishable from "the enemies do not spawn" and "I cannot get
     * into the dungeon". Both of those were reported as bugs and neither was
     * one. A world this size needs to tell you where its content is.
     */
    void WhereTo(FString& OutDungeon, FString& OutFight) const;

    // ── Exploring and fighting ────────────────────────────────────────────────
    /**
     * The game has two phases now. Exploring is free movement with the foes
     * asleep; fighting is the turn order, exactly as it always was. Everything
     * that used to assume the fight had already started reads bExploring first.
     */
    bool bExploring = true;
    /** Index into Camps of the fight in progress, or INDEX_NONE. */
    int32 Fighting = INDEX_NONE;
    /**
     * EVERY camp taking part in the fight in progress, not just the one that
     * started it.
     *
     * Camps are placed nine cells apart as landmarks, but the loose ones and a
     * landmark's own can end up within a few metres of each other, and a fight
     * that rouses exactly one of them looks broken from inside the game:
     * Lucas's words, "alguns nao se alertam mesmo se um inimigo for ferido do
     * lado dele". A bandit watching the man next to him get shot and doing
     * nothing is not a difficulty setting, it is a bug.
     */
    TArray<int32> FightingCamps;
    /** How near a fight somebody has to be to notice it and join in. */
    static constexpr float JoinRange = 300.f;
    /** The most foes one fight may hold. A merge must not become a siege. */
    static constexpr int32 FightCap = 12;
    /**
     * Pulls in anybody standing near the fight who is not in it yet.
     *
     * Called once a second while fighting, not only when it starts, because
     * the fight MOVES: the player backs off, the foes chase, and two rounds
     * later the brawl is next to a camp that was thirty metres away when the
     * initiative was rolled. Joiners are appended to the end of the order
     * rather than sorted in, so the round in progress is not reshuffled under
     * whoever is acting.
     */
    void DrawInBystanders(class AAHCharacter* NoiseSource = nullptr);
    bool CanWitness(const class AAHCharacter* Observer, const class AAHCharacter* Source, float Range=JoinRange) const;
    bool NoticesHero(class AAHCharacter* Observer, class AAHCharacter* Hero, float Range);
    TMap<TWeakObjectPtr<class AAHCharacter>,float> PerceptionChecks;
    float NextJoinCheck = 0.f;
    TArray<FAHCamp> Camps;
    bool IsExploring() const { return bExploring; }
    /**
     * Rouses one camp and rolls initiative over it and the player.
     *
     * bAmbush is true when the fight was started FROM stealth, by the player
     * picking a foe rather than by walking into the camp's notice range. It buys
     * a surprise round and advantage on the opening attack, which is the whole
     * reward for having crept up in the first place.
     */
    void BeginCombat(int32 CampIndex, bool bAmbush = false, class AAHCharacter* Trigger = nullptr);
    /** Round one of an ambush: the camp is caught out and does not act. */
    bool bSurprise = false;
    /** Ends the fight and hands the world back. */
    void EndCombat();
    /** Starts the fight that Foe belongs to. False when it is already running. */
    bool EngageWith(class AAHCharacter* Foe);
    /** True when every camp in the world has been put down. */
    bool IsWorldCleared() const;
    /** The whole encounter space, re-rolled from a new seed each time. */
    FAHArenaPlan Plan;
    UPROPERTY() TArray<TObjectPtr<AActor>> ObstacleActors;
    /** Clears the old props and lays out a fresh world from Seed. */
    void BuildArena(int32 Seed);
    /**
     * A seed that genuinely differs between launches.
     *
     * FMath::Rand() forwards to rand(), which replays the same sequence from the
     * same process start unless something called srand() first -- so seeding from
     * it would have built the identical arena every single time the game opened,
     * which is the exact opposite of the promise.
     */
    static int32 FreshSeed();
    /** Seed behind the layout on screen. Shown in the HUD so "procedural" is
     *  something you can watch change, rather than something you take on faith. */
    int32 ArenaSeed = 0;
    /** Points the map's baked light actors at whatever hour this arena rolled. */
    void ApplyArenaLight();
    double WorldMinutes=540.0;
    float NextClockLight=0.f;
    float DaylightIntensity=-1.f;
    bool IsInTown(const FVector& Location) const;
    FString RestRefusal(bool bLong) const;
    bool RequestRest(bool bLong);
    void TickWorldClock(float DeltaSeconds);
    FString ClockLabel() const;
    /** Cover the target standing at To has against a shooter standing at From. */
    EAHCover CoverBetween(const FVector& From, const FVector& To) const;

    bool NextEncounter();
    bool EndTurn(AAHCharacter* Requester);
};
