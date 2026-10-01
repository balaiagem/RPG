#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHLifeAudioComponent.generated.h"

class USoundBase;
class USoundAttenuation;

UCLASS()
class ASHENHOLLOW_API UAHLifeAudioComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAHLifeAudioComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
    void Impact();
private:
    UPROPERTY() TArray<TObjectPtr<USoundBase>> Steps;
    UPROPERTY() TObjectPtr<USoundBase> HitSound;
    UPROPERTY() TObjectPtr<USoundAttenuation> Falloff;
    FVector Previous=FVector::ZeroVector;
    float Distance=0;
    float LastImpact=-1;
    int32 StepIndex=0;
};
