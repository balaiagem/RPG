#include "AHGameMode.h"
#include "AHWildEnemy.h"
#include "AHKitTable.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHCombatHUD.h"
#include "AHVillager.h"
#include "AHBeast.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PointLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavigationInvokerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
// The middle of the bottom row of cells, standing on the main road, looking up
// it. AHArena::CellCentre(SpawnCellX, SpawnCellY) is this same point, and the
// PlayerStart in the map has to agree with it: the generator keeps this spot
// clear, and the pawn actually appears wherever the PlayerStart is.
// The arrival shelf, the first of the six the heightmap levels into the
// Landscape. Nothing in the map has to agree with this any more: PlaceHero
// stands the pawn on the terrain here itself, which is what ended three days
// of the player spawning inside a hill because a PlayerStart had been baked at
// the old world's coordinates and nothing ever compared the two.
const FVector AAHGameMode::HeroSpawn(AHArena::SpawnX, AHArena::SpawnY, 110.f);

int32 AAHGameMode::FreshSeed()
{
    return static_cast<int32>(FPlatformTime::Cycles64() ^ static_cast<uint64>(FDateTime::Now().GetTicks()));
}

void AAHGameMode::BuildArena(int32 Seed)
{
    ArenaSeed = Seed;
    // Before the first height question of the run: everything below traces the
    // ground, and without this pointer "the ground" means "the first roof".
    FindGround();
    for (auto& Old : ObstacleActors) if (IsValid(Old)) Old->Destroy();
    ObstacleActors.Reset();
    // The living were in that list too, so they have just been destroyed; the
    // arrays that pointed at them have to go with them, or the next streaming
    // pass walks a list of tombstones.
    Folk.Reset();
    Beasts.Reset();
    FolkUp.Reset();
    BeastUp.Reset();
    PrizeActor   = nullptr;
    bPrizeTaken  = false;
    ErrandStage  = 0;
    TalkingTo.Reset();
    TalkUntil    = 0.f;

    // Measure the meshes the generator is about to arrange. A fence can only be
    // tiled without gaps or overlaps by someone who knows how long a panel is,
    // and that is a fact about the asset, not a constant worth guessing. Asking
    // the mesh for its bounds is cheap and needs nothing spawned; the answers are
    // cached because the same fence is asked about dozens of times in a row.
    TMap<FString, FVector> Measured;
    auto MeasureMesh = [&Measured](const FString& Path) -> FVector
    {
        if (const FVector* Known = Measured.Find(Path)) return *Known;
        /**
         * The kit's DECLARED half-extent wins over the mesh's measured bounds.
         *
         * A measured bound is the box around whatever the asset happens to
         * contain -- a roof overhang, a chimney, a signboard on an arm -- and
         * using it to decide how far apart to stand two buildings is how a
         * village ends up either crammed or absurdly spread out. The kit's
         * footprint is what the piece was modelled inside, and it is the same
         * number the offline harness checks against, which is the only way a
         * green harness means anything at all.
         */
        FVector Extent = AHKit::SizeOf(Path) * 0.5;
        if (Extent.X <= 0.0)
        {
            Extent = FVector(100.0, 100.0, 100.0);
            if (const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
                Extent = Mesh->GetBounds().BoxExtent;
        }
        Measured.Add(Path, Extent);
        return Extent;
    };

    // And where the middle of those bounds sits, which is what decides how far
    // to drop a prop so it rests on the ground. With no actor to ask for its
    // world bounds any more, the pivot has to come from the mesh itself.
    TMap<FString, FVector> Middles;
    auto MeshMiddle = [&Middles](const FString& Path) -> FVector
    {
        if (const FVector* Known = Middles.Find(Path)) return *Known;
        FVector Origin(0.0, 0.0, 0.0);
        // Every kit piece has its pivot ON THE GROUND at the centre of its
        // footprint -- that is the promise the kit is built on -- so the
        // middle of its box is exactly half its height up, and nothing needs
        // to be dropped to make it rest on the floor. Asking the mesh instead
        // would answer with the bounds of the roof overhang.
        const FVector Declared = AHKit::SizeOf(Path);
        if (Declared.Z > 0.0)
        {
            Origin = FVector(0.0, 0.0, Declared.Z * 0.5);
        }
        else if (const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            Origin = Mesh->GetBounds().Origin;
        }
        Middles.Add(Path, Origin);
        return Origin;
    };

    FRandomStream Dice(Seed);
    // All four edges get a road running off the map. There is nothing on the
    // other side to walk to -- the world is one piece now -- but a road that
    // carries on over the hill is what stops the boundary reading as a wall.
    /**
     * How high the Landscape is at a spot, by asking it.
     *
     * The generator used to decide this for itself and the world used to be
     * built out of it. Now the terrain is a real Landscape baked into the map,
     * navigation is baked over it once, and this trace is the only thing that
     * knows where the ground is -- which means a house cannot be at a height
     * the ground is not, because nobody is keeping two answers in step any
     * more. There is only one.
     */
    auto GroundUnder = [this](float X, float Y) { return TerrainZ(X, Y); };

    // No gates: there is no neighbouring comarca to walk into any more. The
    // world is one island and its edges are the mountains the heightmap put
    // there.
    Plan = AHArena::Build(Dice, HeroSpawn, MeasureMesh, GroundUnder, 0);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // ── The props, instanced ─────────────────────────────────────────────────
    // One component per distinct mesh, not one actor per prop.
    //
    // Measuring settled an argument I had been losing to my own assumption: a
    // comarca of 2,477 pieces uses 39 DISTINCT meshes, so 97% of it is the same
    // handful of models over and over. As actors that was thousands of draw
    // calls and the reason the world had to stay small and be stitched together
    // with teleports. Instanced it is forty components, and the world can simply
    // be big.
    //
    // Blueprints -- the houses, the braziers, the well -- stay as actors: they
    // carry their own particle systems and lights and are only sixty-odd.
    AActor* Host = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
                                                  FVector::ZeroVector, FRotator::ZeroRotator, Params);
    TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Batches;
    if (Host)
    {
        auto* Anchor = NewObject<USceneComponent>(Host, TEXT("PropRoot"));
        Host->SetRootComponent(Anchor);
        Anchor->RegisterComponent();
        ObstacleActors.Add(Host);
    }

    /**
     * Can this mesh be drawn instanced at all, or will it come out grey?
     *
     * A material has to be compiled WITH the instanced-static-mesh usage flag
     * or Unreal silently substitutes the plain grey default. Four of the
     * pack's materials -- water, hay, flags, food -- ship without it, and the
     * editor has been saying so in the log since the day the props were
     * instanced: "missing usage flag InstancedStaticMeshes! Default Material
     * will be used in game."
     *
     * Asking the material rather than keeping a list of the four, because a
     * list rots the moment a pack is updated or another one is installed. What
     * cannot be instanced is spawned as its own actor instead, which costs a
     * draw call and is not grey.
     */
    auto Instanceable = [](UMaterialInterface* Skin)
    {
        return !Skin || Skin->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes);
    };
    TMap<FString, bool> CanBatch;
    int32 Solos = 0;
    constexpr int32 SoloBudget = 500;
    /**
     * How big a batch may be and still be worth un-batching.
     *
     * The ground is five and a half thousand tiles in one batch. If its
     * material ever failed the usage check, turning each tile into an actor
     * would blow the budget and the valley would have no floor at all -- a
     * cure enormously worse than the disease. Past this size the colour loses
     * and the geometry wins.
     */
    constexpr int32 SoloLimit = 300;
    /** Pieces the terrain trace could not find ground under. Must stay 0. */
    int32 Groundless = 0;
    TMap<FString, int32> BatchSize;
    for (const FAHArenaPiece& Count : Plan.Pieces)
        BatchSize.FindOrAdd(Count.MeshPath + TEXT("|") + Count.MaterialPath
                            + (Count.bFlat ? TEXT("|flat") : TEXT(""))
                            + (Count.bInvisible ? TEXT("|hidden") : TEXT("")))++;

    for (FAHArenaPiece& Piece : Plan.Pieces)
    {
        // ── On the ground, exactly ───────────────────────────────────────
        // The generator worked from a grid of samples five metres apart; the
        // Landscape has a vertex every metre. So every piece is traced onto
        // the real surface here and its height corrected by the difference,
        // which keeps the offset the generator intended -- a deck 130 above
        // its hillside stays 130 above it -- while pinning the thing itself to
        // ground that actually exists. This is what "sem estruturas bugadas"
        // costs: one trace per piece, about four thousand of them, once.
        {
            const float Sampled = Plan.GroundAt(Piece.Location);
            bool bFound = false;
            // Paving, water and the river's blocker are sheets: they follow the
            // ground at their own middle and must not be lifted onto the
            // highest corner of anything. Everything with bulk gets measured
            // across its footprint instead -- see FootprintZ.
            const bool bSheet = Piece.bFlat || Piece.bInvisible || !Piece.bSitOnGround;
            const float Exact = bSheet
                ? TerrainZ(static_cast<float>(Piece.Location.X),
                           static_cast<float>(Piece.Location.Y), &bFound)
                : FootprintZ(Piece.Location, Piece.Radius, &bFound);
            // No ground under it: keep the height the generator worked out from
            // the heightmap. That is roughly right, and dropping to zero is
            // catastrophically wrong.
            if (bFound) Piece.Location.Z += Exact - Sampled;
            else        ++Groundless;
        }

        // ── Smoke ────────────────────────────────────────────────────────
        // A plume has no model, so MeshPath carries the particle system and
        // nothing below this point applies to it. Chimneys and campfires: the
        // cheapest thing in the build that says the valley is inhabited.
        if (Piece.bSmoke)
        {
            UParticleSystem* Puff = LoadObject<UParticleSystem>(
                nullptr, *Piece.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
            if (!Puff) { UE_LOG(LogTemp, Warning, TEXT("AH_SMOKE sem sistema: %s"), *Piece.MeshPath); continue; }
            if (AEmitter* Plume = GetWorld()->SpawnActor<AEmitter>(
                    Piece.Location, FRotator::ZeroRotator, Params))
            {
                if (UParticleSystemComponent* Puffing = Plume->GetParticleSystemComponent())
                    Puffing->SetMobility(EComponentMobility::Movable);
                Plume->SetTemplate(Puff);
                Plume->SetActorScale3D(Piece.Scale);
                ObstacleActors.Add(Plume);
            }
            continue;
        }

        /**
         * The errand's pack: its own actor, never an instance.
         *
         * An instance cannot be removed without rebuilding its whole batch,
         * and a quest item still lying on the floor after you collected it is
         * a quest item the player believes he did not collect. One actor, one
         * pointer, one Destroy. Handled before the blueprint and instancing
         * branches so nothing downstream can also place it, and left to fall
         * through to the light below -- the glow on the bag is most of what
         * makes it findable in a room with no roof and no sun.
         */
        if (Piece.bPrize)
        {
            UStaticMesh* Leather = LoadObject<UStaticMesh>(
                nullptr, *Piece.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
            const FVector HalfBag = MeasureMesh(Piece.MeshPath);
            const FVector MidBag  = MeshMiddle(Piece.MeshPath);
            FVector Lying = Piece.Location;
            if (Piece.bSitOnGround)
                Lying.Z = Piece.Location.Z - (MidBag.Z - HalfBag.Z) * Piece.Scale.Z;
            AStaticMeshActor* Bag = Leather
                ? GetWorld()->SpawnActor<AStaticMeshActor>(Lying, Piece.Rotation, Params)
                : nullptr;
            if (Bag)
            {
                if (UStaticMeshComponent* Skin = Bag->GetStaticMeshComponent())
                {
                    Skin->SetMobility(EComponentMobility::Movable);
                    Skin->SetStaticMesh(Leather);
                    // Walk-through: nobody should have to path around the
                    // thing he walked four hundred metres to pick up.
                    Skin->SetCollisionProfileName(TEXT("NoCollision"));
                    Skin->SetCanEverAffectNavigation(false);
                }
                Bag->SetActorScale3D(Piece.Scale);
                ObstacleActors.Add(Bag);
                PrizeActor = Bag;
            }
        }
        else if (Piece.MeshPath.Contains(TEXT("/blueprints/")))
        {
            // A blueprint spawns by its generated class. Going through the
            // editor's actor factories instead would return nothing outside the
            // editor -- which once cost a whole build and 194 silent failures.
            const FString ClassPath = Piece.MeshPath + TEXT(".") +
                                      FPaths::GetCleanFilename(Piece.MeshPath) + TEXT("_C");
            UClass* Made = LoadClass<AActor>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
            AActor* Built = Made ? GetWorld()->SpawnActor<AActor>(Made, Piece.Location, Piece.Rotation, Params) : nullptr;
            if (Built)
            {
                // Everything spawned here must be fully dynamic. Pack blueprints
                // are authored for a baked level, so their meshes and lights
                // arrive Static or Stationary -- which puts "LIGHTING NEEDS TO BE
                // REBUILT" across the screen and lights them wrongly besides.
                TArray<USceneComponent*> Parts;
                Built->GetComponents(Parts);
                for (USceneComponent* Part : Parts)
                    if (Part && Part->Mobility != EComponentMobility::Movable)
                        Part->SetMobility(EComponentMobility::Movable);

                Built->SetActorScale3D(Piece.Scale);
                FVector Origin, Extent;
                Built->GetActorBounds(false, Origin, Extent);
                if (Piece.bSitOnGround)
                {
                    Built->SetActorLocation(FVector(Piece.Location.X, Piece.Location.Y,
                                                    Piece.Location.Z - (Origin.Z - Extent.Z)));
                    Built->GetActorBounds(false, Origin, Extent);
                }
                if (Piece.bCover)
                {
                    Piece.Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y));
                    Piece.TopZ   = static_cast<float>(Origin.Z + Extent.Z);
                }
                ObstacleActors.Add(Built);
            }
        }
        else if (Host)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
            if (!Mesh) continue;

            // The material and the paving flag are part of the key: one batch
            // cannot hold both a stone road with no collision and a stone wall
            // that blocks you.
            const FString Batch = Piece.MeshPath + TEXT("|") + Piece.MaterialPath
                                + (Piece.bFlat ? TEXT("|flat") : TEXT(""))
                                + (Piece.bInvisible ? TEXT("|hidden") : TEXT(""));

            UMaterialInterface* Skin = Piece.MaterialPath.IsEmpty() ? nullptr
                : LoadObject<UMaterialInterface>(nullptr, *Piece.MaterialPath, nullptr,
                                                 LOAD_NoWarn | LOAD_Quiet);
            if (!Piece.MaterialPath.IsEmpty() && !Skin)
                UE_LOG(LogTemp, Warning, TEXT("AH_SKIN faltou o material %s"), *Piece.MaterialPath);

            bool* Known = CanBatch.Find(Batch);
            if (!Known)
            {
                bool bFine = Instanceable(Skin);
                // The mesh's own slots too, when nothing overrides them. Walked
                // until GetMaterial runs out rather than asked for a count,
                // which is the one form of this question that has not moved
                // between engine versions.
                if (bFine && !Skin)
                    for (int32 Slot = 0; Slot < 8; ++Slot)
                    {
                        UMaterialInterface* Slotted = Mesh->GetMaterial(Slot);
                        if (!Slotted) break;
                        if (!Instanceable(Slotted)) { bFine = false; break; }
                    }
                const int32* Many = BatchSize.Find(Batch);
                const int32  Size = Many ? *Many : 1;
                if (!bFine && Size > SoloLimit)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("AH_SKIN %s sem a flag de malha instanciada, mas sao %d pecas: fica instanciado e cinza"),
                           *Piece.MeshPath, Size);
                    bFine = true;
                }
                else if (!bFine)
                    UE_LOG(LogTemp, Warning,
                           TEXT("AH_SKIN %s sem a flag de malha instanciada: %d pecas viram atores"),
                           *Piece.MeshPath, Size);
                Known = &CanBatch.Add(Batch, bFine);
            }

            if (!*Known)
            {
                // Its own actor: a draw call each, capped, and counted out loud
                // so a pack that ships fifty such materials cannot quietly turn
                // the valley into three thousand actors.
                if (Solos >= SoloBudget) continue;
                const FVector HalfSolo = MeasureMesh(Piece.MeshPath);
                const FVector MidSolo  = MeshMiddle(Piece.MeshPath);
                FVector Stand = Piece.Location;
                if (Piece.bSitOnGround)
                    Stand.Z = Piece.Location.Z - (MidSolo.Z - HalfSolo.Z) * Piece.Scale.Z;
                AStaticMeshActor* Solo = GetWorld()->SpawnActor<AStaticMeshActor>(
                    Stand, Piece.Rotation, Params);
                if (!Solo) continue;
                if (UStaticMeshComponent* Body = Solo->GetStaticMeshComponent())
                {
                    Body->SetMobility(EComponentMobility::Movable);
                    Body->SetStaticMesh(Mesh);
                    if (Skin) Body->SetMaterial(0, Skin);
                    if (Piece.bFlat)
                    {
                        Body->SetCollisionProfileName(TEXT("NoCollision"));
                        Body->SetCastShadow(false);
                        Body->SetCanEverAffectNavigation(false);
                    }
                    else
                    {
                        Body->SetCollisionProfileName(TEXT("BlockAll"));
                    }
                    if(Piece.MeshPath.Contains(TEXT("SM_Kit_arvore")))
                        Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
                }
                Solo->SetActorScale3D(Piece.Scale);
                ObstacleActors.Add(Solo);
                ++Solos;
                if (Piece.bCover)
                {
                    Piece.Radius = static_cast<float>(FMath::Max(HalfSolo.X, HalfSolo.Y)
                                                      * FMath::Max(Piece.Scale.X, Piece.Scale.Y));
                    Piece.TopZ   = static_cast<float>(Stand.Z + (MidSolo.Z + HalfSolo.Z) * Piece.Scale.Z);
                }
                continue;
            }

            UHierarchicalInstancedStaticMeshComponent** Found = Batches.Find(Batch);
            UHierarchicalInstancedStaticMeshComponent* Group = Found ? *Found : nullptr;
            if (!Group)
            {
                Group = NewObject<UHierarchicalInstancedStaticMeshComponent>(Host);
                Group->SetMobility(EComponentMobility::Movable);
                Group->SetStaticMesh(Mesh);
                if (Skin) Group->SetMaterial(0, Skin);
                if (Piece.bFlat)
                {
                    // Paving is paint, not geometry. Navigation is built from
                    // collision, so a road that collides is a hundred little
                    // slabs for Recast to find seams between, and a road you have
                    // to path around is worse than no road at all.
                    Group->SetCollisionProfileName(TEXT("NoCollision"));
                    Group->SetCastShadow(false);
                    Group->SetCanEverAffectNavigation(false);
                }
                else if (Piece.bInvisible)
                {
                    // ── The river's edge ─────────────────────────────────
                    // A box you cannot see, that you cannot walk into, and that
                    // an arrow flies straight through.
                    //
                    // The visibility channel is switched OFF on purpose and it
                    // is the whole trick: the cover rule and the line-of-sight
                    // trace both run on that channel, so a blocker that answered
                    // it would turn every river into a wall of invisible cover
                    // and stop shots in mid-air over open water. It still feeds
                    // navigation, which is what carves the river out of the
                    // navmesh so nobody paths across it.
                    Group->SetCollisionProfileName(TEXT("Custom"));
                    Group->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    Group->SetCollisionObjectType(ECC_WorldStatic);
                    Group->SetCollisionResponseToAllChannels(ECR_Block);
                    Group->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
                    Group->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
                    Group->SetCanEverAffectNavigation(true);
                    Group->SetVisibility(false);
                    Group->SetHiddenInGame(true);
                    Group->SetCastShadow(false);
                }
                else
                {
                    Group->SetCollisionProfileName(TEXT("BlockAll"));
                }
                if(Piece.MeshPath.Contains(TEXT("SM_Kit_arvore")))
                    Group->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
                Group->SetupAttachment(Host->GetRootComponent());
                Group->RegisterComponent();
                Host->AddInstanceComponent(Group);
                Batches.Add(Batch, Group);
            }

            // Sat on the ground from the mesh's own bounds rather than from a
            // spawned actor's: there is no actor to ask any more, and the bounds
            // were already measured for the generator.
            const FVector Half = MeasureMesh(Piece.MeshPath);
            const FVector Mid  = MeshMiddle(Piece.MeshPath);
            FVector Where = Piece.Location;
            if (Piece.bSitOnGround)
                Where.Z = Piece.Location.Z - (Mid.Z - Half.Z) * Piece.Scale.Z;
            Group->AddInstance(FTransform(Piece.Rotation, Where, Piece.Scale), /*bWorldSpace*/ true);

            if (Piece.bCover)
            {
                // Measured, never assumed. The generator's numbers were spacing
                // estimates; the cover rule runs against the real model.
                Piece.Radius = static_cast<float>(FMath::Max(Half.X, Half.Y)
                                                  * FMath::Max(Piece.Scale.X, Piece.Scale.Y));
                Piece.TopZ   = static_cast<float>(Where.Z + (Mid.Z + Half.Z) * Piece.Scale.Z);
            }
        }

        if (Piece.bLight)
        {
            if (auto* Glow = GetWorld()->SpawnActor<APointLight>(
                    Piece.Location + FVector(0.f, 0.f, 170.f), FRotator::ZeroRotator, Params))
            {
                if (auto* Lamp = Cast<UPointLightComponent>(Glow->GetLightComponent()))
                {
                    Lamp->SetMobility(EComponentMobility::Movable);
                    Lamp->SetIntensityUnits(ELightUnits::Lumens);
                    Lamp->SetIntensity(1600.f);
                    Lamp->SetAttenuationRadius(1100.f);
                    Lamp->SetLightColor(FLinearColor(1.f, .62f, .33f));
                    Lamp->SetCastShadows(false);
                }
                ObstacleActors.Add(Glow);
            }
        }
    }

    UE_LOG(LogTemp, Display,
           TEXT("AH_PROPS %d pecas em %d lotes instanciados + %d atores (%d por falta da flag)"),
           Plan.Pieces.Num(), Batches.Num(), ObstacleActors.Num(), Solos);
    // Zero, every time, or something is standing inside a hill. Said out loud
    // because the old code's answer to a failed trace was to pretend the ground
    // was at zero and carry on.
    if (Groundless > 0)
        UE_LOG(LogTemp, Warning,
               TEXT("AH_GROUND %d pecas sem chao sob elas -- ficaram na altura estimada"),
               Groundless);

    // ── Camps ────────────────────────────────────────────────────────────────
    /**
     * Described, not built.
     *
     * The world holds about twenty camps now, where it used to hold five, and
     * every one of them is a landmark you can see from a distance. Spawning
     * them all at load would be sixty skeletal meshes on a 6 GB card. So each
     * camp is written down here -- where, how many, and the seed that decides
     * who -- and StreamCamps builds the ones you are near.
     *
     * Built last, as before, so a foe stands on top of the scenery and never
     * inside it.
     */
    Camps.Reset();
    Loot.Reset();
    PerceptionChecks.Reset();
    Fighting   = INDEX_NONE;
    bExploring = true;
    for (const FAHCampSpot& Spot : Plan.Camps)
    {
        FAHCamp Camp;
        // Lifted onto the hillside it stands on: the generator hands out every
        // Z measured from the ground, so a camp centre arrives at zero, and a
        // foe spawned at zero on ground four metres up is buried in it.
        Camp.Centre = FVector(Spot.Where.X, Spot.Where.Y,
                              TerrainZ(static_cast<float>(Spot.Where.X),
                                       static_cast<float>(Spot.Where.Y)));
        /**
         * Indoors they notice you from five metres, outdoors from ten to
         * thirteen.
         *
         * A dungeon's rooms are twelve to twenty-five metres apart, so an
         * outdoor alert radius would have the whole dungeon come at you
         * through the walls the moment you stepped inside -- eight foes in
         * one fight, which is not a dungeon, it is an ambush. Room by room is
         * the whole point of having rooms.
         */
        Camp.Alert = Spot.bIndoor ? Dice.FRandRange(480.f, 620.f)
                                  : Dice.FRandRange(950.f, 1300.f);
        Camp.Seed  = Dice.RandRange(1, 1000000);
        // How many belong here comes with the place that decided it. This
        // used to be worked out by matching positions against the landmark
        // list, which was a parallel array wearing a disguise.
        Camp.Many  = FMath::Max(1, Spot.Foes);
        Camp.WildlifeKind=Spot.WildlifeKind;
        Camp.bIndoor = Spot.bIndoor;
        Camps.Add(Camp);
    }
    // ── The living ───────────────────────────────────────────────────────────
    /**
     * Described, like the camps, and built near the player only.
     *
     * One flag per described person and per described animal, so the streaming
     * pass is a walk over two arrays of bools rather than a search for who is
     * already standing where. Nothing is spawned here: StreamLiving below does
     * that, on the same schedule as the camps.
     */
    FolkUp.Init(false, Plan.Folk.Num());
    InitializeSideQuests();
    BeastUp.Init(false, Plan.Beasts.Num());
    UE_LOG(LogTemp, Display,
           TEXT("AH_VIVOS %d pessoas e %d bichos descritos no vale"),
           Plan.Folk.Num(), Plan.Beasts.Num());

    NextStream = 0.f;
    StreamCamps();
    StreamLiving();
    int32 Awake = 0;
    for (const FAHCamp& Camp : Camps) if (Camp.bAwake) ++Awake;
    UE_LOG(LogTemp, Display,
           TEXT("AH_CAMPS %d acampamentos no mapa, %d marcos, %d acordados agora"),
           Camps.Num(), Plan.Landmarks.Num(), Awake);

    ApplyArenaLight();
    UE_LOG(LogTemp, Display, TEXT("AH_ARENA %s semente %d, %d pecas, %d acampamentos"),
           *Plan.Name, Seed, Plan.Pieces.Num(), Camps.Num());

    // The ground just moved under the player, so put him back on top of it,
    // and make sure there is something for him to walk on.
    bHeroPlaced = false;
    bNavChecked = false;
    PlaceHero();
    EnsureNavigation();
}

