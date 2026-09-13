#include "AHAnimInstance.h"
#include "AHCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FAHAnimationProxy final : FAnimInstanceProxy
{
    FAnimNode_BlendSpacePlayer_Standalone Locomotion;
    FAnimNode_Slot CombatSlot;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &CombatSlot; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&Locomotion); Nodes.Add(&CombatSlot); }
    explicit FAHAnimationProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void Initialize(UAnimInstance* Instance) override
    {
        FAnimInstanceProxy::Initialize(Instance);
        Locomotion.SetBlendSpace(CastChecked<UAHAnimInstance>(Instance)->LocomotionBlendSpace);
        Locomotion.SetLoop(true);
        CombatSlot.SlotName=TEXT("DefaultSlot");
        CombatSlot.bAlwaysUpdateSourcePose=true;
        CombatSlot.Source.SetLinkNode(&Locomotion);
        CombatSlot.Initialize_AnyThread(FAnimationInitializeContext(this));
    }
    virtual void PreUpdate(UAnimInstance* Instance,float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,DeltaSeconds);
        const APawn* Pawn=Instance->TryGetPawnOwner();
        const FVector Velocity=Pawn?Pawn->GetVelocity():FVector::ZeroVector;
        const FVector Local=Pawn?Pawn->GetActorTransform().InverseTransformVectorNoScale(Velocity):FVector::ZeroVector;
        const float Direction=Velocity.SizeSquared2D()>1.f?FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)):0.f;
        // This template is a 2D blend space: X = direction, Y = cm/s.
        Locomotion.SetPosition(FVector(Direction,Velocity.Size2D(),0));
    }
    virtual void CacheBones() override { CombatSlot.CacheBones_AnyThread(FAnimationCacheBonesContext(this)); }
    virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
    {
        UpdateCounter.Increment();
        CombatSlot.Update_AnyThread(Context);
    }
    virtual bool Evaluate(FPoseContext& Output) override { CombatSlot.Evaluate_AnyThread(Output); return true; }
};
}

UAHAnimInstance::UAHAnimInstance()
{
    static ConstructorHelpers::FObjectFinder<UBlendSpace> Blend(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"));
    LocomotionBlendSpace=Blend.Object;
}
FAnimInstanceProxy* UAHAnimInstance::CreateAnimInstanceProxy() { return new FAHAnimationProxy(this); }
void UAHAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

void UAHAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Lazy-init: TryGetPawnOwner is safe to call every frame and returns null
    // before the owning pawn's BeginPlay, so we cache on first success.
    if (!OwnerCharacter.IsValid())
    {
        OwnerCharacter = Cast<AAHCharacter>(TryGetPawnOwner());
    }
    if (!OwnerCharacter.IsValid()) return;

    const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
    Speed      = Movement ? Movement->Velocity.Size2D() : 0.f;
    bIsMoving  = Speed > 10.f;
    bIsDead    = !OwnerCharacter->IsAlive();
    bIsAttacking = OwnerCharacter->bIsAttacking;
}
