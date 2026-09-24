#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHMagicVisual.generated.h"
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * What a projectile should look like in flight.
 *
 * Every ranged attack used to draw the same three glowing spheres, so an arrow,
 * a fire bolt and a magic missile were indistinguishable. The look is chosen from
 * the attack, not from the class, so a wizard's Fire Bolt and a ranger's arrow
 * never collide again.
 */
UENUM()
enum class EAHProjectileLook : uint8
{
    Arrow,      // a real shaft, head and fletching, flying nearly flat
    Fire,
    Frost,
    Arcane,
    Radiant,
    Necrotic,
    Count UMETA(Hidden)
};

/** Cosmetic only: combat owns damage and the contact frame. */
UCLASS()
class ASHENHOLLOW_API AAHMagicVisual : public AActor
{
    GENERATED_BODY()
public:
    AAHMagicVisual();
    void Initialize(const FVector& Origin, AActor* Target, float TravelSeconds, int32 Count=3,
                    EAHProjectileLook Look=EAHProjectileLook::Arcane);
    virtual void Tick(float DeltaSeconds) override;
private:
    /** Glowing heads with trails. Used by every look except Arrow. */
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Darts;
    /** Shaft, head and two fletches, assembled pointing down the arrow's own +X. */
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ArrowParts;
    UPROPERTY() TObjectPtr<UMaterialInterface> GlowMaterial;
    UPROPERTY() TWeakObjectPtr<AActor> Destination;
    FVector Start;
    int32 HeadCount=3;
    float Age=0.f, Duration=.4f;
    EAHProjectileLook Style=EAHProjectileLook::Arcane;
    bool bArrow=false;
    void TickArrow(float Progress, const FVector& End);
    void TickDarts(float Progress, const FVector& End);
};
