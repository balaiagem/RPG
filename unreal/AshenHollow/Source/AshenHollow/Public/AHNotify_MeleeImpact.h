#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AHNotify_MeleeImpact.generated.h"

/**
 * Place this notify on the attack animation exactly where the weapon connects.
 *
 * When triggered, the owning AAHCharacter resolves the d20 impact immediately —
 * replacing the fallback timer with frame-perfect hit detection.
 *
 * HOW TO ADD TO THE ANIMATION:
 *   1. Compile the project so this class appears in the editor.
 *   2. Open MM_Attack_01 (or your custom attack AnimMontage) in the Animation
 *      editor.
 *   3. Scrub to the frame where the weapon reaches the target (~33% through).
 *   4. Right-click the Notifies track > Add Notify > AH Melee Impact.
 *   5. Save the asset.  Next play session the notify fires at that frame and
 *      AHCharacter::OnMeleeImpactNotify() is called instead of the timer.
 *
 * The timer fallback in AHCharacter remains active until the notify is added,
 * so gameplay continues to work during development.
 */
UCLASS(DisplayName="AH Melee Impact")
class ASHENHOLLOW_API UAHNotify_MeleeImpact : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    /** Label shown in the Notifies track in the animation editor. */
    virtual FString GetNotifyName_Implementation() const override
    {
        return TEXT("AH_MeleeImpact");
    }
};
