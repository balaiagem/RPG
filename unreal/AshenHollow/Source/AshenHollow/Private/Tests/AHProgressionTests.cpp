#include "AHCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHProgressionTest,"AshenHollow.Rules.Progression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAHProgressionTest::RunTest(const FString& Parameters)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for(int32 C=0;C<4;++C)
    {
        auto* Hero=World->SpawnActor<AAHCharacter>(FVector(C*3000,0,100),FRotator::ZeroRotator,P);
        if(!TestNotNull(TEXT("Progression character"),Hero)) continue;
        Hero->ChooseClass(static_cast<EAHHeroClass>(C)); const int32 Initial=Hero->MaxHealth;
        TestFalse(TEXT("No early feat"),Hero->ChooseFeat(1));
        Hero->GainExperience(299); TestEqual(TEXT("Below threshold"),Hero->Level,1);
        Hero->GainExperience(1); TestEqual(TEXT("Level two threshold"),Hero->Level,2);
        Hero->StartTurn();
        if(C==0) { Hero->Turn.SpendAction(); Hero->UseProgressionAbility(); TestTrue(TEXT("Surge restores action"),Hero->Turn.bAction); Hero->Turn.SpendAction(); Hero->UseProgressionAbility(); TestFalse(TEXT("Surge only once"),Hero->Turn.bAction); }
        if(C==1) { Hero->UseProgressionAbility(); TestTrue(TEXT("Reckless active"),Hero->bReckless); TestTrue(TEXT("Reckless preserves attack"),Hero->Turn.bAction); Hero->StartTurn(); TestFalse(TEXT("Reckless expires next turn"),Hero->bReckless); }
        if(C==2) { const int32 AC=Hero->ArmorClass; Hero->UseProgressionAbility(); TestEqual(TEXT("Shield armor"),Hero->ArmorClass,AC+2); TestEqual(TEXT("Shield consumes one slot"),Hero->ClassCharges,2); Hero->Rest(); TestEqual(TEXT("Rest removes shield once"),Hero->ArmorClass,AC); }
        if(C==3) { Hero->UseProgressionAbility(); TestTrue(TEXT("False life HP range"),Hero->TempHP>=5 && Hero->TempHP<=8); TestEqual(TEXT("False life consumes slot"),Hero->ClassCharges,2); }
        Hero->GainExperience(600); TestEqual(TEXT("Level three"),Hero->Level,3);
        if(C>=2) { TestEqual(TEXT("Level three first slots"),Hero->MaxSpellSlots(1),4); TestEqual(TEXT("Level three second slots"),Hero->SpellSlots2,2); Hero->CycleSpellLevel(); Hero->SpendSpellSlot(); TestEqual(TEXT("Upcast consumes second slot"),Hero->SpellSlots2,1); }
        Hero->GainExperience(1800); TestEqual(TEXT("Level four"),Hero->Level,4);
        const int32 Growth[]={8,9,7,6}; TestEqual(TEXT("Class HP progression"),Hero->MaxHealth,Initial+3*Growth[C]);
        TestTrue(TEXT("Feat selectable at four"),Hero->ChooseFeat(1)); TestEqual(TEXT("Tough HP"),Hero->MaxHealth,Initial+3*Growth[C]+8);
        TestFalse(TEXT("Feat cannot stack"),Hero->ChooseFeat(2));
        Hero->GainExperience(9999); TestEqual(TEXT("Milestone level cap"),Hero->Level,4);
        Hero->Rest(); TestEqual(TEXT("Rest heals"),Hero->Health,Hero->MaxHealth);
        if(C>=2) { TestEqual(TEXT("Rest first slots"),Hero->ClassCharges,4); TestEqual(TEXT("Rest second slots"),Hero->SpellSlots2,3); }
        TestEqual(TEXT("Rest preserves XP"),Hero->Experience,2700); TestEqual(TEXT("Rest preserves feat"),Hero->Feat,1);
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
}
#endif
