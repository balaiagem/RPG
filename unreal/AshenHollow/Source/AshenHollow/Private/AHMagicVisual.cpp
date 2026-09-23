#include "AHMagicVisual.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAHMagicVisual::AAHMagicVisual()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/AshenHollow/FX/M_ArcaneDart"));
    // Up to four heads, each with six shrinking trail samples. No lights or collision.
    for(int32 I=0;I<28;++I)
    {
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Dart%d"),I));
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(Sphere.Object);
        Mesh->SetMaterial(0,Glow.Object);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCastShadow(false);
        Darts.Add(Mesh);
    }
}
void AAHMagicVisual::Initialize(const FVector& Origin,AActor* Target,float TravelSeconds,int32 Count)
{
    HeadCount=FMath::Clamp(Count,1,4);
    Start=Origin; Destination=Target; Duration=FMath::Max(.05f,TravelSeconds);
    SetActorLocation(Origin);
    SetLifeSpan(Duration+.25f);
    Tick(0.f);
}
void AAHMagicVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!Destination.IsValid()) { Destroy(); return; }
    Age+=DeltaSeconds;
    const FVector End=Destination->GetActorLocation()+FVector(0,0,35);
    const FVector Side=FVector::CrossProduct((End-Start).GetSafeNormal(),FVector::UpVector).GetSafeNormal();
    for(int32 I=0;I<Darts.Num();++I)
    {
        const int32 Dart=I/7, Trail=I%7;
        const float T=FMath::Clamp(Age/Duration-Trail*.028f,0.f,1.f);
        const FVector Arc=Side*((Dart-(HeadCount-1)*.5f)*95.f)+FVector(0,0,55.f+Dart*25.f);
        Darts[I]->SetWorldLocation(FMath::Lerp(Start,End,T)+Arc*FMath::Sin(PI*T));
        const float Radius=(Trail==0?.13f:.075f)*(1.f-Trail*.12f);
        Darts[I]->SetWorldScale3D(FVector(Radius));
        Darts[I]->SetVisibility(Dart<HeadCount && Age/Duration>=Trail*.028f);
    }
}
