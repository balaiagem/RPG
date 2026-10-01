#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AHPlayerController.generated.h"

/**
 * Which question the reaction prompt is asking.
 *
 * There used to be exactly one, so the prompt could assume it. Now that the
 * arcane Shield also interrupts, the pause has to remember what it stopped for.
 */
enum class EAHReaction : uint8 { Opportunity, Shield };

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
    // Camera. A and D because Q and E are already attack and cast, and the wheel
    // because nothing else uses it.
    void RotateCameraLeft();
    void RotateCameraRight();
    void ZoomCameraIn();
    void ZoomCameraOut();
    void ApplyPerformance();
    int32 PerformanceProfile=1;
    AAHPlayerController();
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    void CombatCommand(FName Command);
    bool bSpellbookOpen=false;
    /** The character sheet and the pack, on P. Drawn by the HUD. */
    bool bSheetOpen=false;
    bool bQuestJournal=false;
    bool bProgressionSheet=false;
    void ToggleJournal();
    int32 BackpackPage=0;
    void ToggleSheet();
    void ToggleSpellbook();
    void EndTurn();
    void Dash();
    bool RequestMoveToLocation(const FVector& Location);
    bool OfferReaction(class AAHCharacter* Mover, EAHReaction Kind = EAHReaction::Opportunity);
    void ResolveReaction(bool bAccept);
    bool IsReactionPending() const { return ReactionTarget.IsValid(); }
    /** What the paused prompt is asking about. Read by the HUD. */
    EAHReaction ReactionKind = EAHReaction::Opportunity;
    /** Whoever the paused prompt is about: the mover, or the attacker mid-swing. */
    class AAHCharacter* PendingAttacker() const { return ReactionTarget.Get(); }
    UPROPERTY() TObjectPtr<class AAHCharacter> HoveredEnemy;

    /**
     * The foe the player picked with a left click, and who every action uses.
     *
     * Before this, casting a spell meant holding the mouse exactly over an enemy
     * and pressing E, and attacking always went for the nearest one -- so with
     * two foes in front of you there was no way to say which. Worse, clicking the
     * MAGIA button put the cursor over the button, which meant nothing was
     * hovered, which quietly ran a different ability instead of the spell.
     *
     * A picked target survives moving the mouse, so it is also the only version
     * of this that works from the HUD buttons.
     */
    UPROPERTY() TObjectPtr<class AAHCharacter> ChosenTarget;
    /** The picked foe, or the one under the cursor, or the nearest living one. */
    class AAHCharacter* CurrentTarget() const;
    /**
     * Steps to the next living foe and makes it the target.
     *
     * On a key, deliberately. Picking with the mouse depends on a click reaching
     * this class past Slate, the HUD's hit boxes and the input stack; a key
     * binding travels the same road as Q and E, which demonstrably work. If the
     * mouse ever fails again, aiming still does not.
     */
    void CycleTarget();
    /** Creep, or stop creeping. Exploration only. */
    void ToggleSneak();
private:
    void SelectTarget();
    TWeakObjectPtr<class AAHCharacter> ReactionTarget;
    void AcceptReaction() { ResolveReaction(true); }
    void DeclineReaction() { ResolveReaction(false); }
    void AttackNearest();
    void Heal();
    void BreathAction();
    void DisengageAction();
    void Check();
    /** Talk to whoever is standing near. G, and also a click on a villager. */
    void Falar();
    /** Set when a click sent us walking to somebody: greet him on arrival. */
    bool bTalkOnArrival = false;
    void Restart();
    UPROPERTY() TObjectPtr<class AAHCharacter> AttackTarget;
    void MoveToCursor();
    /**
     * Holding the right button keeps walking.
     *
     * Lucas: "quero poder segurar o botao direito do mouse e continuar
     * andando". The action was bound to Started only, so every step of a
     * four-hundred-metre walk was its own click.
     *
     * It still PATHFINDS -- it is not direct steering -- because pathfinding
     * is what walks you round a house instead of into it. What makes that
     * affordable is that a fresh path is only asked for when the cursor has
     * actually moved somewhere else, or a fifth of a second has passed:
     * re-pathing sixty times a second over a kilometre is not a feature, it is
     * a stutter.
     */
    void HoldMove();
    void ReleaseMove();
    bool    bHoldingMove   = false;
    float   NextHoldPath   = 0.f;
    FVector LastHoldGoal   = FVector::ZeroVector;
    /** Set while re-issuing a held move, so it does not redraw the marker. */
    bool    bQuietMove     = false;
    void Stop();
    bool bQueuedMove=false;
    FVector QueuedMove=FVector::ZeroVector;
    double QueuedMoveExpires=0;
    float NextQueuedMoveAttempt=0.f;
    /**
     * Walking without a navmesh: the last resort, and the reason the game can
     * never again be completely unplayable because of one.
     *
     * Every step the player takes is a navmesh query, so a navmesh that failed
     * to build leaves a character who simply stands there while the engine
     * logs nothing about it. When the destination cannot be projected onto the
     * navmesh at all, the pawn is steered straight at it instead -- the
     * movement component still handles walls and slopes, so the worst case is
     * bumping into a house rather than a game that does not respond.
     */
    bool bWalkingBlind=false;
    FVector BlindTarget=FVector::ZeroVector;
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> StopAction;
    UPROPERTY() TObjectPtr<UPathFollowingComponent> PathFollowing;
};
