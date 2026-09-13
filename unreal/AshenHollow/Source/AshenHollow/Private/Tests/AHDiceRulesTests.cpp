#include "AHDiceRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHDiceTest, "AshenHollow.Rules.Dice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAHDiceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Negative odd modifiers round down"), UAHDiceRules::AttributeModifier(9), -1);
    TestEqual(TEXT("Strength 16"), UAHDiceRules::AttributeModifier(16), 3);
    TestEqual(TEXT("Level 17 proficiency"), UAHDiceRules::ProficiencyBonus(17), 6);
    FRandomStream Random(912);
    bool bSawCritical = false;
    bool bSawOne = false;
    for (int32 Index = 0; Index < 1000; ++Index)
    {
        FRandomStream EasyAttackRandom = Random;
        FRandomStream HardCheckRandom = Random;
        FRandomStream EasyCheckRandom = Random;
        const FAHDiceOutcome Result = UAHDiceRules::RollAttack(Random, 100, 999, 1, 1, 3);
        const FAHDiceOutcome EasyAttack = UAHDiceRules::RollAttack(EasyAttackRandom, 100, 1, 1, 1, 3);
        const FAHDiceOutcome HardCheck = UAHDiceRules::RollCheck(HardCheckRandom, 0, 30);
        const FAHDiceOutcome EasyCheck = UAHDiceRules::RollCheck(EasyCheckRandom, 100, 1);
        if (Result.NaturalRoll == 20)
        {
            bSawCritical = true;
            TestTrue(TEXT("Natural 20 always hits"), Result.bSuccess);
            TestEqual(TEXT("Critical doubles dice, not modifier"), Result.Damage, 5);
            TestFalse(TEXT("Natural 20 does not automatically pass a check"), HardCheck.bSuccess);
        }
        if (Result.NaturalRoll == 1)
        {
            bSawOne = true;
            TestFalse(TEXT("Natural 1 misses even with sufficient attack bonus"), EasyAttack.bSuccess);
            TestEqual(TEXT("A missed attack deals no damage"), EasyAttack.Damage, 0);
            TestTrue(TEXT("Natural 1 can pass a check with sufficient bonus"), EasyCheck.bSuccess);
        }
    }
    TestTrue(TEXT("Sample exercised natural 20"), bSawCritical);
    TestTrue(TEXT("Sample exercised natural 1"), bSawOne);
    FRandomStream Adv(55), Disadv(55);
    TestTrue(TEXT("Advantage chooses greater roll from the same pair"),
        UAHDiceRules::RollD20(Adv, 1) >= UAHDiceRules::RollD20(Disadv, -1));
    return true;
}
#endif
