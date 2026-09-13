#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AHDiceRules.h"
#include "AHTurnBudget.h"
#include "AHCharacter.generated.h"

class UAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UAnimationAsset;
class UAnimInstance;
enum class EAHHeroClass : uint8 { Fighter, Barbarian, Cleric, Wizard };

/** Shared character for the turn-based SRD combat encounter. */
UCLASS(Blueprintable)
class ASHENHOLLOW_API AAHCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AAHCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;

    // ── Core stats ────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly) bool bEnemy = false;
    UPROPERTY(BlueprintReadOnly) int32 Health = 12;
    UPROPERTY(BlueprintReadOnly) int32 MaxHealth = 12;
    UPROPERTY(BlueprintReadOnly) int32 ArmorClass = 16;
    UPROPERTY(BlueprintReadOnly) FAHDiceOutcome LastRoll;
    UPROPERTY(BlueprintReadOnly) FString LastRollLabel;
    float LastRollTime = -100.f;
    FAHTurnBudget Turn;
    bool bTurnActive = false;
    bool bDodging = false;
    int32 Initiative = 0;
    int32 LastDamage = 0;
    float LastDamageTime = -100.f;
    FString ImpactText;
    float ImpactTextTime = -100.f;
    bool bImpactHealing = false;
    bool bLastImpactCritical = false;
    FString Feedback;
    bool bCharacterReady = false;
    EAHHeroClass HeroClass = EAHHeroClass::Fighter;
    int32 AttackBonus = 5, DamageSides = 8, DamageModifier = 3, InitiativeBonus = 1;
    int32 ClassCharges = 2;
    bool bRaging = false;
    void ChooseClass(EAHHeroClass Choice);
    void UseClassAbility();
    bool CanUseClassAbility() const;
    static FString ClassName(EAHHeroClass Choice);
    FString ClassAbilityName() const;
    FString ClassAbilityDescription() const;
    TArray<FString> CombatLog;

    // ── Animation state (read by UAHAnimInstance) ─────────────────────────────
    /**
     * True during the attack slot animation window.
     * UAHAnimInstance polls this every frame to drive upper-body blend.
     * Set in PlayAttack(), cleared when the slot finishes.
     */
    UPROPERTY(BlueprintReadOnly, Category="AH|Animation")
    bool bIsAttacking = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool IsBusy() const { return AnimationEnds > 0.f; }
    bool CanAct() const { return IsAlive() && bTurnActive && !IsBusy(); }
    bool IsAlive() const { return Health > 0; }

    // ── Combat actions ────────────────────────────────────────────────────────
    void StartTurn();
    void FinishTurn();
    void AddLog(const FString& Message);
    void Dash();
    void Dodge();
    void SecondWind();
    void CheckArcana();
    bool TryAttack(AAHCharacter* Target);
    void ReceiveHit(int32 Damage, bool bPhysical = true);
    void BecomeEnemy();

    /**
     * Called by UAHNotify_MeleeImpact when the attack animation reaches the
     * impact frame.  Resolves the pending d20 roll immediately instead of
     * waiting for the fallback timer.
     *
     * Safe to call from C++ notify code; if the notify has not been added to the
     * animation asset yet, the timer fallback in ResolveImpact() fires instead.
     */
    void OnMeleeImpactNotify();

    /**
     * Resolves the pending d20 roll against the target.
     * Public so automation tests can trigger impact directly.
     * Called internally by OnMeleeImpactNotify() (frame-perfect path)
     * and by the fallback timer in Tick().
     */
    void ResolveImpact();

    bool bSecondWindUsed = false;

private:
    FRandomStream Dice;
    float NextThink = 0.f;

    /** Absolute time when the attack slot finishes.  0 = not animating. */
    float AnimationEnds = 0.f;
    float ReactionEnds = 0.f;
    int32 RageTurns = 0;
    bool bAttackedSinceTurnEnd = false, bDamagedSinceTurnEnd = false;
    int32 PendingSpellDamage = 0;

    /**
     * Absolute time for the impact fallback timer.
     * Computed as animation-start-time + (sequence-length * ImpactFraction).
     * Set to -1 once the notify has already fired to prevent double-resolution.
     */
    float ImpactAt = 0.f;

    /** Fraction of the attack animation length at which the fallback fires. */
    static constexpr float ImpactFraction = 0.35f;

    FVector PreviousLocation;
    bool bImpactResolved = false;     // prevents double-resolution (notify + timer)

    UPROPERTY() TObjectPtr<AAHCharacter> PendingTarget;
    FAHDiceOutcome PendingRoll;
    UPROPERTY() TObjectPtr<UAnimationAsset> AttackAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> AlternateAttackAnimation;
    uint32 AttackAnimationIndex = 0;
    UPROPERTY() TObjectPtr<UAnimationAsset> DeathAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> HitAnimation;
    UPROPERTY() TSubclassOf<UAnimInstance> LocomotionClass;

    void PlayAttack();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;
};
