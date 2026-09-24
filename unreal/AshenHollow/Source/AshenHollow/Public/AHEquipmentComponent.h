#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHEquipmentComponent.generated.h"
class USceneComponent;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class USkeletalMeshComponent;
enum class EAHWeaponKind:uint8;
UCLASS()
class ASHENHOLLOW_API UAHEquipmentComponent:public UActorComponent
{
    GENERATED_BODY()
public:
    UAHEquipmentComponent();
    /**
     * Rebuilds the held weapon (and shield, for sword and mace).
     * Uses the authored mesh named in the weapon art table in the .cpp when one
     * is set, and falls back to the engine-primitive weapon otherwise, so an
     * empty or stale asset path degrades to the old look instead of failing.
     */
    void Configure(EAHWeaponKind Kind);
    /** World position of the weapon's business end; spell and impact FX spawn here. */
    FVector Tip() const;
    /** Plays the held weapon's own firing clip, when it has one. Safe to call always. */
    void PlayShot();
private:
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Parts;
    UPROPERTY() TObjectPtr<USceneComponent> Grip;
    /** The component the last AttachArt created, so Configure can keep the weapon. */
    UPROPERTY() TObjectPtr<USceneComponent> LastAttached;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> WeaponMesh;
    const TCHAR* ShotPath=nullptr;
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
    /**
     * Attaches the authored mesh at Path under Parent, accepting either a static or
     * a skeletal mesh. Returns false when the path is empty or the asset is absent,
     * which is the caller's cue to build the primitive weapon instead.
     */
    bool AttachArt(USceneComponent* Parent,const TCHAR* Path,FVector Offset,FRotator Rotation,float Scale,bool bAutoUpright);
    void SkeletalPart(USceneComponent* Parent,USkeletalMesh* Mesh,FVector Position,FVector Scale,FRotator Rotation);
    /** The original cube-and-cylinder weapon, kept as the fallback. */
    void BuildPrimitiveWeapon(EAHWeaponKind Kind);
    void BuildPrimitiveShield(USceneComponent* Parent);
};
