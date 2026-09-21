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
    int32 LuckyCases=0;
    for(int32 Seed=0;Seed<128;++Seed) for(int32 Advantage=-1;Advantage<=1;++Advantage)
    {
        FRandomStream Expected(Seed),Actual(Seed);
        auto Die=[&]() { int32 Value=Expected.RandRange(1,20); if(Value==1) { ++LuckyCases; Value=Expected.RandRange(1,20); } return Value; };
        const int32 First=Die(); const int32 Second=Advantage==0?First:Die();
        const int32 Wanted=Advantage==0?First:Advantage>0?FMath::Max(First,Second):FMath::Min(First,Second);
        TestEqual(TEXT("Lucky rerolls each natural one once before advantage selection"),UAHDiceRules::RollD20(Actual,Advantage,true),Wanted);
        TestEqual(TEXT("Lucky consumes no hidden extra rolls"),Actual.RandRange(1,20),Expected.RandRange(1,20));
    }
    TestTrue(TEXT("Lucky test exercised natural ones"),LuckyCases>0);
    TestTrue(TEXT("Advantage chooses greater roll from the same pair"),
        UAHDiceRules::RollD20(Adv, 1) >= UAHDiceRules::RollD20(Disadv, -1));

    // The interface can only show advantage or luck if the outcome carries the
    // evidence. A roll that reports nothing is indistinguishable from a plain one.
    {
        FRandomStream Plain(21);
        const FAHDiceOutcome Straight = UAHDiceRules::RollCheck(Plain, 0, 10);
        TestEqual(TEXT("A straight roll discards no die"), Straight.DiscardedRoll, 0);
        TestEqual(TEXT("A straight roll reports no advantage"), Straight.Advantage, 0);
        TestFalse(TEXT("A straight roll reports no luck"), Straight.bLuckyReroll);
    }
    for (int32 Seed = 0; Seed < 64; ++Seed)
    {
        FRandomStream Up(Seed), Down(Seed);
        const FAHDiceOutcome High = UAHDiceRules::RollCheck(Up, 0, 10, 1);
        const FAHDiceOutcome Low  = UAHDiceRules::RollCheck(Down, 0, 10, -1);
        TestEqual(TEXT("Advantage is reported to the interface"), High.Advantage, 1);
        TestEqual(TEXT("Disadvantage is reported to the interface"), Low.Advantage, -1);
        TestTrue(TEXT("Advantage keeps the higher of the two dice it shows"),
            High.DiscardedRoll > 0 && High.NaturalRoll >= High.DiscardedRoll);
        TestTrue(TEXT("Disadvantage keeps the lower of the two dice it shows"),
            Low.DiscardedRoll > 0 && Low.NaturalRoll <= Low.DiscardedRoll);
    }
    bool bReportedLuck = false;
    for (int32 Seed = 0; Seed < 400 && !bReportedLuck; ++Seed)
    {
        FRandomStream Lucky(Seed);
        const FAHDiceOutcome Result = UAHDiceRules::RollCheck(Lucky, 0, 10, 0, true);
        if (Result.bLuckyReroll)
        {
            bReportedLuck = true;
            TestNotEqual(TEXT("Luck never leaves a natural 1 standing"), Result.NaturalRoll, 1);
        }
    }
    TestTrue(TEXT("Luck reports itself to the interface"), bReportedLuck);
    return true;
}
#endif
