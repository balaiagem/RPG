#pragma once
#include "CoreMinimal.h"
#include "AHCharacter.h"
#include "AHWildEnemy.generated.h"

UCLASS()
class ASHENHOLLOW_API AAHWildEnemy : public AAHCharacter
{
    GENERATED_BODY()
public:
    AAHWildEnemy();
    void ConfigureWildlife(int32 Kind,int32 Seed);
    virtual void Tick(float DeltaSeconds) override;
    int32 WildlifeKind=1;
private:
    UPROPERTY() TObjectPtr<USceneComponent> AnimalRoot;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Legs;
    float Gait=0;
};
