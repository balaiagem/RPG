#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AHPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPathFollowingComponent;

UCLASS()
class ASHENHOLLOW_API AAHPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    FString PerformanceLabel=TEXT("EQUILIBRADO / F6");
    void CyclePerformance();
    void ApplyPerformance();
    int32 PerformanceProfile=1;
    AAHPlayerController();
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    void CombatCommand(FName Command);
    void EndTurn();
    void Dash();
    UPROPERTY() TObjectPtr<class AAHCharacter> HoveredEnemy;
private:
    void AttackNearest();
    void Heal();
    void Check();
    void Restart();
    UPROPERTY() TObjectPtr<class AAHCharacter> AttackTarget;
    void MoveToCursor();
    void Stop();
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> StopAction;
    UPROPERTY() TObjectPtr<UPathFollowingComponent> PathFollowing;
};