/**
 * Which way something lies, in one or two letters.
 *
 * World +X is NORTH here, because that is which way the minimap points and
 * the two must agree -- a compass in the HUD that disagrees with the map next
 * to it is worse than neither.
 */
static FString AHCompass(const FVector& From, const FVector& To)
{
    static const TCHAR* const Points[8] = { TEXT("N"), TEXT("NE"), TEXT("L"), TEXT("SE"),
                                            TEXT("S"), TEXT("SO"), TEXT("O"), TEXT("NO") };
    const double Angle = FMath::RadiansToDegrees(
        FMath::Atan2(To.Y - From.Y, To.X - From.X));
    const int32 Slot = ((FMath::RoundToInt(Angle / 45.0) % 8) + 8) % 8;
    return Points[Slot];
}

void AAHGameMode::WhereTo(FString& OutDungeon, FString& OutFight) const
{
    const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
    const FVector Me = Hero ? Hero->GetActorLocation() : HeroSpawn;

    double NearestDen = 1.0e30, NearestFight = 1.0e30;
    FVector Den = FVector::ZeroVector, Fight = FVector::ZeroVector;
    for (const FAHLandmark& Mark : Plan.Landmarks)
    {
        if (Mark.Kind != EAHSite::Masmorra) continue;
        const double Away = FVector::Dist2D(Me, Mark.Where);
        if (Away < NearestDen) { NearestDen = Away; Den = Mark.Where; }
    }
    for (const FAHCamp& Camp : Camps)
    {
        if (Camp.bCleared) continue;
        const double Away = FVector::Dist2D(Me, Camp.Centre);
        if (Away < NearestFight) { NearestFight = Away; Fight = Camp.Centre; }
    }

    OutDungeon = NearestDen < 1.0e29
        ? FString::Printf(TEXT("MASMORRA  %.0f m  %s"), NearestDen / 100.0, *AHCompass(Me, Den))
        : TEXT("MASMORRA  nenhuma neste mundo");
    OutFight = NearestFight < 1.0e29
        ? FString::Printf(TEXT("INIMIGOS  %.0f m  %s"), NearestFight / 100.0, *AHCompass(Me, Fight))
        : TEXT("INIMIGOS  o mundo esta limpo");
}

