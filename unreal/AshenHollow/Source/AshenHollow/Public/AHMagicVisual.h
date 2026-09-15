#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHMagicVisual.generated.h"
class UStaticMeshComponent;

/** Cosmetic only: combat owns damage and the contact frame. */
UCLASS()
class ASHENHOLLOW_API AAHMagicVisual : public AActor
{
    GENERATED_BODY()
public:
    AAHMagicVisual();
    void Initialize(const FVector& Origin, AActor* Target, float TravelSeconds);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Darts;
    UPROPERTY() TWeakObjectPtr<AActor> Destination;
    FVector Start;
    float Age=0.f, Duration=.4f;
};
