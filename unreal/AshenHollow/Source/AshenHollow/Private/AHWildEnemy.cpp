#include "AHWildEnemy.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AAHWildEnemy::AAHWildEnemy()
{
    AnimalRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Animal"));
    AnimalRoot->SetupAttachment(GetCapsuleComponent());
    AnimalRoot->SetRelativeLocation(FVector(0,0,-96));
}
void AAHWildEnemy::ConfigureWildlife(int32 Kind,int32 Seed)
{
    BecomeEnemy(Seed);
    WildlifeKind=Kind;
    const bool Bear=Kind==2;
    HeroClass=EAHHeroClass::Fighter; Ancestry=EAHAncestry::Human;
    EnemyName=Bear?TEXT("URSO PARDO"):TEXT("LOBO CINZENTO");
    Health=MaxHealth=Bear?38:16;
    ArmorClass=Bear?12:13; AttackBonus=Bear?5:4;
    DamageSides=Bear?10:6; DamageModifier=Bear?4:2;
    RangedRange=RangedSides=RangedBonus=0; ClassCharges=SpellSlots2=0;
    BaseMovement=Bear?900:1200;
    bIndoorFoe=false;
    GetMesh()->SetVisibility(false,true);
    GetMesh()->SetComponentTickEnabled(false);
    auto Make=[&](const FString& Suffix,FVector Position)
    {
        auto* Part=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(Part);
        Part->SetCanEverAffectNavigation(false); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetupAttachment(AnimalRoot);
        Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/AshenHollow/Wildlife/Meshes/SM_%s_%s"),Bear?TEXT("Bear"):TEXT("Wolf"),*Suffix)));
        Part->SetRelativeLocation(Position); Part->RegisterComponent(); return Part;
    };
    Make(TEXT("Body"),FVector::ZeroVector);
    for(int32 I=0;I<4;++I)
        Legs.Add(Make(TEXT("Leg"),FVector((I<2?1:-1)*(Bear?45:37),(I%2?1:-1)*(Bear?29:19),Bear?74:58)));
    Gait=Seed%100;
}
void AAHWildEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!IsAlive()) { AnimalRoot->SetRelativeRotation(FRotator(0,0,80)); return; }
    const float Speed=GetVelocity().Size2D();
    Gait+=DeltaSeconds*(WildlifeKind==2?5.f:8.f)*FMath::Clamp(Speed/170.f,.15f,2.f);
    const float Amount=FMath::Clamp(Speed/220.f,0.f,1.f);
    for(int32 I=0;I<Legs.Num();++I)
        Legs[I]->SetRelativeRotation(FRotator(FMath::Sin(Gait+(I==0||I==3?0:PI))*24*Amount,0,0));
    const float Lunge=bIsAttacking?FMath::Sin(GetWorld()->GetTimeSeconds()*12)*7:0;
    AnimalRoot->SetRelativeLocation(FVector(Lunge,0,-96+FMath::Abs(FMath::Sin(Gait))*3*Amount));
}
