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
    static int32 RollD20(UPARAM(ref) FRandomStream& Random, int32 Advantage = 0);

    UFUNCTION(BlueprintCallable, Category="Ashen Hollow|Rules")
    static FAHDiceOutcome RollAttack(UPARAM(ref) FRandomStream& Random,
        int32 AttackBonus, int32 ArmorClass, int32 DiceCount, int32 DiceSides,
        int32 DamageBonus, int32 Advantage = 0);

    UFUNCTION(BlueprintCallable, Category="Ashen Hollow|Rules")
    static FAHDiceOutcome RollCheck(UPARAM(ref) FRandomStream& Random,
        int32 Modifier, int32 DifficultyClass, int32 Advantage = 0);
};
