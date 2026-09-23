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
    bool bSpellbookOpen=false;
    void ToggleSpellbook();
    void EndTurn();
    void Dash();
    bool RequestMoveToLocation(const FVector& Location);
    bool OfferReaction(class AAHCharacter* Mover);
    void ResolveReaction(bool bAccept);
    bool IsReactionPending() const { return ReactionTarget.IsValid(); }
    UPROPERTY() TObjectPtr<class AAHCharacter> HoveredEnemy;
private:
    TWeakObjectPtr<class AAHCharacter> ReactionTarget;
    void AcceptReaction() { ResolveReaction(true); }
    void DeclineReaction() { ResolveReaction(false); }
    void AttackNearest();
    void Heal();
    void BreathAction();
    void DisengageAction();
    void Check();
    void Restart();
    UPROPERTY() TObjectPtr<class AAHCharacter> AttackTarget;
    void MoveToCursor();
    void Stop();
    bool bQueuedMove=false;
    FVector QueuedMove=FVector::ZeroVector;
    double QueuedMoveExpires=0;
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> StopAction;
    UPROPERTY() TObjectPtr<UPathFollowingComponent> PathFollowing;
};