void AAHGameMode::EnsureNavigation()
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav)
    {
        UE_LOG(LogTemp, Warning, TEXT("AH_NAV nao ha sistema de navegacao neste mundo"));
        return;
    }
    ANavigationData* Data = nullptr;
    for (TActorIterator<ANavigationData> It(GetWorld()); It; ++It) { Data = *It; break; }
    if (!Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("AH_NAV nao ha malha de navegacao no mapa"));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("AH_NAV %s geracao=%d"),
           *Data->GetName(), static_cast<int32>(Data->GetRuntimeGenerationMode()));

    // Built here rather than trusted to rebuild itself. The valley is rolled
    // fresh every time, so the geometry the navmesh has to cover did not exist
    // when the map was saved, and the map's own idea of whether it rebuilds at
    // runtime is a serialised property the project's ini cannot reach.
    Nav->Build();
    // And the navmesh itself is told to rebuild, which is the one call that
    // does not care what the map serialised about runtime generation. If that
    // property came out of an older save as Static, the baked tiles thrown
    // away at load would never be replaced and nothing would say so -- the
    // same shape of bug as the PlayerStart that was left behind at (0, -6000).
    Data->RebuildAll();

    FNavLocation Landed;
    // Traced, not sampled: this is the one position in the game that has to be
    // exactly right, because being wrong about it means spawning inside a hill.
    const FVector Where(HeroSpawn.X, HeroSpawn.Y,
                        TerrainZ(HeroSpawn.X, HeroSpawn.Y) + HeroSpawn.Z);
    const bool bFound = Nav->ProjectPointToNavigation(Where, Landed, FVector(300.f, 300.f, 600.f));
    /**
     * Asked once here, and then kept asking in Tick until it says yes.
     *
     * A navmesh built around an invoker does not exist at the instant the
     * invoker appears; Recast has tiles to grind. This call runs a few
     * milliseconds after PlaceHero, so a NO here means nothing at all, and
     * saying it in the same words as a real failure is how two debugging
     * rounds went into a line that was never wrong.
     */
    bNavConfirmed = bFound;
    UE_LOG(LogTemp, Display, TEXT("AH_NAV ponto de chegada %s%s"),
           bFound ? TEXT("esta na malha em ") : TEXT("ainda nao esta na malha (normal: o Recast acabou de comecar)"),
           bFound ? *FString::Printf(TEXT("(%.0f, %.0f, %.0f)"),
                                     Landed.Location.X, Landed.Location.Y, Landed.Location.Z)
                  : TEXT(""));

    // And the dungeons, by name and bearing. "Nao consigo entrar na masmorra"
    // has two very different causes -- the ground, or not finding it -- and
    // these three lines tell them apart before anyone theorises.
    int32 Dens = 0;
    for (const FAHLandmark& Mark : Plan.Landmarks)
    {
        if (Mark.Kind != EAHSite::Masmorra) continue;
        ++Dens;
        UE_LOG(LogTemp, Display, TEXT("AH_MASMORRA %d: %.0f m ao %s do inicio, em (%.0f, %.0f)"),
               Dens, FVector::Dist2D(Mark.Where, HeroSpawn) / 100.0,
               *AHCompass(HeroSpawn, Mark.Where), Mark.Where.X, Mark.Where.Y);
    }
    if (Dens == 0)
        UE_LOG(LogTemp, Warning, TEXT("AH_MASMORRA este mundo nao rolou nenhuma"));
}

