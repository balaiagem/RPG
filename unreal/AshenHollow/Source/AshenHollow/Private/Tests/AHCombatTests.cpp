#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHNotify_MeleeImpact.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCombatTest,"AshenHollow.Rules.Combat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHCombatTest::RunTest(const FString& Parameters)
{
    for(int32 Index=1; Index<=2; ++Index)
    {
        const FString Path=FString::Printf(TEXT("/Game/AshenHollow/Animation/MM_Attack_0%d"),Index);
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*Path);
        if(!TestNotNull(TEXT("Authored attack exists"),Clip)) continue;
        int32 Contacts=0;
        for(const auto& Event:Clip->Notifies)
            if(Event.Notify && Event.Notify->IsA<UAHNotify_MeleeImpact>())
            {
                ++Contacts;
                TestTrue(TEXT("Contact lies inside attack"),Event.GetTime()>0.f && Event.GetTime()<Clip->GetPlayLength());
            }
        TestEqual(TEXT("Exactly one contact per attack"),Contacts,1);
    }
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Hero=World->SpawnActor<AAHCharacter>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Enemy=World->SpawnActor<AAHCharacter>(FVector(1000,0,100),FRotator::ZeroRotator,Spawn);
    auto* Healer=World->SpawnActor<AAHCharacter>(FVector(3000,0,100),FRotator::ZeroRotator,Spawn);
    if(!Hero || !Enemy || !Healer) { AddError(TEXT("Fixture spawn failed")); World->DestroyWorld(false); return false; }
    Enemy->bEnemy=true; Enemy->Health=Enemy->MaxHealth=40;
    TestFalse(TEXT("Cannot act before own turn"),Hero->TryAttack(Enemy));
    Hero->StartTurn();
    TestFalse(TEXT("Out of range rejected"),Hero->TryAttack(Enemy));
    TestTrue(TEXT("Invalid target preserves action"),Hero->Turn.bAction);
    Enemy->SetActorLocation(FVector(150,0,100));
    TestTrue(TEXT("Melee starts"),Hero->TryAttack(Enemy));
    TestFalse(TEXT("Action consumed"),Hero->Turn.bAction);
    TestEqual(TEXT("No damage before contact"),Enemy->Health,40);
    TestFalse(TEXT("Cannot queue duplicate attack"),Hero->TryAttack(Enemy));
    Hero->OnMeleeImpactNotify();
    TestEqual(TEXT("HUD roll is exactly damage applied"),Enemy->Health,40-Hero->LastRoll.Damage);
    TestEqual(TEXT("Target feedback agrees with hit outcome"),Enemy->ImpactText,
        Hero->LastRoll.bSuccess ? FString::Printf(TEXT("%s-%d"),Hero->LastRoll.bCritical?TEXT("CRITICO  "):TEXT(""),Hero->LastRoll.Damage) : FString(TEXT("ERROU")));
    const int32 AfterImpact=Enemy->Health;
    Hero->OnMeleeImpactNotify();
    Hero->ResolveImpact();
    TestEqual(TEXT("Impact cannot apply twice"),Enemy->Health,AfterImpact);
    Healer->StartTurn(); Healer->ReceiveHit(5); Healer->Dodge();
    TestTrue(TEXT("Dodge active"),Healer->bDodging);
    Healer->SecondWind();
    TestTrue(TEXT("Bonus heal works after spending action"),Healer->bSecondWindUsed);
    TestFalse(TEXT("Heal consumes bonus action"),Healer->Turn.bBonus);
    TestTrue(TEXT("Healing has distinct feedback"),Healer->bImpactHealing && Healer->ImpactText.StartsWith(TEXT("+")));
    TestTrue(TEXT("No overheal"),Healer->Health<=Healer->MaxHealth);
    Healer->FinishTurn();
    TestTrue(TEXT("Dodge survives enemy turn"),Healer->bDodging);
    Healer->StartTurn();
    TestFalse(TEXT("Dodge expires on next own turn"),Healer->bDodging);
    TestTrue(TEXT("Next turn renews bonus"),Healer->Turn.bBonus);
    Healer->ReceiveHit(2); const int32 Before=Healer->Health; Healer->SecondWind();
    TestEqual(TEXT("Class resource does not reset each turn"),Healer->Health,Before);
    Healer->Dash(); TestEqual(TEXT("Dash doubles movement"),Healer->Turn.Movement,1800.f);
    Healer->Turn.Travel(1900); TestEqual(TEXT("Movement never becomes negative"),Healer->Turn.Movement,0.f);
    Healer->StartTurn(); TestEqual(TEXT("Turn restores normal movement"),Healer->Turn.Movement,900.f);
    Enemy->StartTurn(); TestTrue(TEXT("Enemy starts attack"),Enemy->TryAttack(Hero));
    Enemy->ReceiveHit(1000); const int32 HeroBefore=Hero->Health; Enemy->ResolveImpact();
    TestEqual(TEXT("Dead attacker cancels pending damage"),Hero->Health,HeroBefore);
    TestEqual(TEXT("Overkill clamps HP"),Enemy->Health,0);
    Enemy->ReceiveHit(-10); TestEqual(TEXT("Negative damage cannot resurrect"),Enemy->Health,0);
    auto* Mode=World->SpawnActor<AAHGameMode>();
    auto* Next=World->SpawnActor<AAHCharacter>(FVector(3500,0,100),FRotator::ZeroRotator,Spawn);
    if(Mode && Next)
    {
        Mode->Order.Add(Healer); Mode->Order.Add(Next); Mode->bStarted=true;
        Healer->StartTurn(); Healer->Turn.SpendAction(); Healer->Turn.Travel(250);
        TestFalse(TEXT("Other participant cannot end active turn"),Mode->EndTurn(Next));
        TestTrue(TEXT("Active participant ends turn"),Mode->EndTurn(Healer));
        TestFalse(TEXT("Previous actor loses control"),Healer->bTurnActive);
        TestTrue(TEXT("Next actor gains control"),Next->bTurnActive);
        TestEqual(TEXT("Round only advances after full cycle"),Mode->Round,1);
        TestTrue(TEXT("Complete the turn cycle"),Mode->EndTurn(Next));
        TestEqual(TEXT("New round begins"),Mode->Round,2);
        TestTrue(TEXT("Action renews on next own turn"),Healer->Turn.bAction);
        TestEqual(TEXT("Movement renews on next own turn"),Healer->Turn.Movement,900.f);
        Mode->bFinished=true;
        TestFalse(TEXT("Finished encounter cannot advance"),Mode->EndTurn(Healer));
    }
    else AddError(TEXT("Turn fixture spawn failed"));
    World->DestroyWorld(false); return true;
}
#endif
