#include "AHCombatBurst.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
AAHCombatBurst::AAHCombatBurst()
{
    PrimaryActorTick.bCanEverTick=true;
    Particles=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks")); RootComponent=Particles;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/AshenHollow/FX/M_CombatGlow"));
    Particles->SetCanEverAffectNavigation(false);
    Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision); Particles->SetGenerateOverlapEvents(false);
    Material=M.Object; Particles->SetStaticMesh(Mesh.Object); Particles->SetMaterial(0,Material);
    Particles->SetCastShadow(false);
}
void AAHCombatBurst::Emit(UWorld* World,const FVector& Position,EAHBurst Kind)
{
    if(!World || World->GetNetMode()==NM_DedicatedServer) return;
    auto* Burst=World->SpawnActor<AAHCombatBurst>(Position,FRotator::ZeroRotator);
    if(!Burst) return;
    Burst->Style=Kind; Burst->SetLifeSpan(.75f);
    const FLinearColor Color=Kind==EAHBurst::Heal?FLinearColor(.3f,5.f,1.f):Kind==EAHBurst::Rage?FLinearColor(7.f,.3f,.02f):Kind==EAHBurst::Force?FLinearColor(.3f,3.5f,7.f):Kind==EAHBurst::Guard?FLinearColor(1.f,2.f,4.f):FLinearColor(5.f,2.f,.35f);
    auto* Tint=UMaterialInstanceDynamic::Create(Burst->Material,Burst); Tint->SetVectorParameterValue(TEXT("Tint"),Color);
    Burst->Particles->SetMaterial(0,Tint);
    for(int I=0;I<16;++I) Burst->Particles->AddInstance(FTransform(FVector::ZeroVector));
    Burst->Tick(0);
}
void AAHCombatBurst::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); Age+=DeltaSeconds;
    const float T=FMath::Clamp(Age/.7f,0.f,1.f);
    for(int I=0;I<Particles->GetInstanceCount();++I)
    {
        const float Angle=I*2.399963f;
        const bool Rising=Style==EAHBurst::Heal || Style==EAHBurst::Rage;
        const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),.25f+(I%4)*.22f);
        const FVector Position=Rising?FVector(FMath::Cos(Angle+T*2)*35,FMath::Sin(Angle+T*2)*35,T*95+(I%4)*10):Direction*(8+T*80);
        const float Size=(1-T)*(.04f+(I%3)*.012f);
        const FVector Scale=Rising?FVector(Size):FVector(Size*3,Size*.5f,Size*.5f);
        Particles->UpdateInstanceTransform(I,FTransform(Direction.Rotation(),Position,Scale),false,false,true);
    }
    Particles->MarkRenderStateDirty();
}
