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
            PC->CombatCommand(FName(*FString::Printf(TEXT("Class%d"),ClassIndex)));
            Test->TestEqual(TEXT("Class selection routes through controller"),static_cast<int32>(Hero->HeroClass),ClassIndex);
            Hero->StartTurn();
            FNavLocation Point;
            if(!Test->TestTrue(TEXT("Destination projects onto navigation"),Nav->ProjectPointToNavigation(Start+FVector(250,0,-90),Point,FVector(80,80,180)))) return true;
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(PC,Point.Location);
            Phase=1; Requested=FPlatformTime::Seconds(); return false;
        }
        if(!Hero.IsValid()) { Test->AddError(TEXT("Character disappeared during movement")); return true; }
        if(FPlatformTime::Seconds()-Requested<3) return false;
        if(Phase==4)
        {
            auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
            Test->TestFalse(TEXT("Death-save timer releases the previous turn"),Hero->bTurnActive);
            Test->TestTrue(TEXT("Death-save timer transfers control through GameMode"),Mode && Mode->ActiveCharacter()!=Hero.Get() && Mode->ActiveCharacter()->bTurnActive);
            return true;
        }
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
            Phase=2; Requested=FPlatformTime::Seconds(); return false;
        }
        if(Phase==2)
        {
            Test->TestTrue(TEXT("Input is released after class animation"),Hero->CanAct());
            ReturnStart=Hero->GetActorLocation();
            FNavLocation Point;
            if(!Test->TestTrue(TEXT("Spawn remains navigable after effects"),Nav && Nav->ProjectPointToNavigation(Start-FVector(0,0,90),Point,FVector(80,80,180)))) return true;
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(PC,Point.Location);
            Phase=3; Requested=FPlatformTime::Seconds(); return false;
        }
        Test->TestTrue(TEXT("Character walks again after class animation and effects"),FVector::Dist2D(ReturnStart,Hero->GetActorLocation())>100);
        Test->TestTrue(TEXT("Return path reaches original spawn"),FVector::Dist2D(Start,Hero->GetActorLocation())<65);
        PC->StopMovement(); Phase=0; Requested=0;
        if(++ClassIndex<4) return false;
        auto* Mode=Cast<AAHGameMode>(World->GetAuthGameMode());
        AAHCharacter* Enemy=nullptr;
        for(TActorIterator<AAHCharacter> It(World);It;++It) if(It->bEnemy) { Enemy=*It; break; }
        if(!Mode || !Enemy) { Test->AddError(TEXT("Turn handoff fixture unavailable")); return true; }
        Mode->Order.Reset(); Mode->Order.Add(Hero.Get()); Mode->Order.Add(Enemy);
        Mode->ActiveIndex=0; Mode->bStarted=true; Mode->bFinished=false;
        Hero->Health=0; Hero->bDowned=true; Hero->bStabilized=true; Hero->DeathSuccesses=3;
        Mode->Tick(0.f);
        Test->TestFalse(TEXT("Encounter does not end before death saves"),Mode->bFinished);
        Hero->StartTurn(); Phase=4; Requested=FPlatformTime::Seconds(); return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHNavigationTest,"AshenHollow.Navigation.AllClasses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FAHNavigationTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FAHWalkAllClasses(this)); return true;
}
#endif
