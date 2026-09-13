#include "AHDiceRules.h"

int32 UAHDiceRules::AttributeModifier(const int32 Score)
{
    return FMath::FloorToInt((static_cast<double>(Score) - 10.0) / 2.0);
}

int32 UAHDiceRules::ProficiencyBonus(const int32 Level)
{
    return 2 + (FMath::Clamp(Level, 1, 20) - 1) / 4;
}

int32 UAHDiceRules::RollD20(FRandomStream& Random, const int32 Advantage)
{
    const int32 First = Random.RandRange(1, 20);
    if (Advantage == 0) return First;
    const int32 Second = Random.RandRange(1, 20);
    return Advantage > 0 ? FMath::Max(First, Second) : FMath::Min(First, Second);
}

FAHDiceOutcome UAHDiceRules::RollCheck(FRandomStream& Random, const int32 Modifier,
    const int32 DifficultyClass, const int32 Advantage)
{
    FAHDiceOutcome Result;
    Result.NaturalRoll = RollD20(Random, Advantage);
    Result.Modifier = Modifier;
    Result.Total = Result.NaturalRoll + Modifier;
    Result.Target = DifficultyClass;
    // A natural 1/20 is not an automatic failure/success on ordinary checks/saves.
    Result.bSuccess = Result.Total >= DifficultyClass;
    return Result;
}

FAHDiceOutcome UAHDiceRules::RollAttack(FRandomStream& Random, const int32 AttackBonus,
    const int32 ArmorClass, const int32 DiceCount, const int32 DiceSides,
    const int32 DamageBonus, const int32 Advantage)
{
    FAHDiceOutcome Result = RollCheck(Random, AttackBonus, ArmorClass, Advantage);
    Result.bCritical = Result.NaturalRoll == 20;
    Result.bSuccess = Result.bCritical || (Result.NaturalRoll != 1 && Result.Total >= ArmorClass);
    if (!Result.bSuccess) return Result;

    // Defensive bounds protect editor-authored data; validation should reject bad definitions.
    const int32 Count = FMath::Clamp(DiceCount, 0, 100) * (Result.bCritical ? 2 : 1);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result.Damage += Random.RandRange(1, FMath::Clamp(DiceSides, 1, 1000));
    }
    Result.Damage = FMath::Max(0, Result.Damage + DamageBonus);
    return Result;
}