/**
 * Finds the Landscape once, and says so.
 *
 * By class name, because `#include "Landscape.h"` means adding the Landscape
 * module to Build.cs, and a new module dependency is a build risk taken for a
 * pointer comparison. The names that matter are `Landscape` and
 * `LandscapeStreamingProxy`; `LandscapeGizmoActiveActor` and the like are
 * editor furniture and are excluded by requiring the name to start with it.
 */
void AAHGameMode::FindGround()
{
    GroundActor = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Maybe = *It;
        if (!IsValid(Maybe)) continue;
        const FString Kind = Maybe->GetClass()->GetName();
        if (!Kind.StartsWith(TEXT("Landscape"))) continue;
        if (Kind.Contains(TEXT("Gizmo")) || Kind.Contains(TEXT("Spline"))) continue;
        GroundActor = Maybe;
        UE_LOG(LogTemp, Display, TEXT("AH_CHAO o terreno e %s (%s)"),
               *Maybe->GetName(), *Kind);
        return;
    }
    UE_LOG(LogTemp, Warning,
           TEXT("AH_CHAO nao achei o Landscape: as alturas vao usar o primeiro ")
           TEXT("obstaculo de cima, que e como coisas acabam em cima dos telhados"));
}

float AAHGameMode::TerrainZ(float X, float Y, bool* bFound) const
{
    FCollisionQueryParams Look;
    Look.bTraceComplex = false;
    const FVector Above(X, Y, 60000.f), Below(X, Y, -20000.f);

    /**
     * Every hit, and then the one that is the Landscape.
     *
     * A single trace returns the FIRST thing in the way, and on top of a
     * building that is the roof. This is the whole of the fix for "alguns
     * inimigos aparecem em cima de estruturas e nao se movem": they were being
     * stood on the roof, where there is no navmesh, so they could not take a
     * step for the rest of the fight. The same line had been quietly lifting
     * props onto each other all along, because BuildArena traces each piece
     * after spawning the ones before it.
     */
    if (GetWorld() && IsValid(GroundActor))
    {
        TArray<FHitResult> Everything;
        if (GetWorld()->LineTraceMultiByObjectType(Everything, Above, Below, FCollisionObjectQueryParams(ECC_WorldStatic), Look))
            for (const FHitResult& Hit : Everything)
                if (Hit.GetActor() == GroundActor)
                {
                    if (bFound) *bFound = true;
                    return static_cast<float>(Hit.ImpactPoint.Z);
                }
        // Fall through: no Landscape under this spot at all (past its edge).
        if (bFound) *bFound = false;
        return 0.f;
    }

    FHitResult Found;
    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
            Found, Above, Below, ECC_Visibility, Look))
    {
        if (bFound) *bFound = true;
        return static_cast<float>(Found.ImpactPoint.Z);
    }
    /**
     * A miss used to return 0, and that was one of the worst lines in the file.
     *
     * On the old flat slab 0 WAS the ground, so the failure was invisible and
     * harmless. On a kilometre of Landscape 0 is up to forty-eight metres below
     * the surface, so the same line silently teleported whatever asked into the
     * middle of a hill -- with nothing anywhere saying a trace had failed. A
     * fallback that looks like a plausible answer is worse than no fallback.
     * The caller is told, and decides.
     */
    if (bFound) *bFound = false;
    return 0.f;
}

float AAHGameMode::FootprintZ(const FVector& Where, float Radius, bool* bFound) const
{
    // Five samples: the pivot and the four corners of the footprint, pulled in
    // slightly so a piece standing at the very lip of a terrace measures its
    // own ground rather than the drop beside it.
    const float Reach = FMath::Clamp(Radius, 60.f, 700.f) * 0.85f;
    static const float Corner[4][2] = {{1.f, 1.f}, {1.f, -1.f}, {-1.f, 1.f}, {-1.f, -1.f}};

    bool bAny = false;
    bool bHere = false;
    float Highest = TerrainZ(static_cast<float>(Where.X), static_cast<float>(Where.Y), &bHere);
    bAny |= bHere;
    // Not MAX_FLT: that macro is on its way out of the engine, and a number
    // this far below the map is unambiguous anyway.
    if (!bHere) Highest = -1.0e9f;

    for (const auto& Off : Corner)
    {
        bool bThere = false;
        const float Sample = TerrainZ(static_cast<float>(Where.X) + Off[0] * Reach,
                                      static_cast<float>(Where.Y) + Off[1] * Reach, &bThere);
        if (bThere) { bAny = true; Highest = FMath::Max(Highest, Sample); }
    }

    if (bFound) *bFound = bAny;
    return bAny ? Highest : 0.f;
}

