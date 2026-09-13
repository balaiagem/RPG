#include "AHNotify_MeleeImpact.h"
#include "AHCharacter.h"

void UAHNotify_MeleeImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;
    if (AAHCharacter* Character = Cast<AAHCharacter>(MeshComp->GetOwner()))
    {
        Character->OnMeleeImpactNotify();
    }
}
