#include "AHEquipmentComponent.h"
#include "AHCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
UAHEquipmentComponent::UAHEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> C(TEXT("/Engine/BasicShapes/Cube")),S(TEXT("/Engine/BasicShapes/Sphere")),Y(TEXT("/Engine/BasicShapes/Cylinder"));
    Cube=C.Object; Sphere=S.Object; Cylinder=Y.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/AshenHollow/FX/M_WeaponSteel")),G(TEXT("/Game/AshenHollow/FX/M_WeaponGold")),L(TEXT("/Game/AshenHollow/FX/M_WeaponLeather")),E(TEXT("/Game/AshenHollow/FX/M_CombatGlow"));
    Steel=M.Object; Gold=G.Object; Leather=L.Object; Glow=E.Object;
}
USceneComponent* UAHEquipmentComponent::Anchor(FName Bone)
{
    auto* Character=CastChecked<AAHCharacter>(GetOwner());
    auto* Node=NewObject<USceneComponent>(GetOwner());
    GetOwner()->AddInstanceComponent(Node);
    Node->SetupAttachment(Character->GetMesh(),Bone); Node->RegisterComponent();
    // Establish a vertical resting grip; subsequent wrist motion drives the weapon.
    Node->SetWorldRotation(Character->GetActorRotation());
    Parts.Add(Node); return Node;
}
void UAHEquipmentComponent::Part(USceneComponent* Parent,UStaticMesh* Mesh,UMaterialInterface* Material,FVector Position,FVector Scale,FRotator Rotation)
{
    auto* P=NewObject<UStaticMeshComponent>(GetOwner()); GetOwner()->AddInstanceComponent(P);
    // SetStaticMesh updates navigation even before registration. Disable it first
    // so a cosmetic weapon cannot enqueue geometry at the character's spawn.
    P->SetCanEverAffectNavigation(false);
    P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetGenerateOverlapEvents(false);
    P->SetupAttachment(Parent); P->SetStaticMesh(Mesh); P->SetMaterial(0,Material);
    P->SetRelativeLocation(Position); P->SetRelativeScale3D(Scale); P->SetRelativeRotation(Rotation);
    P->RegisterComponent(); Parts.Add(P);
}
void UAHEquipmentComponent::Configure(EAHHeroClass Class)
{
    for(int32 I=Parts.Num()-1;I>=0;--I) if(IsValid(Parts[I])) Parts[I]->DestroyComponent();
    Parts.Reset(); Grip=Anchor(TEXT("hand_r")); TipHeight=75;
    const bool Staff=Class==EAHHeroClass::Wizard;
    Part(Grip,Cylinder,Leather,FVector(0,0,Staff?15:0),FVector(.045,.045,Staff?1.45:.3));
    for(float Z:{-12.f,12.f}) Part(Grip,Cylinder,Gold,FVector(0,0,Z),FVector(.065,.065,.035));
    if(Class==EAHHeroClass::Fighter)
    {
        Part(Grip,Cube,Gold,FVector(0,0,17),FVector(.05,.27,.045));
        Part(Grip,Cube,Steel,FVector(0,0,48),FVector(.024,.085,.60));
        Part(Grip,Cube,Gold,FVector(0,0,45),FVector(.03,.015,.47));
        Part(Grip,Sphere,Gold,FVector(0,0,-17),FVector(.09));
    }
    else if(Class==EAHHeroClass::Barbarian)
    {
        Part(Grip,Cylinder,Leather,FVector(0,0,27),FVector(.055,.055,.85));
        for(float Sign:{-1.f,1.f}) Part(Grip,Cube,Steel,FVector(0,Sign*16,60),FVector(.045,.29,.28),FRotator(0,0,Sign*18));
        Part(Grip,Sphere,Gold,FVector(0,0,61),FVector(.10));
    }
    else if(Class==EAHHeroClass::Cleric)
    {
        Part(Grip,Cylinder,Steel,FVector(0,0,30),FVector(.04,.04,.55));
        for(int I=0;I<4;++I) Part(Grip,Cube,Gold,FVector(0,0,57),FVector(.22,.035,.23),FRotator(0,I*45,0));
        TipHeight=68;
    }
    else
    {
        Part(Grip,Cylinder,Gold,FVector(0,0,83),FVector(.13,.13,.09));
        Part(Grip,Sphere,Glow,FVector(0,0,94),FVector(.17,.17,.31));
        TipHeight=110;
    }
    if(Class==EAHHeroClass::Fighter || Class==EAHHeroClass::Cleric)
    {
        auto* Shield=Anchor(TEXT("hand_l"));
        Part(Shield,Cylinder,Gold,FVector(0,0,0),FVector(.50,.50,.055),FRotator(90,0,0));
        Part(Shield,Cylinder,Steel,FVector(4,0,0),FVector(.44,.44,.035),FRotator(90,0,0));
        Part(Shield,Sphere,Gold,FVector(7,0,0),FVector(.1));
    }
}
FVector UAHEquipmentComponent::Tip() const { return Grip?Grip->GetComponentTransform().TransformPosition(FVector(0,0,TipHeight)):GetOwner()->GetActorLocation(); }
