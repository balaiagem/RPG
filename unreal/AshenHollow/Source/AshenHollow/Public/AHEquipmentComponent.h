#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHEquipmentComponent.generated.h"
class USceneComponent;
class UMaterialInterface;
class UStaticMesh;
enum class EAHWeaponKind:uint8;
UCLASS()
class ASHENHOLLOW_API UAHEquipmentComponent:public UActorComponent
{
    GENERATED_BODY()
public:
    UAHEquipmentComponent();
    void Configure(EAHWeaponKind Kind);
    FVector Tip() const;
private:
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Parts;
    UPROPERTY() TObjectPtr<USceneComponent> Grip;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UMaterialInterface> Steel;
    UPROPERTY() TObjectPtr<UMaterialInterface> Gold;
    UPROPERTY() TObjectPtr<UMaterialInterface> Leather;
    UPROPERTY() TObjectPtr<UMaterialInterface> Glow;
    float TipHeight=75;
    USceneComponent* Anchor(FName Bone);
    void Part(USceneComponent* Parent,UStaticMesh* Mesh,UMaterialInterface* Material,FVector Position,FVector Scale,FRotator Rotation=FRotator::ZeroRotator);
};