void AAHGameMode::PlaceHero()
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;                      // not spawned yet; Tick tries again
    const FVector Was   = Hero->GetActorLocation();
    // Traced, not sampled: this is the one position in the game that has to be
    // exactly right, because being wrong about it means spawning inside a hill.
    const FVector Where(HeroSpawn.X, HeroSpawn.Y,
                        TerrainZ(HeroSpawn.X, HeroSpawn.Y) + HeroSpawn.Z);
    Hero->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
    bHeroPlaced = true;

    /**
     * And the hero becomes a navigation invoker.
     *
     * This is the line that makes navigation possible at all on a map this
     * size, and it is worth being precise about why. Recast with dynamic
     * generation dirties EVERY tile in the navigation bounds at load. Over a
     * kilometre that is sixteen hundred tiles, it chews through them in an
     * order nobody chose, and while it does the game has a navmesh in some
     * arbitrary places and none in others -- which is exactly what the log
     * showed: `caminho valido parcial` in one spot and `sem navmesh` in the
     * next, and foes walking at walls in between.
     *
     * With an invoker, and bGenerateNavigationOnlyAroundNavigationInvokers
     * in DefaultEngine.ini, only the tiles near the player are built. A
     * hundred-odd instead of sixteen hundred, and they are the hundred that
     * matter. The generation radius covers more than a camp's wake range
     * (16000 uu is where camps come alive, but a fight happens far closer),
     * and the removal radius is well beyond it so walking back and forth
     * across a boundary does not thrash.
     */
    if (!Hero->FindComponentByClass<UNavigationInvokerComponent>())
    {
        auto* Invoker = NewObject<UNavigationInvokerComponent>(Hero);
        if (Invoker)
        {
            // Eighty metres, not a hundred and thirty.
            //
            // The radius is not free: every tile inside it has to be built
            // and rebuilt as the player walks, and on this machine they were
            // arriving late -- the log has `sem navmesh` in open grass with
            // nothing anywhere near it. Eighty metres is still more than
            // twice what the camera shows and about a third of the tiles.
            Invoker->SetGenerationRadii(8000.f, 13000.f);
            Invoker->RegisterComponent();
            UE_LOG(LogTemp, Display,
                   TEXT("AH_NAV heroi virou invoker: gera 80 m, descarta 130 m"));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("AH_NAV NAO consegui criar o invoker -- sem malha de navegacao"));
        }
    }
    // Said out loud, with both positions. The whole bug was that nothing ever
    // compared where the map put him with where the generator assumed he was.
    UE_LOG(LogTemp, Display, TEXT("AH_SPAWN de (%.0f, %.0f, %.0f) para (%.0f, %.0f, %.0f)"),
           Was.X, Was.Y, Was.Z, Where.X, Where.Y, Where.Z);
}

void AAHGameMode::ApplyArenaLight()
{
    // The hour of the day is part of the roll. The light actors themselves stay
    // baked in the map -- only their settings move -- because a light spawned at
    // runtime cannot be captured by the sky light the way a placed one can.
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
    {
        It->SetActorRotation(FRotator(Plan.SunPitch, Plan.SunYaw, 0.f));
        if (auto* Key = It->GetLightComponent())
        {
            Key->SetMobility(EComponentMobility::Movable);
            Key->SetTemperature(Plan.SunTemperature);
        }
    }
    for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
        if (auto* Fill = It->GetLightComponent())
        {
            Fill->SetMobility(EComponentMobility::Movable);
            Fill->SetIntensity(Plan.SkyIntensity);
        }

    /**
     * And then say what the rig actually came out as.
     *
     * "As coisas continuam cinza" has now survived two fixes aimed at the
     * materials, and the log carries no material warning at all -- which
     * means the next suspect is the light, and there is currently no way to
     * tell a washed-out world from a grey one without being in the room. A
     * strong sky against a weak sun is exactly what flattens colour into
     * grey, and neither number has ever been printed.
     */
    int32 Suns = 0, Skies = 0;
    float SunLux = 0.f, SkyLux = 0.f;
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
        if (const auto* Key = It->GetLightComponent()) { ++Suns; SunLux = Key->Intensity; }
    for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
        if (const auto* Fill = It->GetLightComponent()) { ++Skies; SkyLux = Fill->Intensity; }
    UE_LOG(LogTemp, Display,
           TEXT("AH_LUZ %d sol(%.1f, inclinacao %.0f, %.0fK) + %d ceu(%.2f)"),
           Suns, SunLux, Plan.SunPitch, Plan.SunTemperature, Skies, SkyLux);
    if (Suns == 0)
        UE_LOG(LogTemp, Warning, TEXT("AH_LUZ NAO ha luz direcional no mapa -- ")
                                 TEXT("tudo vai parecer chapado e cinza"));
}

EAHCover AAHGameMode::CoverBetween(const FVector& From, const FVector& To) const
{
    return AHArena::CoverBetween(Plan.Pieces, From, To);
}

AAHGameMode::AAHGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AAHCharacter::StaticClass();
    PlayerControllerClass = AAHPlayerController::StaticClass();
    HUDClass = AAHCombatHUD::StaticClass();
}
void AAHGameMode::BeginPlay()
{
    Super::BeginPlay();
    BuildArena(FreshSeed());
    if(auto* Forest=LoadObject<USoundBase>(nullptr,TEXT("/Game/AshenHollow/Audio/Forest")))
        UGameplayStatics::SpawnSound2D(this,Forest,.3f,1.f,0.f,nullptr,false,true);
}

AAHCharacter* AAHGameMode::ActiveCharacter() const { return Order.IsValidIndex(ActiveIndex) ? Order[ActiveIndex].Get() : nullptr; }
void AAHGameMode::BeginCombat(int32 CampIndex, bool bAmbush, AAHCharacter* Trigger)
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || !Camps.IsValidIndex(CampIndex)) return;
    Camps[CampIndex].bTouched = true;

    Order.Reset();
    FightingCamps.Reset();
    FightingCamps.Add(CampIndex);
    if(!Trigger)
        for(const auto& Foe:Camps[CampIndex].Foes)
            if(Foe.IsValid() && Foe->IsAlive() && CanWitness(Foe.Get(),Hero,JoinRange)) { Trigger=Foe.Get(); break; }
    if(!IsValid(Trigger) || !Trigger->IsAlive()) return;
    Order.Add(Trigger);
    // Only witnesses of the original event join. No recursive camp-to-camp alarm.
    for(int32 I=0;I<Camps.Num();++I)
    {
        if(!Camps[I].bAwake || Camps[I].bCleared) continue;
        for(const auto& Foe:Camps[I].Foes)
        {
            if(!Foe.IsValid() || Foe.Get()==Trigger || !Foe->IsAlive() || Order.Num()>=FightCap) continue;
            if(!CanWitness(Foe.Get(),Trigger)) continue;
            Order.Add(Foe.Get()); Camps[I].bTouched=true; FightingCamps.AddUnique(I);
        }
    }
    Order.Add(Hero);

    FRandomStream Dice; Dice.GenerateNewSeed();
    for (const auto& Character : Order)
        Character->Initiative = Dice.RandRange(1, 20) + Character->InitiativeBonus;
    Order.StableSort([](const AAHCharacter& A, const AAHCharacter& B)
        { return A.Initiative > B.Initiative || (A.Initiative == B.Initiative && !A.bEnemy && B.bEnemy); });

    Hero->bRoaming  = false;
    Hero->bSneaking = false;
    bSurprise       = bAmbush;
    if (bAmbush)
    {
        // Hidden, so the opening swing carries advantage through the ordinary
        // machinery -- and a rogue's Sneak Attack rides along with it.
        Hero->bHidden = true;
        // And the hero opens, whatever the dice said about initiative.
        Order.StableSort([](const AAHCharacter& A, const AAHCharacter& B) { return !A.bEnemy && B.bEnemy; });
    }
    Fighting    = CampIndex;
    bExploring  = false;
    bStarted    = true;
    bFinished   = false;
    ActiveIndex = 0;
    Round       = 1;
    TurnStarted = GetWorld()->GetTimeSeconds();
    ActiveCharacter()->StartTurn();
    Hero->AddLog(bAmbush
        ? FString::Printf(TEXT("EMBOSCADA! %d inimigo(s) pegos de surpresa: eles perdem a primeira rodada e voce ataca com vantagem."),
                          Order.Num() - 1)
        : FString::Printf(TEXT("Voce foi avistado: %d inimigo(s). Sua iniciativa %d"),
                          Order.Num() - 1, Hero->Initiative));
}

void AAHGameMode::EndCombat()
{
    for (const auto& Character : Order) if (Character) Character->FinishTurn();

    for (int32 CampIndex : FightingCamps)
    {
        if (!Camps.IsValidIndex(CampIndex)) continue;
        bool bStanding = false;
        for (const auto& Foe : Camps[CampIndex].Foes)
            if (Foe.IsValid() && Foe->IsAlive()) bStanding = true;
        Camps[CampIndex].bCleared = !bStanding;
    }
    FightingCamps.Reset();
    UpdateSideQuests();

    Order.Reset();
    Fighting   = INDEX_NONE;
    bSurprise  = false;
    bStarted   = false;
    bFinished  = false;
    bExploring = true;

    if (auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        // A breather, not a night's sleep: the once-per-encounter abilities come
        // back, hit points and spell slots do not. A string of camps should cost
        // something, or exploring is just a corridor with fights in it.
        // Recovery is now an explicit rest, chosen from the character sheet.
        int32 Left = 0;
        for (const FAHCamp& Camp : Camps) if (!Camp.bCleared) ++Left;
        Hero->AddLog(Left > 0
            ? FString::Printf(TEXT("Area limpa. Ainda ha %d grupo(s) no vale."), Left)
            : FString(TEXT("Area limpa. O vale esta em silencio.")));
    }
}

