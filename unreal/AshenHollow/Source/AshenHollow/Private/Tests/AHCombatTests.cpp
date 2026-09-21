#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHNotify_MeleeImpact.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCombatTest,"AshenHollow.Rules.Combat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHCombatTest::RunTest(const FString& Parameters)
{
    for(const TCHAR* Name:{TEXT("AH_SwordSlash"),TEXT("AH_AxeCleave"),TEXT("AH_MaceStrike"),TEXT("AH_StaffStrike"),TEXT("AH_Cast")})
    {
        const FString Path=FString::Printf(TEXT("/Game/AshenHollow/Animation/%s"),Name);
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
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto FinishAnimations=[&]()
    {
        // The engine clamps a single large delta. Advance normal-sized frames.
        for(int32 Frame=0;Frame<90;++Frame)
        {
            World->Tick(LEVELTICK_All,1.f/30.f);
            for(TActorIterator<AAHCharacter> It(World);It;++It) It->Tick(0.f);
        }
    };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Hero=World->SpawnActor<AAHCharacter>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Enemy=World->SpawnActor<AAHCharacter>(FVector(1000,0,100),FRotator::ZeroRotator,Spawn);
    auto* Healer=World->SpawnActor<AAHCharacter>(FVector(3000,0,100),FRotator::ZeroRotator,Spawn);
    if(!Hero || !Enemy || !Healer) { AddError(TEXT("Fixture spawn failed")); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
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
    TestFalse(TEXT("Gesture cannot be interrupted by another action"),Healer->bSecondWindUsed);
    FinishAnimations();
    Healer->SecondWind();
    TestTrue(TEXT("Bonus heal works after spending action"),Healer->bSecondWindUsed);
    TestFalse(TEXT("Heal consumes bonus action"),Healer->Turn.bBonus);
    TestTrue(TEXT("Healing has distinct feedback"),Healer->bImpactHealing && Healer->ImpactText.StartsWith(TEXT("+")));
    TestTrue(TEXT("No overheal"),Healer->Health<=Healer->MaxHealth);
    FinishAnimations();
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
    auto* Downed=World->SpawnActor<AAHCharacter>(FVector(5000,0,100),FRotator::ZeroRotator,Spawn);
    if(TestNotNull(TEXT("Downed fixture"),Downed))
    {
        Downed->ReceiveHit(100);
        TestTrue(TEXT("Zero HP enters downed state"),Downed->bDowned);
        TestFalse(TEXT("Downed character cannot act"),Downed->CanAct());
        Downed->bStabilized=true; Downed->DeathSuccesses=3;
        const float PreviousSaveTime=Downed->DeathSaveRollTime;
        Downed->StartTurn();
        TestEqual(TEXT("Stable character does not roll another death save"),Downed->DeathSaveRollTime,PreviousSaveTime);
        Downed->FinishTurn(); // Real timer/turn handoff is covered in the runtime navigation test.
        Downed->ReceiveHit(1);
        TestFalse(TEXT("Damage removes stabilization"),Downed->bStabilized);
        TestEqual(TEXT("Old successes cannot immediately restabilize after damage"),Downed->DeathSuccesses,0);
        TestEqual(TEXT("Damage at zero HP adds a death failure"),Downed->DeathFailures,1);
    }
    // ── Reactions: opportunity attacks and Disengage ─────────────────────────
    auto* Runner=World->SpawnActor<AAHCharacter>(FVector(8000,0,100),FRotator::ZeroRotator,Spawn);
    auto* Threat=World->SpawnActor<AAHCharacter>(FVector(8100,0,100),FRotator::ZeroRotator,Spawn);
    if(TestNotNull(TEXT("Reaction mover fixture"),Runner) && TestNotNull(TEXT("Reaction threat fixture"),Threat))
    {
        const FVector InReach(8000,0,100), Away(8600,0,100), Edge(8290,0,100);
        Threat->bEnemy=true; Threat->Health=Threat->MaxHealth=60; Threat->AttackBonus=40;
        Runner->Health=Runner->MaxHealth=60;
        Threat->StartTurn(); Threat->FinishTurn();
        Runner->StartTurn();
        TestTrue(TEXT("Reaction is available at the start of a round"),Threat->Turn.bReaction);

        // Standing inside the reach only arms the threat.
        Runner->ImpactText.Reset();
        Runner->SetActorLocation(InReach); Runner->UpdateThreatState();
        TestTrue(TEXT("Standing inside the reach never provokes"),
            Runner->ImpactText.IsEmpty() && Threat->Turn.bReaction);

        // Walking out of it is what provokes. The departure spans several frames
        // in the real game, so this must be a latch and not a one-frame window.
        Runner->SetActorLocation(Away); Runner->UpdateThreatState();
        TestFalse(TEXT("Leaving melee reach spends the hostile reaction"),Threat->Turn.bReaction);
        TestTrue(TEXT("Opportunity attack labels its outcome on the mover"),
            Runner->ImpactText.Contains(TEXT("OPORTUNIDADE")));

        Runner->ImpactText.Reset();
        Runner->SetActorLocation(InReach); Runner->UpdateThreatState();
        Runner->SetActorLocation(Away);    Runner->UpdateThreatState();
        TestTrue(TEXT("Only one reaction per round"),Runner->ImpactText.IsEmpty());

        // Disengage covers every step of the turn that bought it.
        Threat->StartTurn(); Threat->FinishTurn();
        TestTrue(TEXT("Own turn renews the reaction"),Threat->Turn.bReaction);
        Runner->StartTurn();
        Runner->SetActorLocation(InReach); Runner->UpdateThreatState();
        Runner->Disengage();
        TestTrue(TEXT("Disengage marks the turn"),Runner->bDisengaging);
        TestFalse(TEXT("Disengage costs the action"),Runner->Turn.bAction);
        Runner->ImpactText.Reset();
        Runner->SetActorLocation(Away); Runner->UpdateThreatState();
        TestTrue(TEXT("Disengage prevents the opportunity attack"),Runner->ImpactText.IsEmpty());
        TestTrue(TEXT("Disengage leaves the hostile reaction unspent"),Threat->Turn.bReaction);
        Runner->StartTurn();
        TestFalse(TEXT("Disengage expires with the turn that bought it"),Runner->bDisengaging);

        // Clipping the outer edge of the reach never arms anything, so a foe
        // walking past cannot hand out a free swing it did not earn.
        Runner->ImpactText.Reset();
        Runner->SetActorLocation(Edge); Runner->UpdateThreatState();
        Runner->SetActorLocation(Away); Runner->UpdateThreatState();
        TestTrue(TEXT("Grazing the edge of the reach arms nothing"),
            Runner->ImpactText.IsEmpty() && Threat->Turn.bReaction);

        // Allies never provoke each other.
        Threat->bEnemy=false;
        Runner->StartTurn();
        Runner->ImpactText.Reset();
        Runner->SetActorLocation(InReach); Runner->UpdateThreatState();
        Runner->SetActorLocation(Away);    Runner->UpdateThreatState();
        TestTrue(TEXT("Allies do not provoke each other"),
            Runner->ImpactText.IsEmpty() && Threat->Turn.bReaction);
    }
    // ── Ranged attacks ───────────────────────────────────────────────────────
    auto* Archer=World->SpawnActor<AAHCharacter>(FVector(12000,0,100),FRotator::ZeroRotator,Spawn);
    auto* Quarry=World->SpawnActor<AAHCharacter>(FVector(12900,0,100),FRotator::ZeroRotator,Spawn);
    if(TestNotNull(TEXT("Archer fixture"),Archer) && TestNotNull(TEXT("Quarry fixture"),Quarry))
    {
        Quarry->bEnemy=true; Quarry->Health=Quarry->MaxHealth=400; Quarry->ArmorClass=2;
        Archer->ChooseAncestry(EAHAncestry::Human);
        Archer->ChooseClass(EAHHeroClass::Wizard);
        TestTrue(TEXT("The wizard carries a ranged attack"),Archer->HasRangedAttack());

        Archer->StartTurn();
        Quarry->SetActorLocation(FVector(12000.f+Archer->RangedReach()+500.f,0,100));
        TestFalse(TEXT("A target beyond range is refused"),Archer->TryRangedAttack(Quarry));
        TestTrue(TEXT("A refused shot keeps the action"),Archer->Turn.bAction);

        // 900 cm is far outside melee reach. If the shot were still checked
        // against the 210 cm melee limit it would resolve as a miss every time.
        int32 Landed=0;
        for(int32 Attempt=0;Attempt<6 && Landed==0;++Attempt)
        {
            Archer->StartTurn();
            Quarry->SetActorLocation(FVector(12900,0,100));
            Quarry->ImpactText.Reset();
            const int32 QuarryBefore=Quarry->Health;
            if(!TestTrue(TEXT("A target inside range is shot"),Archer->TryRangedAttack(Quarry))) break;
            TestFalse(TEXT("The shot spends the action"),Archer->Turn.bAction);
            TestEqual(TEXT("No damage before the shot lands"),Quarry->Health,QuarryBefore);
            Archer->OnMeleeImpactNotify();
            TestFalse(TEXT("A shot always reports an outcome"),Quarry->ImpactText.IsEmpty());
            if(Quarry->Health<QuarryBefore) ++Landed;
        }
        TestTrue(TEXT("Shots land at range rather than being voided by melee reach"),Landed>0);

        // Firing with someone in your face is disadvantaged.
        Quarry->SetActorLocation(FVector(12900,0,100));
        TestFalse(TEXT("Nothing threatens an archer at range"),Archer->IsThreatenedInMelee());
        Quarry->SetActorLocation(FVector(12100,0,100));
        TestTrue(TEXT("A foe inside reach threatens the archer"),Archer->IsThreatenedInMelee());

        // Melee-only archetypes are unaffected.
        auto* Brawler=World->SpawnActor<AAHCharacter>(FVector(15000,0,100),FRotator::ZeroRotator,Spawn);
        if(TestNotNull(TEXT("Melee fixture"),Brawler))
        {
            Brawler->ChooseAncestry(EAHAncestry::Human);
            Brawler->ChooseClass(EAHHeroClass::Fighter);
            TestFalse(TEXT("A melee archetype has no ranged attack"),Brawler->HasRangedAttack());
            Brawler->StartTurn();
            TestFalse(TEXT("A melee archetype cannot shoot"),Brawler->TryRangedAttack(Quarry));
            TestTrue(TEXT("A refused shot costs a melee archetype nothing"),Brawler->Turn.bAction);
        }
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
