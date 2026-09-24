#include "AHMagicVisual.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    struct FAHProjectileStyle
    {
        FLinearColor Tint;      // fed to M_CombatGlow's Tint parameter
        float Arc;              // sideways/upward bow of the flight path, in cm
        float HeadRadius;       // scale of the leading sphere
        float TrailFade;        // seconds of delay between trail samples
    };

    /** One row per EAHProjectileLook, in enum order. Arrow ignores every field. */
    const FAHProjectileStyle GStyles[] =
    {
        { FLinearColor(1.00f, 1.00f, 1.00f, 1.f),  0.f, .00f, .000f },   // Arrow
        { FLinearColor(1.00f, 0.42f, 0.10f, 1.f), 40.f, .15f, .030f },   // Fire
        { FLinearColor(0.55f, 0.85f, 1.00f, 1.f), 30.f, .12f, .026f },   // Frost
        { FLinearColor(0.62f, 0.35f, 1.00f, 1.f), 95.f, .13f, .028f },   // Arcane
        { FLinearColor(1.00f, 0.86f, 0.42f, 1.f), 55.f, .16f, .032f },   // Radiant
        { FLinearColor(0.42f, 0.90f, 0.40f, 1.f), 45.f, .14f, .030f },   // Necrotic
    };
    static_assert(UE_ARRAY_COUNT(GStyles) == static_cast<int32>(EAHProjectileLook::Count),
        "One projectile style row per EAHProjectileLook, in enum order.");

    const FAHProjectileStyle& StyleFor(EAHProjectileLook Look)
    {
        return GStyles[FMath::Clamp(static_cast<int32>(Look), 0,
                                    static_cast<int32>(EAHProjectileLook::Count) - 1)];
    }
}

AAHMagicVisual::AAHMagicVisual()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    // M_CombatGlow, not M_ArcaneDart: only the former exposes a Tint parameter,
    // and tinting is the whole reason a frost ray no longer looks like an arrow.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/AshenHollow/FX/M_CombatGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Steel(TEXT("/Game/AshenHollow/FX/M_WeaponSteel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/AshenHollow/FX/M_WeaponLeather"));
    GlowMaterial=Glow.Object;

    auto Cosmetic=[this](UStaticMeshComponent* Mesh, UStaticMesh* Shape, UMaterialInterface* Material)
    {
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCastShadow(false);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(Shape);
        Mesh->SetMaterial(0,Material);
    };

    // Up to four heads, each with seven shrinking trail samples.
    for(int32 I=0;I<28;++I)
    {
        auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Dart%d"),I));
        Cosmetic(Mesh,Sphere.Object,Glow.Object);
        Darts.Add(Mesh);
    }

    // ── The arrow ────────────────────────────────────────────────────────────
    // Built nose-first along the actor's local +X, so pointing it down the flight
    // path is just SetActorRotation(direction.Rotation()) once per frame. The
    // engine primitives run up +Z, hence the 90 degree pitch on each piece.
    auto* Shaft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowShaft"));
    Cosmetic(Shaft,Cylinder.Object,Wood.Object);
    Shaft->SetRelativeRotation(FRotator(90.f,0.f,0.f));
    Shaft->SetRelativeScale3D(FVector(.022f,.022f,.72f));
    Shaft->SetRelativeLocation(FVector(-36.f,0.f,0.f));
    ArrowParts.Add(Shaft);

    auto* Head=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowHead"));
    Cosmetic(Head,Cone.Object,Steel.Object);
    Head->SetRelativeRotation(FRotator(90.f,0.f,0.f));
    Head->SetRelativeScale3D(FVector(.05f,.05f,.13f));
    Head->SetRelativeLocation(FVector(-6.f,0.f,0.f));
    ArrowParts.Add(Head);

    for(int32 I=0;I<2;++I)
    {
        auto* Fletch=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ArrowFletch%d"),I));
        Cosmetic(Fletch,Cube.Object,Wood.Object);
        Fletch->SetRelativeRotation(FRotator(0.f,0.f,I*90.f));
        Fletch->SetRelativeScale3D(FVector(.14f,.012f,.055f));
        Fletch->SetRelativeLocation(FVector(-66.f,0.f,0.f));
        ArrowParts.Add(Fletch);
    }
}

void AAHMagicVisual::Initialize(const FVector& Origin,AActor* Target,float TravelSeconds,int32 Count,
                                EAHProjectileLook Look)
{
    HeadCount=FMath::Clamp(Count,1,4);
    Start=Origin; Destination=Target; Duration=FMath::Max(.05f,TravelSeconds);
    Style=Look; bArrow=(Look==EAHProjectileLook::Arrow);
    SetActorLocation(Origin);
    SetLifeSpan(Duration+.25f);

    // One set is always dark. Visibility is settled here rather than every frame.
    for(auto& Part:ArrowParts) if(Part) Part->SetVisibility(bArrow);
    if(bArrow)
    {
        for(auto& Dart:Darts) if(Dart) Dart->SetVisibility(false);
    }
    else
    {
        const FLinearColor Tint=StyleFor(Style).Tint;
        for(auto& Dart:Darts)
        {
            if(!Dart) continue;
            // A dynamic instance per head is what lets fire, frost and necrotic
            // share one material and still read as three different spells.
            if(UMaterialInstanceDynamic* Painted=Dart->CreateDynamicMaterialInstance(0,GlowMaterial))
                Painted->SetVectorParameterValue(TEXT("Tint"),Tint);
        }
    }
    Tick(0.f);
}

void AAHMagicVisual::TickArrow(float Progress,const FVector& End)
{
    const FVector Flight=End-Start;
    // Barely any arc. An arrow that loops like a magic missile stops reading as
    // an arrow, so this is a shallow sag rather than a lob.
    const float Sag=FMath::Sin(PI*Progress)*FMath::Min(Flight.Size()*.03f,45.f);
    const FVector Here=FMath::Lerp(Start,End,Progress)+FVector(0.f,0.f,Sag);
    SetActorLocation(Here);
    if(!Flight.IsNearlyZero())
        SetActorRotation((FVector(Flight.X,Flight.Y,Flight.Z-Sag*2.f)).Rotation());
}

void AAHMagicVisual::TickDarts(float Progress,const FVector& End)
{
    const FAHProjectileStyle& Look=StyleFor(Style);
    const FVector Side=FVector::CrossProduct((End-Start).GetSafeNormal(),FVector::UpVector).GetSafeNormal();
    for(int32 I=0;I<Darts.Num();++I)
    {
        const int32 Dart=I/7, Trail=I%7;
        const float T=FMath::Clamp(Progress-Trail*Look.TrailFade,0.f,1.f);
        const FVector Bow=Side*((Dart-(HeadCount-1)*.5f)*Look.Arc)+FVector(0,0,55.f+Dart*25.f);
        Darts[I]->SetWorldLocation(FMath::Lerp(Start,End,T)+Bow*FMath::Sin(PI*T));
        const float Radius=(Trail==0?Look.HeadRadius:Look.HeadRadius*.58f)*(1.f-Trail*.12f);
        Darts[I]->SetWorldScale3D(FVector(Radius));
        Darts[I]->SetVisibility(Dart<HeadCount && Progress>=Trail*Look.TrailFade);
    }
}

void AAHMagicVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!Destination.IsValid()) { Destroy(); return; }
    Age+=DeltaSeconds;
    const FVector End=Destination->GetActorLocation()+FVector(0,0,35);
    const float Progress=FMath::Clamp(Age/Duration,0.f,1.f);
    if(bArrow) TickArrow(Progress,End); else TickDarts(Progress,End);
}