int32 AAHGameMode::WakeCamp(FAHCamp& Camp)
{
    if (Camp.bAwake || Camp.bCleared) return 0;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    // Its own stream, from its own seed: the same camp holds the same people
    // every time you walk back into it, which is the difference between a
    // world and a slot machine.
    FRandomStream Local(Camp.Seed);
    int32 Stood = 0;
    for (int32 I = 0; I < Camp.Many; ++I)
    {
      for (int32 Attempt=0; Attempt<8; ++Attempt)
      {
        const float   Angle = Camp.bIndoor ? I * 360.f / Camp.Many + Attempt*43.f : Local.FRandRange(0.f, 360.f);
        const FVector Out   = Camp.Centre
            + FVector(FMath::Cos(FMath::DegreesToRadians(Angle)),
                      FMath::Sin(FMath::DegreesToRadians(Angle)), 0.f)
              * (Camp.bIndoor ? 180.f+Attempt*30.f : Local.FRandRange(300.f, 850.f));
        // The ground UNDER THE FOE, not under the camp's middle. That one line
        // was "inimigos de baixo da terra": a foe stands metres out from the
        // centre, and on any slope that is metres of difference.
        bool bStood = false;
        const float Under = TerrainZ(static_cast<float>(Out.X),
                                     static_cast<float>(Out.Y), &bStood);
        if(!bStood) continue;
        FVector Stand(Out.X, Out.Y,
                      (bStood ? Under : static_cast<float>(Camp.Centre.Z)) + 110.f);
        /**
         * And then put down where the navmesh actually is.
         *
         * Standing a foe on correct ground is not the same as standing him
         * somewhere he can walk from. A spot inside a wall, on a tent, or in
         * the two metres of rock between two dungeon rooms is ground -- and a
         * foe there never takes a step for the whole fight, which is what
         * "alguns nao se movem" was. Projection answers the real question,
         * costs one query per foe, and falls back to the rolled spot so a
         * camp can never come out empty because Recast was still building.
         */
        if (auto* Mesh = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            FNavLocation Footing;
            if (!Mesh->ProjectPointToNavigation(FVector(Out.X,Out.Y,Under), Footing, FVector(140.f,140.f,140.f))) continue;
            if(FMath::Abs(Footing.Location.Z-TerrainZ(Footing.Location.X,Footing.Location.Y))>80) continue;
            const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
            if(!Player) continue;
            const auto* Route=Mesh->FindPathToLocationSynchronously(GetWorld(),Player->GetNavAgentLocation(),Footing.Location);
            if(!Route || !Route->IsValid() || Route->IsPartial()) continue;
            Stand = Footing.Location + FVector(0,0,100);
        }
        else continue;
        if (auto* Foe = GetWorld()->SpawnActor<AAHCharacter>(
                Camp.WildlifeKind ? AAHWildEnemy::StaticClass() : AAHCharacter::StaticClass(), Stand, FRotator(0.f, Angle + 180.f, 0.f), Params))
        {
            if (auto* Animal=Cast<AAHWildEnemy>(Foe)) Animal->ConfigureWildlife(Camp.WildlifeKind,Local.RandRange(1,MAX_int32));
            else Foe->BecomeEnemy(Local.RandRange(1,MAX_int32));
            // Where he is standing decides how far he may stroll while he waits.
            Foe->bIndoorFoe = Camp.bIndoor;
            Camp.Foes.Add(Foe);
            ObstacleActors.Add(Foe);      // torn down with the rest on a rebuild
            ++Stood;
            break;
        }
      }
    }
    Camp.bAwake = Stood > 0;
    return Stood;
}

int32 AAHGameMode::SleepCamp(FAHCamp& Camp)
{
    if (!Camp.bAwake) return 0;
    int32 Gone = 0;
    for (const auto& Foe : Camp.Foes)
    {
        if (!Foe.IsValid()) continue;
        ObstacleActors.Remove(static_cast<AActor*>(Foe.Get()));
        Foe->Destroy();
        ++Gone;
    }
    Camp.Foes.Reset();
    Camp.bAwake = false;
    return Gone;
}

void AAHGameMode::StreamCamps()
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    const FVector Eye = Hero->GetActorLocation();

    int32 Standing = 0;
    for (const FAHCamp& Camp : Camps)
        for (const auto& Foe : Camp.Foes)
            if (Foe.IsValid()) ++Standing;

    for (int32 I = 0; I < Camps.Num(); ++I)
    {
        FAHCamp& Camp = Camps[I];
        if (Camp.bCleared) continue;
        const float Away = FVector::Dist2D(Eye, Camp.Centre);

        if (!Camp.bAwake)
        {
            // The budget is a HARDWARE limit, not a design one, so a camp that
            // does not fit simply waits: walk closer and something behind you
            // will have gone to sleep by then.
            if (Away < WakeRange && Standing + Camp.Many <= LiveFoes)
                Standing += WakeCamp(Camp);
        }
        // Never a camp you have fought in, and never the one you are fighting.
        // Putting a half-cleared camp to sleep and waking it again would build
        // its dead back at full health.
        else if (Away > SleepRange && I != Fighting && !FightingCamps.Contains(I)
                 && !Camp.bTouched)
        {
            Standing -= SleepCamp(Camp);
        }
    }
}

/**
 * Builds the people and animals you are near, and forgets the ones behind you.
 *
 * The same shape as StreamCamps and for the same reason, with two differences.
 * The ranges are shorter -- a hen a hundred and fifty metres away is a hen
 * nobody can see, and spending a skeletal mesh on a villager at that distance
 * is spending it on nothing. And there is no equivalent of bTouched: nothing
 * here can be half-killed, so anybody can be put away and rebuilt from the
 * same seed, which is why the carter's quest state lives on the game mode.
 */
void AAHGameMode::StreamLiving()
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    const FVector Eye = Hero->GetActorLocation();

    FActorSpawnParameters Params;
    // AlwaysSpawn, deliberately. The generator already proved this spot is
    // clear of the scenery with the pieces' real footprints; what the engine
    // would find here instead is another villager's capsule or the player's,
    // and refusing to spawn over those would leave a hole in the village that
    // never fills. They walk apart in a second.
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // ── People ──────────────────────────────────────────────────────────────
    Folk.RemoveAll([](const TObjectPtr<AAHVillager>& Who) { return !IsValid(Who); });
    int32 Standing = Folk.Num();
    for (int32 I = 0; I < Plan.Folk.Num() && I < FolkUp.Num(); ++I)
    {
        const FAHFolkSpot& Spot = Plan.Folk[I];
        const float Away = FVector::Dist2D(Eye, Spot.Where);
        if (!FolkUp[I])
        {
            if (Away > LivingWake || Standing >= LiveFolk) continue;
            bool bGround = false;
            const float Under = TerrainZ(static_cast<float>(Spot.Where.X),
                                         static_cast<float>(Spot.Where.Y), &bGround);
            // Plus the capsule's half height, exactly as the foes are placed:
            // a character spawned with its middle on the ground is a character
            // buried to the waist.
            FVector Stand(Spot.Where.X, Spot.Where.Y,
                          (bGround ? Under : static_cast<float>(Spot.Where.Z)) + 110.f);
            // On the navmesh, for the same reason the foes are: a villager off
            // it stands still forever, and unlike a foe he has no fight to
            // explain it.
            if (auto* Mesh = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
            {
                FNavLocation Footing;
                if (Mesh->ProjectPointToNavigation(Stand, Footing, FVector(250.f, 250.f, 400.f)))
                    Stand = FVector(Footing.Location.X, Footing.Location.Y, Footing.Location.Z + 100.f);
            }
            AAHVillager* Who = GetWorld()->SpawnActor<AAHVillager>(
                AAHVillager::StaticClass(), Stand, FRotator(0.f, Spot.Yaw, 0.f), Params);
            if (!Who) continue;
            // Folded first: a seed times a hundred and thirty-one overflows
            // int32 for perfectly ordinary seeds, and signed overflow is
            // undefined rather than merely wrong.
            Who->Settle(Spot, (ArenaSeed % 100003) * 131 + I * 7919 + 3);
            // His home is the GROUND he stood up on, not his capsule's middle:
            // it is handed to GetRandomReachablePointInRadius, and a navmesh
            // query asked from a metre in the air is a query asking about the
            // wrong place.
            Who->Casa = FVector(Spot.Where.X, Spot.Where.Y,
                                bGround ? Under : static_cast<float>(Spot.Where.Z));
            Folk.Add(Who);
            ObstacleActors.Add(Who);
            FolkUp[I] = true;
            ++Standing;
            if (Spot.bGiver) UE_LOG(LogTemp, Display,
                TEXT("AH_RECADO o carroceiro %s esta de pe em (%.0f, %.0f)"),
                *Who->Nome, Stand.X, Stand.Y);
        }
        else if (Away > LivingSleep)
        {
            // Never the one you are in the middle of talking to -- and skipping
            // just HIM, not the rest of the pass: an early return here would
            // also have stopped every animal in the valley streaming for as
            // long as one conversation was open.
            bool bBusy = false;
            for (int32 J = Folk.Num() - 1; J >= 0; --J)
            {
                AAHVillager* Who = Folk[J].Get();
                if (!IsValid(Who) || FVector::Dist2D(Who->Casa, Spot.Where) > 20.f) continue;
                if (TalkingTo.Get() == Who) { bBusy = true; continue; }
                ObstacleActors.Remove(static_cast<AActor*>(Who));
                Folk.RemoveAt(J);
                Who->Destroy();
                --Standing;
            }
            if (!bBusy) FolkUp[I] = false;
        }
    }

    // ── Animals ─────────────────────────────────────────────────────────────
    Beasts.RemoveAll([](const TObjectPtr<AAHBeast>& What) { return !IsValid(What); });
    int32 Loose = Beasts.Num();
    for (int32 I = 0; I < Plan.Beasts.Num() && I < BeastUp.Num(); ++I)
    {
        const FAHBeastSpot& Spot = Plan.Beasts[I];
        const float Away = FVector::Dist2D(Eye, Spot.Where);
        if (!BeastUp[I])
        {
            if (Away > LivingWake || Loose >= LiveBeasts) continue;
            static const TCHAR* const Bodies[] =
            {
                TEXT("SM_Kit_galinha"), TEXT("SM_Kit_veado"), TEXT("SM_Kit_corvo")
            };
            const int32 Which = FMath::Clamp(static_cast<int32>(Spot.Kind), 0, 2);
            UStaticMesh* Body = LoadObject<UStaticMesh>(nullptr,
                *(FString(AHKit::Folder) + Bodies[Which]), nullptr, LOAD_NoWarn | LOAD_Quiet);
            if (!Body) continue;     // the kit has not been imported yet
            bool bGround = false;
            const float Under = TerrainZ(static_cast<float>(Spot.Where.X),
                                         static_cast<float>(Spot.Where.Y), &bGround);
            AAHBeast* What = GetWorld()->SpawnActor<AAHBeast>(
                AAHBeast::StaticClass(),
                FVector(Spot.Where.X, Spot.Where.Y, bGround ? Under : Spot.Where.Z),
                FRotator::ZeroRotator, Params);
            if (!What) continue;
            What->Settle(Spot, (ArenaSeed % 100003) * 977 + I * 613 + 11,
                         Body, bGround ? Under : static_cast<float>(Spot.Where.Z));
            Beasts.Add(What);
            ObstacleActors.Add(What);
            BeastUp[I] = true;
            ++Loose;
        }
        else if (Away > LivingSleep)
        {
            for (int32 J = Beasts.Num() - 1; J >= 0; --J)
            {
                AAHBeast* What = Beasts[J].Get();
                if (!IsValid(What) || FVector::Dist2D(What->Casa, Spot.Where) > 20.f) continue;
                ObstacleActors.Remove(static_cast<AActor*>(What));
                Beasts.RemoveAt(J);
                What->Destroy();
                --Loose;
            }
            BeastUp[I] = false;
        }
    }
}

