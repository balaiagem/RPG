#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AHDiceRules.generated.h"

/** Immutable outcome for gameplay and UMG. UI must display this roll, not roll again. */
USTRUCT(BlueprintType)
struct ASHENHOLLOW_API FAHDiceOutcome
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 NaturalRoll = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 Modifier = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 Total = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 Target = 0;
    UPROPERTY(BlueprintReadOnly, Category="Dice") bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly, Category="Dice") bool bCritical = false;
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 Damage = 0;

    // ── Evidence for the interface ────────────────────────────────────────
    // Advantage and the halfling's luck both resolve inside the d20 roll. If the
    // outcome does not carry what was discarded, the player sees a single number
    // and has no way to tell either feature ever fired.
    /** The die advantage or disadvantage threw away. 0 when a single die was rolled. */
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 DiscardedRoll = 0;
    /** -1 disadvantage, 0 straight roll, +1 advantage. */
    UPROPERTY(BlueprintReadOnly, Category="Dice") int32 Advantage = 0;
    /** True when luck rerolled at least one natural 1 out of this roll. */
    UPROPERTY(BlueprintReadOnly, Category="Dice") bool bLuckyReroll = false;
};

/** Pure SRD-derived rules; six-second action timing is owned by the ability system. */
UCLASS()
class ASHENHOLLOW_API UAHDiceRules : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Ashen Hollow|Rules")
    static int32 AttributeModifier(int32 Score);

    UFUNCTION(BlueprintPure, Category="Ashen Hollow|Rules")
    static int32 ProficiencyBonus(int32 Level);

    UFUNCTION(BlueprintCallable, Category="Ashen Hollow|Rules")
    static int32 RollD20(UPARAM(ref) FRandomStream& Random, int32 Advantage = 0, bool bLucky = false);

    /**
     * Same roll as RollD20, but also reports the die that was discarded and
     * whether luck rerolled a natural 1, so the interface can show both.
     * Consumes exactly the same values from the stream as RollD20.
     */
    static int32 RollD20Detailed(FRandomStream& Random, int32 Advantage, bool bLucky,
        int32& OutDiscarded, bool& bOutLuckyFired);

    UFUNCTION(BlueprintCallable, Category="Ashen Hollow|Rules")
    static FAHDiceOutcome RollAttack(UPARAM(ref) FRandomStream& Random,
        int32 AttackBonus, int32 ArmorClass, int32 DiceCount, int32 DiceSides,
        int32 DamageBonus, int32 Advantage = 0, bool bLucky = false);

    UFUNCTION(BlueprintCallable, Category="Ashen Hollow|Rules")
    static FAHDiceOutcome RollCheck(UPARAM(ref) FRandomStream& Random,
        int32 Modifier, int32 DifficultyClass, int32 Advantage = 0, bool bLucky = false);
};
