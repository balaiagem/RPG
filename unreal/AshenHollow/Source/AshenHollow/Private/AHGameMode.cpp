#include "AHGameMode.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHCombatHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
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
#include "GameFramework/CharacterMovementComponent.h"
const FVector AAHGameMode::HeroSpawn(0.f, -500.f, 110.f);
const FVector AAHGameMode::FoeSpawn (0.f,  500.f, 110.f);

int32 AAHGameMode::FreshSeed()
{
    return static_cast<int32>(FPlatformTime::Cycles64() ^ static_cast<uint64>(FDateTime::Now().GetTicks()));
}

void AAHGameMode::BuildArena(int32 Seed)
{
    ArenaSeed = Seed;
    for (auto& Old : ObstacleActors) if (IsValid(Old)) Old->Destroy();
    ObstacleActors.Reset();

    // Measure the meshes the generator is about to arrange. A fence can only be
    // tiled without gaps or overlaps by someone who knows how long a panel is,
    // and that is a fact about the asset, not a constant worth guessing. Asking
    // the mesh for its bounds is cheap and needs nothing spawned; the answers are
    // cached because the same fence is asked about dozens of times in a row.
    TMap<FString, FVector> Measured;
    auto MeasureMesh = [&Measured](const FString& Path) -> FVector
    {
        if (const FVector* Known = Measured.Find(Path)) return *Known;
        FVector Extent(100.0, 100.0, 100.0);
        if (const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
            Extent = Mesh->GetBounds().BoxExtent;
        Measured.Add(Path, Extent);
        return Extent;
    };

    FRandomStream Dice(Seed);
    Plan = AHArena::Build(Dice, HeroSpawn, FoeSpawn, MeasureMesh);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (FAHArenaPiece& Piece : Plan.Pieces)
    {
        AActor* Built = nullptr;

        if (Piece.MeshPath.Contains(TEXT("/blueprints/")))
        {
            // A blueprint spawns by its generated class. Going through the editor's
            // actor factories instead would return nothing outside the editor.
            const FString ClassPath = Piece.MeshPath + TEXT(".") +
                                      FPaths::GetCleanFilename(Piece.MeshPath) + TEXT("_C");
            if (UClass* Made = LoadClass<AActor>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
                Built = GetWorld()->SpawnActor<AActor>(Made, Piece.Location, Piece.Rotation, Params);
        }
        else if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            auto* Prop = GetWorld()->SpawnActor<AStaticMeshActor>(Piece.Location, Piece.Rotation, Params);
            if (Prop)
            {
                if (auto* Body = Prop->GetStaticMeshComponent())
                {
                    // Movable before anything else: a Static actor spawned at
                    // runtime refuses to be moved or scaled afterwards.
                    Body->SetMobility(EComponentMobility::Movable);
                    Body->SetStaticMesh(Mesh);
                    if (!Piece.MaterialPath.IsEmpty())
                        if (UMaterialInterface* Skin = LoadObject<UMaterialInterface>(
                                nullptr, *Piece.MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
                            Body->SetMaterial(0, Skin);
                }
                Built = Prop;
            }
        }

        if (!Built) continue;

        // Everything spawned here must be fully dynamic.
        //
        // The village pack's blueprints are authored for a baked level, so their
        // meshes and lights arrive on Static or Stationary mobility. Spawned
        // after load that is wrong twice over: the level starts asking to have
        // its lighting rebuilt -- the message on screen -- and a static
        // primitive created at runtime cannot be lit properly anyway, because
        // the lightmap it expects was never baked and never will be.
        //
        // Sweeping every scene component rather than only the meshes: a brazier
        // blueprint carries its own light, and a stationary light is the louder
        // half of the same complaint.
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
            // Measured, never assumed. The generator's numbers were spacing
            // estimates; the cover rule runs against what is actually standing there.
            Piece.Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y));
            Piece.TopZ   = static_cast<float>(Origin.Z + Extent.Z);
        }
        ObstacleActors.Add(Built);

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

    ApplyArenaLight();
    UE_LOG(LogTemp, Display, TEXT("AH_ARENA %s semente %d, %d pecas"),
           *Plan.Name, Seed, ObstacleActors.Num());
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
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (auto* Enemy = GetWorld()->SpawnActor<AAHCharacter>(AAHCharacter::StaticClass(), FoeSpawn, FRotator(0,-90,0), Params))
    {
        Enemy->BecomeEnemy();
        Order.Add(Enemy);
    }
}
AAHCharacter* AAHGameMode::ActiveCharacter() const { return Order.IsValidIndex(ActiveIndex) ? Order[ActiveIndex].Get() : nullptr; }
void AAHGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!bStarted && Hero && Hero->bCharacterReady && !Hero->bPreparingSpells && Order.Num()==1)
    {
        Order.Add(Hero);
        FRandomStream Dice; Dice.GenerateNewSeed();
        for (const auto& Character : Order) Character->Initiative = Dice.RandRange(1,20) + Character->InitiativeBonus;
        Order.StableSort([](const AAHCharacter& A,const AAHCharacter& B) { return A.Initiative>B.Initiative || (A.Initiative==B.Initiative && !A.bEnemy && B.bEnemy); });
        bStarted=true;
        TurnStarted=GetWorld()->GetTimeSeconds();
        ActiveCharacter()->StartTurn();
        Hero->AddLog(FString::Printf(TEXT("Iniciativa: você %d / inimigo %d"),Hero->Initiative,Order[0]==Hero?Order[1]->Initiative:Order[0]->Initiative));
    }
    if (!bStarted || bFinished) return;
    bool HeroAlive=false, HeroSaving=false, EnemyAlive=false;
    for(const auto& Character:Order) { if(Character->bEnemy) EnemyAlive|=Character->IsAlive(); else { HeroAlive|=Character->IsAlive(); HeroSaving|=Character->IsDowned(); } }
    if((!HeroAlive && !HeroSaving) || !EnemyAlive)
    {
        if(!EnemyAlive && Hero && Hero->IsAlive()) Hero->GainExperience(300);
        bFinished=true;
        for(const auto& Character:Order) Character->FinishTurn();
        return;
    }
    auto* Active=ActiveCharacter();
    if(Active && Active->bEnemy && !Active->IsBusy())
    {
        if(!HeroAlive) { EndTurn(Active); return; }
        const float Elapsed=GetWorld()->GetTimeSeconds()-TurnStarted;
        const bool OutOfReach=Hero && FVector::Dist2D(Hero->GetActorLocation(),Active->GetActorLocation())>190.f;
        if((!Active->Turn.bAction && Elapsed>1.2f) || (Active->Turn.Movement<=1.f && OutOfReach && Elapsed>2.f) || Elapsed>10.f) EndTurn(Active);
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

bool AAHGameMode::NextEncounter()
{
    auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!bFinished || !Hero || (!Hero->IsAlive() && !Hero->bStabilized) || (Hero->Level==4 && Hero->Feat==0)) return false;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    auto* Enemy=GetWorld()->SpawnActor<AAHCharacter>(FoeSpawn,FRotator(0,-90,0),Params);
    if(!Enemy) return false;
    for(auto& Actor:Order) if(Actor && Actor->bEnemy) Actor->Destroy();
    Enemy->BecomeEnemy(); Enemy->MaxHealth+=(Hero->Level-1)*5; Enemy->Health=Enemy->MaxHealth;
    Hero->Rest(); Hero->bPreparingSpells=Hero->MaxSpellSlots(1)>0; Hero->SetActorLocation(HeroSpawn,false,nullptr,ETeleportType::TeleportPhysics);
    // A new encounter is a new arena: the props are re-rolled, so the second
    // fight is never the first fight with a different foe standing in it.
    BuildArena(FreshSeed());
    Order.Reset(); Order.Add(Enemy); ActiveIndex=0; Round=1; bStarted=false; bFinished=false;
    return true;
}
