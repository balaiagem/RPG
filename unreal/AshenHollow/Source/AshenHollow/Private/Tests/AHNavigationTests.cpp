#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AHGameMode.h"
#include "NavigationSystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
// Exercise the real map, controller, equipment and class gestures. Isolated rules
// tests cannot detect cosmetic geometry accidentally invalidating the navmesh.
class FAHWalkAllClasses final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(), Requested=0;
    int32 ClassIndex=0, Phase=0;
    TWeakObjectPtr<AAHCharacter> Hero;
    FVector Start, ReturnStart;
    FVector BudgetStart;
    float FirstDeathSaveTime=0;
public:
    explicit FAHWalkAllClasses(FAutomationTestBase* InTest):Test(InTest){}
    virtual bool Update() override
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.WorldType==EWorldType::Game) { World=Context.World(); break; }
        auto* PC=World?Cast<AAHPlayerController>(World->GetFirstPlayerController()):nullptr;
        if(!PC)
        {
            if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Game controller unavailable")); return true; }
            return false;
        }
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if(auto* Mode=World->GetAuthGameMode()) Mode->SetActorTickEnabled(false);
        for(TActorIterator<AAHCharacter> It(World);It;++It) if(It->bEnemy) It->SetActorTickEnabled(false);
        if(Phase==0)
        {
            if(!Nav || Nav->IsNavigationBuildInProgress())
            {
                if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Navigation never became ready")); return true; }
                return false;
            }
            if(ClassIndex==0)
            {
                Hero=Cast<AAHCharacter>(PC->GetPawn());
                if(!Test->TestNotNull(TEXT("Player character exists"),Hero.Get())) return true;
                Start=Hero->GetActorLocation();
            }
            else
            {
                FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* Next=World->SpawnActor<AAHCharacter>(Start,FRotator::ZeroRotator,P);
                if(!Test->TestNotNull(TEXT("Next class character spawns"),Next)) return true;
                auto* Old=Hero.Get(); PC->Possess(Next); Hero=Next; if(Old) Old->Destroy();
            }
            PC->CombatCommand(FName(*FString::Printf(TEXT("Race%d"),ClassIndex)));
            PC->CombatCommand(FName(*FString::Printf(TEXT("Class%d"),ClassIndex)));
            Test->TestEqual(TEXT("Class selection routes through controller"),static_cast<int32>(Hero->HeroClass),ClassIndex);
            auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
            AAHCharacter* Enemy=nullptr;
            for(TActorIterator<AAHCharacter> It(World);It;++It) if(It->bEnemy) { Enemy=*It; break; }
            if(!Mode || !Enemy) { Test->AddError(TEXT("Initiative fixture unavailable")); return true; }
            Enemy->MaxHealth=Enemy->Health=100;
            Enemy->FinishTurn(); Hero->FinishTurn();
            Mode->Order.Reset(); Mode->Order.Add(Enemy);
            Mode->ActiveIndex=0; Mode->Round=1; Mode->bStarted=false; Mode->bFinished=false;
            Mode->Tick(0.f); // Use the production initiative roll and turn assignment.
            if(!Test->TestTrue(TEXT("Initiative starts the encounter"),Mode->bStarted && Mode->Order.Num()==2)) return true;
            Test->TestTrue(TEXT("Initiative is descending, with player winning ties"),
                Mode->Order[0]->Initiative>Mode->Order[1]->Initiative ||
                (Mode->Order[0]->Initiative==Mode->Order[1]->Initiative && Mode->Order[0]==Hero.Get()));
            Test->TestTrue(TEXT("Only initiative winner can act"),Mode->ActiveCharacter()->CanAct() && Hero->bTurnActive!=Enemy->bTurnActive);
            Test->AddInfo(FString::Printf(TEXT("Class %d initiative: hero=%d enemy=%d first=%s"),ClassIndex,Hero->Initiative,Enemy->Initiative,Mode->ActiveCharacter()==Hero.Get()?TEXT("hero"):TEXT("enemy")));
            if(Mode->ActiveCharacter()==Enemy)
            {
                PC->CombatCommand(TEXT("Dash"));
                Test->TestFalse(TEXT("Controller rejects dash outside player turn"),Hero->bDashing);
                Test->TestTrue(TEXT("Enemy turn hands control to player"),Mode->EndTurn(Enemy));
            }
            Test->TestTrue(TEXT("Player has control before movement"),Hero->CanAct());
            Test->TestEqual(TEXT("New player turn uses ancestry movement"),Hero->Turn.Movement,Hero->BaseMovement);
            FNavLocation Point;
            if(!Test->TestTrue(TEXT("Destination projects onto navigation"),Nav->ProjectPointToNavigation(Start+FVector(250,0,-90),Point,FVector(80,80,180)))) return true;
            Test->TestTrue(TEXT("Controller accepts initial movement"),PC->RequestMoveToLocation(Point.Location));
            Phase=1; Requested=FPlatformTime::Seconds(); return false;
        }
        if(!Hero.IsValid()) { Test->AddError(TEXT("Character disappeared during movement")); return true; }
        if(FPlatformTime::Seconds()-Requested<3) return false;
        if(Phase==4)
        {
            auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
            if(!Hero->IsAlive())
            {
                Test->TestTrue(TEXT("Death saves continue without waiting ten seconds"),Hero->DeathSaveRollTime>FirstDeathSaveTime);
                Test->TestTrue(TEXT("Empty enemy turn is skipped between saves"),Mode && (Mode->bFinished || Mode->ActiveCharacter()==Hero.Get()));
            }
            Hero->Health=0; Hero->bDowned=true; Hero->bStabilized=true;
            Mode->Tick(0.f);
            Test->TestTrue(TEXT("Stabilization ends this solo encounter without an endless turn loop"),Mode->bFinished);
            return true;
        }
        if(Phase==5)
        {
            const float Travel=FVector::Dist2D(BudgetStart,Hero->GetActorLocation());
            Test->AddInfo(FString::Printf(TEXT("Class %d budget cap: travelled=%.2f cm remaining=%.2f cm"),ClassIndex,Travel,Hero->Turn.Movement));
            Test->TestTrue(TEXT("Path stops at remaining 120cm budget"),FMath::IsNearlyEqual(Travel,120.f,2.f));
            Test->TestTrue(TEXT("Exhausted movement is nonnegative and at most 1cm"),Hero->Turn.Movement>=0.f && Hero->Turn.Movement<=1.f);
            auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
            PC->CombatCommand(TEXT("EndTurn"));
            if(!Test->TestTrue(TEXT("Controller end-turn transfers initiative"),Mode && Mode->ActiveCharacter() && Mode->ActiveCharacter()!=Hero.Get() && !Hero->bTurnActive)) return true;
            Test->TestTrue(TEXT("Next round returns control"),Mode->EndTurn(Mode->ActiveCharacter()));
            Test->TestEqual(TEXT("Next own turn restores ancestry movement"),Hero->Turn.Movement,Hero->BaseMovement);
            Test->TestTrue(TEXT("Next own turn restores action and bonus"),Hero->Turn.bAction && Hero->Turn.bBonus && Hero->CanAct());
            BudgetStart=Hero->GetActorLocation();
            Test->TestTrue(TEXT("Controller accepts a nearby 20cm destination"),PC->RequestMoveToLocation(BudgetStart+FVector(20,0,-90)));
            Phase=7; Requested=FPlatformTime::Seconds(); return false;
        }
        if(Phase==7) Test->TestTrue(TEXT("Short clicks move instead of being swallowed by capsule acceptance radius"),FVector::Dist2D(BudgetStart,Hero->GetActorLocation())>10.f);
        if(Phase==1)
        {
            const float Travel=FVector::Dist2D(Start,Hero->GetActorLocation());
            Test->AddInfo(FString::Printf(TEXT("Class %d walked %.1f cm with equipment"),ClassIndex,Travel));
            Test->TestTrue(TEXT("Equipped character leaves spawn"),Travel>100);
            Test->TestTrue(TEXT("Movement spends the turn budget"),Hero->Turn.Movement<800);
            PC->StopMovement();
            if(ClassIndex==0 || ClassIndex==2) Hero->Health-=3;
            Test->TestTrue(TEXT("Class ability is available"),Hero->CanUseClassAbility());
            PC->CombatCommand(TEXT("Heal"));
            Test->TestTrue(TEXT("Class ability starts its animation"),Hero->IsBusy());
            ReturnStart=Hero->GetActorLocation();
            Test->TestTrue(TEXT("Movement click during animation is buffered"),PC->RequestMoveToLocation(Start-FVector(0,0,90)));
            Phase=2; Requested=FPlatformTime::Seconds(); return false;
        }
        if(Phase==2)
        {
            Test->TestTrue(TEXT("Input is released after class animation"),Hero->CanAct());
            FNavLocation Point;
            if(!Test->TestTrue(TEXT("Spawn remains navigable after effects"),Nav && Nav->ProjectPointToNavigation(Start-FVector(0,0,90),Point,FVector(80,80,180)))) return true;
            Test->TestTrue(TEXT("Buffered click executes without another click"),FVector::Dist2D(Start,Hero->GetActorLocation())<65);
            const bool ActionBefore=Hero->Turn.bAction;
            PC->CombatCommand(TEXT("Inspect"));
            Test->TestEqual(TEXT("Inspect is free"),Hero->Turn.bAction,ActionBefore);
            Phase=3; Requested=FPlatformTime::Seconds(); return false;
        }
        if(Phase==3)
        {
            Test->TestTrue(TEXT("Character walks again after class animation and effects"),FVector::Dist2D(ReturnStart,Hero->GetActorLocation())>100);
            Test->TestTrue(TEXT("Return path reaches original spawn"),FVector::Dist2D(Start,Hero->GetActorLocation())<65);
            PC->StopMovement(); Hero->Turn.Movement=120.f;
            BudgetStart=Hero->GetActorLocation();
            FNavLocation Point;
            if(!Test->TestTrue(TEXT("Budget test destination is navigable"),Nav && Nav->ProjectPointToNavigation(BudgetStart+FVector(400,0,-90),Point,FVector(80,80,180)))) return true;
            Test->TestTrue(TEXT("Controller accepts budget-limited movement"),PC->RequestMoveToLocation(Point.Location));
            Phase=5; Requested=FPlatformTime::Seconds(); return false;
        }
        PC->StopMovement(); Phase=0; Requested=0;
        if(++ClassIndex<4) return false;
        auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
        AAHCharacter* Enemy=nullptr;
        for(TActorIterator<AAHCharacter> It(World);It;++It) if(It->bEnemy) { Enemy=*It; break; }
        if(!Mode || !Enemy) { Test->AddError(TEXT("Turn handoff fixture unavailable")); return true; }
        Mode->Order.Reset(); Mode->Order.Add(Hero.Get()); Mode->Order.Add(Enemy);
        Hero->FinishTurn();
        Mode->ActiveIndex=1; Mode->bStarted=true; Mode->bFinished=false;
        Hero->Health=0; Hero->bDowned=true; Hero->bStabilized=false; Hero->DeathSuccesses=0; Hero->DeathFailures=0;
        Enemy->StartTurn(); Mode->TurnStarted=World->GetTimeSeconds();
        Mode->Tick(0.f);
        Test->TestFalse(TEXT("Encounter does not end before death saves"),Mode->bFinished);
        Test->TestTrue(TEXT("Empty enemy turn immediately advances to death save"),Mode->ActiveCharacter()==Hero.Get());
        FirstDeathSaveTime=Hero->DeathSaveRollTime;
        Phase=4; Requested=FPlatformTime::Seconds(); return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHNavigationTest,"AshenHollow.Navigation.AllClasses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FAHNavigationTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FAHWalkAllClasses(this)); return true;
}
#endif