/**
 * Picks the pack up.
 *
 * Walking onto it, rather than a key: the pack is the only thing in the valley
 * that can be collected, and a prompt for one object in a kilometre of world is
 * a prompt nobody will still remember by the time he finds it.
 */
void AAHGameMode::TakePrize()
{
    if (bPrizeTaken || !Plan.Errand.bValid) return;
    bPrizeTaken = true;
    if (ErrandStage < 2) ErrandStage = 2;
    if (IsValid(PrizeActor))
    {
        ObstacleActors.Remove(PrizeActor);
        PrizeActor->Destroy();
    }
    PrizeActor = nullptr;
    if (auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Hero->AddLog(TEXT("Voce pegou a mochila do carroceiro."));
        Hero->AddLog(TEXT("MISSAO: leve a mochila de volta ao carroceiro, perto de onde voce chegou."));
        Hero->Feedback = TEXT("Mochila recuperada");
    }
    UE_LOG(LogTemp, Display, TEXT("AH_RECADO mochila recolhida"));
}

void AAHGameMode::DrawInBystanders(AAHCharacter* NoiseSource)
{
    if (bExploring || !bStarted || bFinished) return;
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;

    int32 Foes = 0;
    for (const auto& Fighter : Order)
        if (IsValid(Fighter) && Fighter->bEnemy && Fighter->IsAlive()) ++Foes;
    if (Foes >= FightCap) return;

    for (int32 I = 0; I < Camps.Num(); ++I)
    {
        if (Camps[I].bCleared || !Camps[I].bAwake) continue;
        for (const auto& Watcher : Camps[I].Foes)
        {
            if (Foes >= FightCap) return;
            AAHCharacter* Bystander = Watcher.Get();
            if (!IsValid(Bystander) || !Bystander->IsAlive()) continue;
            bool bAlready = false;
            for (const auto& Listed : Order)
                if (Listed.Get() == Bystander) { bAlready = true; break; }
            if (bAlready) continue;
            const bool bHeard=NoiseSource && CanWitness(Bystander,NoiseSource);
            if(!bHeard && !NoticesHero(Bystander,Hero,JoinRange)) continue;

            // Appended, never sorted in: re-sorting the order mid-round moves
            // whoever is acting out from under ActiveIndex. He acts last this
            // round and takes his proper place from the next one.
            FRandomStream Late; Late.GenerateNewSeed();
            Bystander->Initiative = Late.RandRange(1, 20) + Bystander->InitiativeBonus;
            Order.Add(Bystander);
            Camps[I].bTouched = true;
            FightingCamps.AddUnique(I);
            ++Foes;
            Hero->AddLog(FString::Printf(TEXT("%s ouviu a briga e veio."), *Bystander->EnemyName));
            UE_LOG(LogTemp, Display, TEXT("AH_BRIGA %s entrou na briga (acampamento %d)"),
                   *Bystander->EnemyName, I);
        }
    }
}

/** What the HUD writes about the errand, in one line, or nothing. */
FString AAHGameMode::ErrandLine() const
{
    if (!Plan.Errand.bValid) return FString();
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector Eye = Hero ? Hero->GetActorLocation() : HeroSpawn;
    switch (ErrandStage)
    {
    case 0:
        return FString::Printf(TEXT("RECADO   fale com o carroceiro  %.0f m  %s"),
            FVector::Dist2D(Eye, Plan.Errand.Giver) / 100.0,
            *AHCompass(Eye, Plan.Errand.Giver));
    case 1:
        return FString::Printf(TEXT("MISSAO   a mochila na masmorra  %.0f m  %s"),
            FVector::Dist2D(Eye, Plan.Errand.Prize) / 100.0,
            *AHCompass(Eye, Plan.Errand.Prize));
    case 2:
        return FString::Printf(TEXT("MISSAO   leve a mochila ao carroceiro  %.0f m  %s"),
            FVector::Dist2D(Eye, Plan.Errand.Giver) / 100.0,
            *AHCompass(Eye, Plan.Errand.Giver));
    default:
        return FString(TEXT("RECADO   cumprido"));
    }
}

/**
 * Talks to whoever is nearest, if anybody is near enough.
 *
 * Three and a half metres, which is the same figure the generator keeps clear
 * around the carter -- so a spot that was reserved for a conversation is a spot
 * a conversation can happen in.
 */
bool AAHGameMode::TalkToNearest()
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return false;
    AAHVillager* Nearest = nullptr;
    float Best = 360.f;
    for (const TObjectPtr<AAHVillager>& Who : Folk)
    {
        if (!IsValid(Who)) continue;
        const float Away = FVector::Dist2D(Hero->GetActorLocation(), Who->GetActorLocation());
        if (Away < Best) { Best = Away; Nearest = Who.Get(); }
    }
    if (!Nearest)
    {
        Hero->Feedback = TEXT("Ninguem por perto para conversar");
        return false;
    }
    Nearest->Conversa(Hero);
    TalkingTo = Nearest;
    TalkUntil = GetWorld()->GetTimeSeconds() + 7.f;
    return true;
}

bool AAHGameMode::EngageWith(AAHCharacter* Foe)
{
    if (!bExploring || !IsValid(Foe) || !Foe->bEnemy || !Foe->IsAlive()) return false;
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const bool bAmbush = Hero && Hero->bSneaking;
    for (int32 I = 0; I < Camps.Num(); ++I)
        for (const auto& Member : Camps[I].Foes)
            if (Member.Get() == Foe) { BeginCombat(I, bAmbush, Foe); return true; }
    return false;
}

void AAHGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickWorldClock(DeltaSeconds);
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero) return;
    // BuildArena runs from BeginPlay, before the game mode has spawned a pawn
    // to place, so the first tick that has one finishes the job.
    if (!bHeroPlaced) PlaceHero();
    if (!Hero->bCharacterReady || Hero->bPreparingSpells)
    {
        // Once per change, not once per frame. Both of these are legitimate
        // states -- picking a class, choosing spells -- and both of them stop
        // the exploring tick from handing the player a turn, so from the
        // outside they look exactly like a character that cannot move.
        const int32 Gate = (Hero->bCharacterReady ? 2 : 0) + (Hero->bPreparingSpells ? 1 : 0);
        if (Gate != LastGateLogged)
        {
            LastGateLogged = Gate;
            UE_LOG(LogTemp, Display, TEXT("AH_READY pronto=%d preparando magias=%d"),
                   Hero->bCharacterReady ? 1 : 0, Hero->bPreparingSpells ? 1 : 0);
        }
        return;
    }
    if (LastGateLogged != 2)
    {
        LastGateLogged = 2;
        UE_LOG(LogTemp, Display, TEXT("AH_READY o heroi pode agir"));
    }
    // Asked a second time, once play has actually started. The first call
    // happens inside BuildArena during BeginPlay, and geometry spawned moments
    // earlier is not always in the navigation octree yet -- so a build ordered
    // then can be a build over a world that is not all there.
    // Only once the hero is standing somewhere, because he is the navigation
    // invoker now: asking Recast to build before anything is invoking it
    // builds nothing, and this gate only fires once.
    if (!bNavChecked && bHeroPlaced) { bNavChecked = true; EnsureNavigation(); }

    /**
     * And then keep asking, every second, until it says yes.
     *
     * This is the line that would have saved two rounds. The navmesh around
     * an invoker arrives a few seconds after the invoker does, so the answer
     * that matters is not the first one -- it is the first YES, and how long
     * it took. Silence after thirty seconds is the real failure, and now it
     * has its own words and cannot be confused with the startup NO.
     */
    if (bNavChecked && !bNavConfirmed)
    {
        NextNavPoll  -= DeltaSeconds;
        NavWaitedFor += DeltaSeconds;
        if (NextNavPoll <= 0.f)
        {
            NextNavPoll = 1.f;
            if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
            {
                const FVector Where = Hero ? Hero->GetActorLocation() : HeroSpawn;
                FNavLocation Landed;
                if (Nav->ProjectPointToNavigation(Where, Landed, FVector(300.f, 300.f, 600.f)))
                {
                    bNavConfirmed = true;
                    UE_LOG(LogTemp, Display,
                           TEXT("AH_NAV malha pronta sob o heroi depois de %.1f s"), NavWaitedFor);
                }
                else if (NavWaitedFor > 30.f)
                {
                    bNavConfirmed = true;   // stop asking; say it once, loudly
                    UE_LOG(LogTemp, Warning,
                           TEXT("AH_NAV TRINTA SEGUNDOS e ainda nao ha malha sob o heroi -- ")
                           TEXT("isso sim e um problema de navegacao"));
                }
            }
        }
    }

    if (bExploring)
    {
        if (!Hero->IsAlive()) return;
        // The turn machinery stays switched on so every existing check keeps
        // working unchanged; the budget is simply topped up so it never bites.
        // Cheaper and far less risky than teaching a hundred call sites about a
        // second mode.
        Hero->bTurnActive   = true;
        Hero->bRoaming      = true;
        Hero->Turn.Movement = Hero->BaseMovement * 8.f;
        // And the reaction is back.
        //
        // FinishTurn deliberately does not clear it -- a reaction lasts until
        // your next turn, which is the rule. But nothing reset it between
        // FIGHTS either, so an opportunity attack made in the last seconds of
        // one camp left you walking into the next one unable to react at all.
        // Out here there is no round to be spending it in.
        Hero->Turn.bReaction = true;

        // And the action, and the bonus action, and the per-turn spell locks.
        //
        // Only movement and the reaction were being topped up, so whatever you
        // spent on the last turn of a fight stayed spent for the whole walk to
        // the next one -- which meant you could never buff on the way in. Out of
        // initiative there is no action economy to respect: the spell slots are
        // the cost, and they are not refunded here.
        Hero->Turn.bAction = true;
        Hero->Turn.bBonus  = true;
        Hero->bBonusSpellCast = false;
        Hero->bLeveledActionSpellCast = false;

        // Build the camps you are near, forget the ones behind you. Once a
        // second: the pass walks twenty camps and measures a distance each,
        // which is nothing, but there is no reason to do it every frame.
        NextStream -= DeltaSeconds;
        if (NextStream <= 0.f) { NextStream = 1.f; StreamCamps(); StreamLiving(); UpdateSideQuests(); }

        /**
         * Walk onto the pack and it is yours.
         *
         * Two metres, and no key to press. There is exactly one collectable
         * object in a kilometre of valley; a prompt for it would be a control
         * the player reads once, at the start, and has forgotten by the time he
         * reaches the room it is in.
         */
        if (Plan.Errand.bValid && !bPrizeTaken
            && FVector::Dist2D(Hero->GetActorLocation(), Plan.Errand.Prize) < 200.f)
            TakePrize();

        // A conversation ends by walking away from it, which is how people end
        // conversations.
        if (TalkingTo.IsValid()
            && (GetWorld()->GetTimeSeconds() > TalkUntil
             || FVector::Dist2D(Hero->GetActorLocation(),
                                TalkingTo->GetActorLocation()) > 700.f))
            TalkingTo.Reset();

        for (int32 I = 0; I < Camps.Num(); ++I)
        {
            if (Camps[I].bCleared) continue;
            /**
             * A SLEEPING camp has no actors, and "nobody standing" must not be
             * read as "everybody here is dead".
             *
             * Without this line, switching on streaming would have marked
             * every camp in the world cleared on the first tick -- the whole
             * map emptied, silently, by a rule that was correct for five
             * always-spawned camps and wrong for twenty streamed ones.
             */
            if (!Camps[I].bAwake) continue;
            bool bStanding = false;
            for (const auto& Foe : Camps[I].Foes)
                if (Foe.IsValid() && Foe->IsAlive()) bStanding = true;
            if (!bStanding) { Camps[I].bCleared = true; continue; }
            const float Notice=Camps[I].bIndoor?JoinRange:Camps[I].Alert;
            for(const auto& Foe:Camps[I].Foes)
                if(Foe.IsValid() && NoticesHero(Foe.Get(),Hero,Notice))
                { BeginCombat(I,false,Foe.Get()); break; }
            if(!bExploring) break;
        }
        return;
    }

    if (!bStarted || bFinished) return;

    bool HeroAlive = false, HeroSaving = false, EnemyAlive = false;
    for (const auto& Character : Order)
    {
        if (!Character) continue;
        if (Character->bEnemy) EnemyAlive |= Character->IsAlive();
        else { HeroAlive |= Character->IsAlive(); HeroSaving |= Character->IsDowned(); }
    }

    if (!HeroAlive && !HeroSaving)
    {
        // Defeat stays a full stop. The world does not hand itself back.
        bFinished = true;
        for (const auto& Character : Order) if (Character) Character->FinishTurn();
        return;
    }
    if (!EnemyAlive)
    {
        if (Hero->IsAlive()) Hero->GainExperience(300);
        EndCombat();
        return;
    }

    // Anybody near enough to see this join in, once a second. The fight moves
    // while it runs, so this cannot only be asked when it starts.
    NextJoinCheck -= DeltaSeconds;
    if (NextJoinCheck <= 0.f) { NextJoinCheck = 1.f; DrawInBystanders(); }

    auto* Active = ActiveCharacter();
    if (Active && Active->bEnemy && !Active->IsBusy())
    {
        if (!HeroAlive) { EndTurn(Active); return; }
        // Surprised: they spend the first round realising what happened.
        if (bSurprise && Round == 1)
        {
            Active->AddLog(FString::Printf(TEXT("%s foi pego de surpresa e perde o turno."), *Active->EnemyName));
            EndTurn(Active);
            return;
        }
        const float Elapsed = GetWorld()->GetTimeSeconds() - TurnStarted;
        const bool OutOfReach = FVector::Dist2D(Hero->GetActorLocation(), Active->GetActorLocation()) > 190.f;
        if ((!Active->Turn.bAction && Elapsed > 1.2f)
         || (Active->Turn.Movement <= 1.f && OutOfReach && Elapsed > 2.f)
         || Elapsed > 10.f) EndTurn(Active);
    }
}

bool AAHGameMode::EndTurn(AAHCharacter* Requester)
{
    if(!bStarted || bFinished || !Requester || Requester!=ActiveCharacter() || Requester->IsBusy()) return false;
    Requester->FinishTurn();
    bool HasLivingHero=false;
    for(const auto& Character:Order) if(!Character->bEnemy && Character->IsAlive()) HasLivingHero=true;
    bool Found=false;
    for(int32 I=0;I<Order.Num();++I)
    {
        ActiveIndex=(ActiveIndex+1)%Order.Num();
        if(ActiveIndex==0) ++Round;
        const auto* Next=ActiveCharacter();
        if(Next && (Next->IsAlive() || Next->IsDowned()) && (!Next->bEnemy || HasLivingHero)) { Found=true; break; }
    }
    if(!Found) { bFinished=true; return true; }
    for(auto& Actor:Order) if(Actor) Actor->bSneakUsed=false;
    ActiveCharacter()->StartTurn();
    TurnStarted=GetWorld()->GetTimeSeconds();
    return true;
}

bool AAHGameMode::IsWorldCleared() const
{
    if (Camps.Num() == 0) return false;
    for (const FAHCamp& Camp : Camps) if (!Camp.bCleared) return false;
    return true;
}

bool AAHGameMode::NextEncounter()
{
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Hero || (!Hero->IsAlive() && !Hero->bStabilized)) return false;
    if (Hero->Level == 4 && Hero->Feat == 0) return false;      // pick the feat first
    // Only once the valley is quiet, or after a defeat has settled.
    if (!IsWorldCleared() && !bFinished) return false;

    Hero->Rest();
    Hero->bPreparingSpells = Hero->MaxSpellSlots(1) > 0;
    Order.Reset(); ActiveIndex = 0; Round = 1; bStarted = false; bFinished = false;

    BuildArena(FreshSeed());
    Hero->AddLog(TEXT("Um mundo novo."));
    return true;
}
