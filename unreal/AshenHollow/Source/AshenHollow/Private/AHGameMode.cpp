#include "AHGameMode.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHCombatHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
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

    FRandomStream Dice(Seed);
    Obstacles = AHArena::Generate(Dice, HeroSpawn, FoeSpawn);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (FAHArenaPiece& Piece : Obstacles)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath, nullptr, LOAD_NoWarn|LOAD_Quiet);
        if (!Mesh) continue;
        auto* Prop = GetWorld()->SpawnActor<AStaticMeshActor>(Piece.Location, FRotator(0.f, Piece.Yaw, 0.f), Params);
        if (!Prop) continue;
        auto* Body = Prop->GetStaticMeshComponent();
        if (Body)
        {
            // Movable before anything else: a Static actor spawned at runtime
            // refuses to be moved or scaled afterwards.
            Body->SetMobility(EComponentMobility::Movable);
            Body->SetStaticMesh(Mesh);
        }
        Prop->SetActorScale3D(FVector(Piece.Scale));

        // Measured, never assumed. The generator's radius and height are only
        // spacing estimates; the cover rule runs against the prop's real bounds,
        // and the prop is dropped so its base rests on the flagstones.
        FVector Origin, Extent;
        Prop->GetActorBounds(false, Origin, Extent);
        Piece.Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y));
        Piece.Height = static_cast<float>(Extent.Z * 2.0);
        Prop->SetActorLocation(FVector(Piece.Location.X, Piece.Location.Y,
                                       Piece.Location.Z - (Origin.Z - Extent.Z)));
        ObstacleActors.Add(Prop);
    }
    UE_LOG(LogTemp, Display, TEXT("AH_ARENA semente %d, %d obstaculos"), Seed, ObstacleActors.Num());
}

EAHCover AAHGameMode::CoverBetween(const FVector& From, const FVector& To) const
{
    return AHArena::CoverBetween(Obstacles, From, To);
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
