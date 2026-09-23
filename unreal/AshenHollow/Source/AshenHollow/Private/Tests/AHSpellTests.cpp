#include "AHCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHSpellTest,"AshenHollow.Rules.SpellbookAndCasting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHSpellTest::RunTest(const FString& Parameters)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* C=World->SpawnActor<AAHCharacter>(FVector(0,0,100),FRotator::ZeroRotator,P);
    auto* W=World->SpawnActor<AAHCharacter>(FVector(10000,0,100),FRotator::ZeroRotator,P);
    auto* Foe=World->SpawnActor<AAHCharacter>(FVector(600,0,100),FRotator::ZeroRotator,P);
    auto* R=World->SpawnActor<AAHCharacter>(FVector(20000,0,100),FRotator::ZeroRotator,P);
    if(!C || !W || !Foe || !R) { AddError(TEXT("Spell fixture unavailable")); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
    C->ChooseClass(EAHHeroClass::Cleric); W->ChooseClass(EAHHeroClass::Wizard);
    R->ChooseClass(EAHHeroClass::Ranger);
    Foe->bEnemy=true; Foe->Health=Foe->MaxHealth=10000;
    TestFalse(TEXT("Spell class restrictions"),C->SelectSpell(EAHSpell::MagicMissile));
    TestFalse(TEXT("Rank two locked at level one"),W->SelectSpell(EAHSpell::ScorchingRay));
    C->bPreparingSpells=true;
    TestEqual(TEXT("Four default prepared prayers"),C->PreparedSpells.Num(),4);
    TestFalse(TEXT("Preparation limit enforced"),C->TogglePreparedSpell(EAHSpell::Bless));
    TestTrue(TEXT("Can remove prepared prayer"),C->TogglePreparedSpell(EAHSpell::CureWounds));
    TestTrue(TEXT("Can replace prepared prayer"),C->TogglePreparedSpell(EAHSpell::Bless));
    C->bPreparingSpells=false;
    TestFalse(TEXT("No preparation during combat"),C->TogglePreparedSpell(EAHSpell::Bless));
    C->StartTurn(); TestFalse(TEXT("Unprepared spell cannot cast"),C->CastSpell(EAHSpell::CureWounds));
    TestEqual(TEXT("Rejected cast preserves slots"),C->ClassCharges,2);
    for(int32 I=0;I<AHSpells::Count();++I)
    {
        const auto Id=static_cast<EAHSpell>(I); const auto& S=AHSpells::Get(Id);
        auto* Hero=S.Class==EAHHeroClass::Cleric?C:S.Class==EAHHeroClass::Ranger?R:W;
        Hero->Rest(); Hero->GainExperience(2700); Hero->Rest();
        Hero->bPreparingSpells=true; Hero->PreparedSpells.Reset();
        if(S.Rank>0) TestTrue(TEXT("Prepare catalog spell"),Hero->TogglePreparedSpell(Id));
        Hero->bPreparingSpells=false; Hero->StartTurn(); Hero->Health=1;
        Hero->SelectSpell(Id);
        const int32 Slots1=Hero->ClassCharges,Slots2=Hero->SpellSlots2;
        Foe->SetActorLocation(Hero->GetActorLocation()+FVector(S.Range>0?FMath::Min(300.f,S.Range-10.f):300.f,0,0));
        Foe->Health=10000; Foe->ArmorClass=10; Foe->FrostTurns=0; Foe->GuidingSource.Reset();
        TestTrue(FString::Printf(TEXT("Catalog cast: %s"),S.Name),Hero->CastSpell(Id,S.bHostile?Foe:nullptr));
        TestEqual(TEXT("Action budget spent once"),S.bBonus?Hero->Turn.bBonus:Hero->Turn.bAction,false);
        TestEqual(TEXT("First slot pool"),Hero->ClassCharges,Slots1-(S.Rank==1?1:0));
        TestEqual(TEXT("Second slot pool"),Hero->SpellSlots2,Slots2-(S.Rank==2?1:0));
        if(S.bHostile)
        {
            TestEqual(TEXT("Damage waits for contact"),Foe->Health,10000);
            Hero->ResolveImpact(); const int32 After=Foe->Health;
            Hero->ResolveImpact(); TestEqual(TEXT("Impact resolves once"),Foe->Health,After);
            if(Id==EAHSpell::MagicMissile) TestTrue(TEXT("Three darts damage"),After>=9985 && After<=9994);
        }
        if(Id==EAHSpell::Aid) TestEqual(TEXT("Aid max HP buff"),Hero->AidBonus,5);
        if(Id==EAHSpell::Bless) TestEqual(TEXT("Bless concentration"),Hero->BlessTurns,10);
        if(Id==EAHSpell::MageArmor) TestEqual(TEXT("Mage armor bonus"),Hero->MageArmorBonus,3);
        if(Id==EAHSpell::ShieldOfFaith) TestEqual(TEXT("Shield concentration"),Hero->GuardTurns,100);
        if(Id==EAHSpell::FalseLife) TestTrue(TEXT("False life HP range"),Hero->TempHP>=5 && Hero->TempHP<=8);
        AddInfo(FString::Printf(TEXT("Executed %s"),S.Name));
    }
    // Empty pools still permit cantrips; invalid targets never spend actions.
    W->Rest(); W->StartTurn(); W->ClassCharges=W->SpellSlots2=0;
    Foe->SetActorLocation(W->GetActorLocation()+FVector(5000,0,0));
    TestFalse(TEXT("Out-of-range cantrip rejected"),W->CastSpell(EAHSpell::FireBolt,Foe));
    TestTrue(TEXT("Invalid target preserves action"),W->Turn.bAction);
    Foe->SetActorLocation(W->GetActorLocation()+FVector(600,0,0));
    TestTrue(TEXT("Cantrip without slots"),W->CastSpell(EAHSpell::FireBolt,Foe)); W->ResolveImpact();
    C->Rest(); C->StartTurn(); C->PreparedSpells={EAHSpell::HealingWord,EAHSpell::GuidingBolt}; C->Health=1;
    TestTrue(TEXT("Bonus healing casts"),C->CastSpell(EAHSpell::HealingWord));
    // Rest clears animation only; use a fresh turn-budget fixture for the same-turn spell restriction.
    TestTrue(TEXT("Bonus-spell flag is set"),C->bBonusSpellCast);
    TestTrue(TEXT("Healing word preserves action"),C->Turn.bAction);
    C->StartTurn(); TestFalse(TEXT("Bonus spell restriction resets next turn"),C->bBonusSpellCast);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
