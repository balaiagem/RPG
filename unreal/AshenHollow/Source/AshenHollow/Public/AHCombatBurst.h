#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHCombatBurst.generated.h"
class UInstancedStaticMeshComponent;
class UMaterialInterface;
enum class EAHBurst:uint8 {Steel,Force,Heal,Rage,Guard};
UCLASS()
class ASHENHOLLOW_API AAHCombatBurst:public AActor
{
    GENERATED_BODY()
public:
    AAHCombatBurst();
    static void Emit(UWorld* World,const FVector& Position,EAHBurst Kind);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Particles;
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    EAHBurst Style=EAHBurst::Steel;
    float Age=0;
};
