#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

bool AAHCharacter::FindCombatPosition(AAHCharacter* Target, bool bKeepDistance, FVector& Out) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav || !Target) return false;
    float Best=MAX_flt;
    const FVector TargetFeet=Target->GetNavAgentLocation();
    const float BaseAngle=(GetActorLocation()-Target->GetActorLocation()).Rotation().Yaw;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyPosition),false,this);
    Query.AddIgnoredActor(Target);
    for(int32 Ring=0;Ring<2;++Ring)
        for(int32 Sector=0;Sector<12;++Sector)
        {
            const float Radius=bKeepDistance ? (Ring==0?650.f:400.f) : (Ring==0?145.f:175.f);
            const float Angle=FMath::DegreesToRadians(BaseAngle+Sector*30.f);
            FNavLocation Projected;
            if(!Nav->ProjectPointToNavigation(TargetFeet+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius,
                    Projected,FVector(70,70,200))) continue;
            const FVector Body=Projected.Location+FVector(0,0,98);
            if(GetWorld()->OverlapBlockingTestByChannel(Body,FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(43,94),Query)) continue;
            // A reachable position behind a wall is still useful for approaching,
            // but strongly prefer a place from which the action can actually fire.
            FHitResult Hit;
            const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Hit,Body+FVector(0,0,35),
                    Target->GetActorLocation()+FVector(0,0,35),ECC_Visibility,Query);
            if(!bKeepDistance && FVector::Dist2D(Projected.Location,TargetFeet)>185) continue;
            UNavigationPath* Route=Nav->FindPathToLocationSynchronously(GetWorld(),GetNavAgentLocation(),Projected.Location);
            if(!Route || !Route->IsValid() || Route->IsPartial()) continue;
            const float Cost=Route->GetPathLength()+(bBlocked?3000.f:0.f)
                +(bKeepDistance?FMath::Abs(FVector::Dist2D(Projected.Location,TargetFeet)-650.f)*.6f:0.f)
                +(FVector::Dist2D(Projected.Location,GetActorLocation())<70?500.f:0.f);
            if(Cost<Best) { Best=Cost; Out=Projected.Location; }
        }
    return Best<MAX_flt;
}

void AAHCharacter::TickEnemyAI(float Now)
{
    if(!bEnemy || !IsAlive()) return;
    auto* AI=Cast<AAIController>(GetController());
    auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>();
    if(!AI || !Mode) return;
    if(Mode->IsExploring())
    {
        // Short patrols remain inside the camp/room and use full navmesh paths.
        // They do not borrow combat actions or movement from the next turn.
        bPatrolling=true;
        if(Now<PatrolAt || IsBusy()) return;
        PatrolAt=Now+Dice.FRandRange(3.5f,8.f);
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        FNavLocation Point;
        /**
         * How far a camp's guard may wander from his fire.
         *
         * It was 180 cm for everybody, and 180 cm is a shuffle: from across a
         * clearing it reads as a man standing still twitching, which is what
         * "o movimento dos npcs ta meio burrinho" was about. Outdoors a camp is
         * six or seven metres across and the ring it keeps clear is 650, so
         * four and a half metres is a patrol of his own camp and no further --
         * he still cannot wander out of it and pull a fight the player did not
         * choose.
         *
         * Indoors it STAYS short, and that is not an oversight: a dungeon room
         * is four and a half metres across, and a guard who strolls out of his
         * room brings his room's fight into the corridor.
         */
        const float Beat=bIndoorFoe?170.f:450.f;
        if(Nav && Nav->GetRandomReachablePointInRadius(EnemyHome,Beat,Point))
        {
            UNavigationPath* Route=Nav->FindPathToLocationSynchronously(GetWorld(),GetNavAgentLocation(),Point.Location);
            // The path may not be twice the walk: a point four metres away that
            // costs twelve metres of walking is on the far side of something,
            // and a guard who goes round the tent to stand where he was is the
            // shuffle again in a bigger costume.
            if(Route && Route->IsValid() && !Route->IsPartial()
               && Route->GetPathLength()<Beat*2.4f)
                AI->MoveToLocation(Point.Location,25,false,true,false,false,nullptr,false);
        }
        return;
    }
    if(bPatrolling) { bPatrolling=false; AI->StopMovement(); }
    if(!CanAct() || Now<NextThink) return;
    NextThink=Now+.25f;
    if(Now-Mode->TurnStarted<.65f) return;
    auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Hero || !Hero->IsAlive()) return;
    const float Distance=FVector::Dist2D(GetActorLocation(),Hero->GetActorLocation());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySight),false,this);
    Query.AddIgnoredActor(Hero);
    FHitResult Hit;
    const bool bSight=!GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation()+FVector(0,0,35),
        Hero->GetActorLocation()+FVector(0,0,35),ECC_Visibility,Query);

    // A failed spell (wall/range/resource) must not starve movement forever.
    if(CanUseClassAbility())
    {
        const bool bWorth=HeroClass==EAHHeroClass::Barbarian ? Distance<600 :
            (HeroClass==EAHHeroClass::Wizard || HeroClass==EAHHeroClass::Sorcerer)
                ? bSight && Distance<1800 : Health*2<MaxHealth;
        if(bWorth)
        {
            const bool HadAction=Turn.bAction, HadBonus=Turn.bBonus;
            UseClassAbility();
            if(IsBusy() || HadAction!=Turn.bAction || HadBonus!=Turn.bBonus) return;
        }
    }
    if(bSight && CanUseRacialAbility() && Distance<=BreathReach)
    {
        const bool HadAction=Turn.bAction;
        UseRacialAbility();
        if(IsBusy() || HadAction!=Turn.bAction) return;
    }
    const bool bShooter=HasRangedAttack();
    if(bSight && Turn.bAction)
    {
        if(bShooter && Distance<=RangedReach() && Distance>ThreatReach && TryRangedAttack(Hero)) return;
        if(Distance<=180 && TryAttack(Hero)) return;
    }
    if(!Turn.bAction || Turn.Movement<=1) { AI->StopMovement(); return; }

    // Keep an accepted path alive. Restarting MoveToActor five times a second
    // prevented progress at corners and against other enemies.
    const bool bMoving=AI->GetMoveStatus()==EPathFollowingStatus::Moving;
    bool bStuck=false;
    if(Now-ProgressAt>=1.f)
    {
        bStuck=bMoving && FVector::Dist2D(GetActorLocation(),ProgressLocation)<18;
        ProgressLocation=GetActorLocation(); ProgressAt=Now;
    }
    if(bMoving && !bStuck && Now<RepathAt && FVector::Dist2D(LastPursuedLocation,Hero->GetActorLocation())<120) return;
    if(bStuck) AI->StopMovement();
    FVector Goal;
    RepathAt=Now+2.f;
    LastPursuedLocation=Hero->GetActorLocation();
    if(FindCombatPosition(Hero,bShooter,Goal))
    {
        const auto Result=AI->MoveToLocation(Goal,12,false,true,false,false,nullptr,false);
        if(Result!=EPathFollowingRequestResult::Failed)
        {
            FailedPaths=0;
            Feedback=bShooter?TEXT("Buscando linha de tiro"):TEXT("Contornando obstaculos");
            return;
        }
    }
    // Bound retries while tiles are arriving; an unreachable target produces a
    // defensive turn rather than ten seconds of running into the same wall.
    NextThink=Now+.8f;
    if(++FailedPaths>=3)
    {
        AI->StopMovement();
        Dodge();
        Hero->AddLog(EnemyName+TEXT(" assume guarda: passagem bloqueada."));
    }
}
