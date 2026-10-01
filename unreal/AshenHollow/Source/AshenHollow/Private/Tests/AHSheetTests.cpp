#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHSheetMathTest,"AshenHollow.Sheet.PointBuyAndDerivedStats",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHSheetMathTest::RunTest(const FString&)
{
    FAHAbilities Scores;
    Scores.Set(EAHAbility::Forca,9);
    TestEqual(TEXT("Odd negative modifier rounds down"),Scores.Mod(EAHAbility::Forca),-1);
    TestEqual(TEXT("Buying 14 costs seven"),AHSheet::BuyCost(14),7);
    TestEqual(TEXT("Buying 15 costs nine"),AHSheet::BuyCost(15),9);
    for(int32 C=0;C<AHRules::ClassCount();++C)
    {
        const auto Class=static_cast<EAHHeroClass>(C);
        for(int32 I=0;I<6;++I) Scores.Score[I]=AHSheet::Recommended(Class)[I];
        TestTrue(TEXT("Every class starts with a legal spread"),AHSheet::Legal(Scores));
        TestEqual(TEXT("Every suggested spread spends 27"),AHSheet::Spent(Scores),27);
    }
    for(int32& Score:Scores.Score) Score=8;
    FString Worn[static_cast<int32>(EAHSlot::Count)];
    Worn[static_cast<int32>(EAHSlot::Armadura)]=TEXT("cota_malha");
    auto Made=AHSheet::Derive(EAHHeroClass::Fighter,EAHAncestry::Human,1,Scores,Worn);
    TestEqual(TEXT("Heavy armour ignores negative Dexterity; Defense adds one"),Made.ArmorClass,17);
    TestTrue(TEXT("Insufficient Strength slows heavy armour"),Made.bOverloaded);
    Worn[static_cast<int32>(EAHSlot::Armadura)].Reset();
    Made=AHSheet::Derive(EAHHeroClass::Wizard,EAHAncestry::Human,1,Scores,Worn);
    TestEqual(TEXT("Empty hands do not restore a class weapon"),Made.DamageSides,1);
    TestEqual(TEXT("Empty hands cannot shoot"),Made.RangedRange,0);
    TestEqual(TEXT("Montante rolls two dice"),AHItems::Find(TEXT("montante"))->DiceCount,2);
    TestEqual(TEXT("Montante uses d6"),AHItems::Find(TEXT("montante"))->Dano,6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHSheetActionsTest,"AshenHollow.Sheet.CreationInventoryAndRest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHSheetActionsTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Hero=World->SpawnActor<AAHCharacter>(FVector(0,0,100),FRotator::ZeroRotator,Params);
    auto* PC=World->SpawnActor<AAHPlayerController>();
    PC->Possess(Hero);
    Hero->ChooseAncestry(EAHAncestry::Human); Hero->PickClass(EAHHeroClass::Fighter);
    Hero->Abilities.Score[0]=8;
    PC->CombatCommand(TEXT("AbilAuto"));
    TestEqual(TEXT("Suggestion command reaches its own handler"),Hero->Abilities.Score[0],15);
    TestFalse(TEXT("Cannot buy above 15"),Hero->BuyAbility(EAHAbility::Forca,1));
    TestTrue(TEXT("May refund a point"),Hero->BuyAbility(EAHAbility::Forca,-1));
    PC->CombatCommand(TEXT("AbilDone"));
    TestTrue(TEXT("Continue actually advances point buy"),Hero->bPointsDone);
    PC->CombatCommand(TEXT("Kit0"));
    TestTrue(TEXT("Kit completes character creation"),Hero->bCharacterReady);
    const int32 Wounded=Hero->Health-2; Hero->Health=Wounded;
    Hero->Carry(TEXT("montante"));
    auto IndexOf=[Hero](const TCHAR* Id){return Hero->Backpack.IndexOfByPredicate([Id](const auto& Entry){return Entry.Id==Id;});};
    auto Count=[Hero](const TCHAR* Id)
    {
        int32 N=0;
        for(const auto& Entry:Hero->Backpack) if(Entry.Id==Id) N+=Entry.Many;
        for(const auto& Entry:Hero->Equipped) if(Entry==Id) ++N;
        return N;
    };
    TestTrue(TEXT("Equip owned two-handed weapon"),Hero->UseBackpackItem(IndexOf(TEXT("montante"))));
    TestTrue(TEXT("Two-handed weapon puts shield away"),Hero->Equipped[static_cast<int32>(EAHSlot::MaoSecundaria)].IsEmpty());
    TestEqual(TEXT("Equipment does not heal"),Hero->Health,Wounded);
    TestEqual(TEXT("Shield is conserved"),Count(TEXT("escudo")),1);
    TestFalse(TEXT("Cannot equip shield with two-handed weapon"),Hero->UseBackpackItem(IndexOf(TEXT("escudo"))));
    TestEqual(TEXT("Rejected equipment does not consume shield"),Count(TEXT("escudo")),1);
    Hero->Unequip(EAHSlot::MaoPrincipal); Hero->Unequip(EAHSlot::MaoPrincipal);
    TestEqual(TEXT("Repeated unequip does not duplicate item"),Count(TEXT("montante")),1);
    TestFalse(TEXT("Invalid pack index rejected"),Hero->UseBackpackItem(999));
    Hero->Health=0; Hero->RecomputeSheet(); TestEqual(TEXT("Recompute cannot resurrect"),Hero->Health,0);
    Hero->Health=Hero->MaxHealth;
    const int32 Potions=Count(TEXT("pocao_cura"));
    TestFalse(TEXT("Full health does not consume potion"),Hero->UseBackpackItem(IndexOf(TEXT("pocao_cura"))));
    TestEqual(TEXT("Potion retained"),Count(TEXT("pocao_cura")),Potions);
    Hero->Health=1;
    TestTrue(TEXT("Potion heals and is consumed"),Hero->UseBackpackItem(IndexOf(TEXT("pocao_cura"))));
    TestEqual(TEXT("Potion count decreases once"),Count(TEXT("pocao_cura")),Potions-1);
    Hero->Health=1; Hero->HitDice=1; Hero->ShortRest();
    TestEqual(TEXT("Short rest spends one hit die"),Hero->HitDice,0);
    const int32 AfterRest=Hero->Health; Hero->ShortRest();
    TestEqual(TEXT("No free healing without hit dice"),Hero->Health,AfterRest);
    auto* Wizard=World->SpawnActor<AAHCharacter>(FVector(4000,0,100),FRotator::ZeroRotator,Params);
    Wizard->ChooseClass(EAHHeroClass::Wizard);
    Wizard->Abilities.Set(EAHAbility::Inteligencia,8); Wizard->RecomputeSheet();
    const int32 LowDC=Wizard->SpellSaveDC();
    Wizard->Abilities.Set(EAHAbility::Inteligencia,15); Wizard->RecomputeSheet();
    TestTrue(TEXT("Intelligence changes actual spell DC"),Wizard->SpellSaveDC()>LowDC);
    TestEqual(TEXT("Displayed spell attack is used in combat"),Wizard->SpellAttackBonus(),Wizard->SpellAttack);
    Wizard->ClassCharges=0; Wizard->ShortRest();
    TestEqual(TEXT("Wizard recovers one first-level slot"),Wizard->ClassCharges,1);
    Wizard->ClassCharges=0; Wizard->ShortRest();
    TestEqual(TEXT("Arcane recovery cannot be repeated before long rest"),Wizard->ClassCharges,0);
    Wizard->Rest(); TestFalse(TEXT("Long rest resets recovery"),Wizard->bArcaneRecoveryUsed);
    auto* Mode=World->SpawnActor<AAHGameMode>();
    Mode->Plan.Cells.Init(EAHCell::Meadow,AHArena::GridSide*AHArena::GridSide);
    TestFalse(TEXT("Meadow is outside town"),Mode->IsInTown(Hero->GetActorLocation()));
    int32 X=0,Y=0; AHArena::CellOf(Hero->GetActorLocation(),X,Y);
    Mode->Plan.Cells[X*AHArena::GridSide+Y]=EAHCell::Village;
    TestTrue(TEXT("Village permits town rest location"),Mode->IsInTown(Hero->GetActorLocation()));
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
#endif
