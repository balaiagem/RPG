#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AHAnimInstance.generated.h"

class AAHCharacter;

/** Native directional locomotion graph with DefaultSlot combat overlays. */
UCLASS()
class ASHENHOLLOW_API UAHAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UAHAnimInstance();
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
    UPROPERTY() TObjectPtr<class UBlendSpace> LocomotionBlendSpace;
    // ── Locomotion ────────────────────────────────────────────────────────────

    /** Horizontal ground speed in cm/s; the blend space uses it on its Y axis. */
    UPROPERTY(BlueprintReadOnly, Category="AH|Locomotion")
    float Speed = 0.f;

    /** True while the character has meaningful horizontal velocity (Speed > 10). */
    UPROPERTY(BlueprintReadOnly, Category="AH|Locomotion")
    bool bIsMoving = false;

    // ── State ─────────────────────────────────────────────────────────────────

    /** True once health reaches zero.  Drives the transition into the death pose. */
    UPROPERTY(BlueprintReadOnly, Category="AH|State")
    bool bIsDead = false;

    /**
     * True during the attack window — from PlayAttack() until the animation slot
     * finishes.  Use this in the ABP to blend the upper-body attack layer over
     * the locomotion base pose.
     */
    UPROPERTY(BlueprintReadOnly, Category="AH|State")
    bool bIsAttacking = false;

protected:
    /** Polls owner state each frame; safe to call even before BeginPlay. */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    TWeakObjectPtr<AAHCharacter> OwnerCharacter;
};
