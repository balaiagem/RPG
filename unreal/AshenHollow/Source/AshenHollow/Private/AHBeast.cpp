#include "AHBeast.h"
#include "AHCharacter.h"
#include "AHGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

AAHBeast::AAHBeast()
{
    PrimaryActorTick.bCanEverTick = true;
    Corpo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Corpo"));
    SetRootComponent(Corpo);
    Corpo->SetMobility(EComponentMobility::Movable);
    // No collision, and never in the navmesh. See the note in the header: a
    // moving obstacle is the one thing Recast cannot be asked to keep up with.
    Corpo->SetCollisionProfileName(TEXT("NoCollision"));
    Corpo->SetCanEverAffectNavigation(false);
    Corpo->SetCastShadow(true);
}

void AAHBeast::Settle(const FAHBeastSpot& Spot, int32 Seed, UStaticMesh* Body, float FloorZ)
{
    Dice.Initialize(Seed ? Seed : 1);
    Especie = Spot.Kind;
    Casa    = FVector(Spot.Where.X, Spot.Where.Y, FloorZ);
    Alcance = Spot.Range;
    Chao    = FloorZ;
    Salto   = Dice.FRandRange(0.f, 6.28f);
    Volta   = Dice.FRandRange(0.f, 6.28f);
    Altura  = Dice.FRandRange(900.f, 1700.f);
    if (Body) Corpo->SetStaticMesh(Body);

    switch (Especie)
    {
    case EAHBeast::Galinha: Passo =  62.f; break;
    case EAHBeast::Veado:   Passo = 150.f; break;
    default:                Passo = 280.f; break;      // a crow, going round
    }
    Destino     = Casa;
    ProximoAlvo = 0.f;
    SetActorLocation(FVector(Casa.X, Casa.Y, FloorZ));
    SetActorRotation(FRotator(0.f, Dice.FRandRange(0.f, 360.f), 0.f));
}

void AAHBeast::EscolheDestino()
{
    /**
     * Four tries, each one traced.
     *
     * A yard has a barn in it and a pasture has trees in it, and an animal
     * that walks through either is worse than an animal that stands still --
     * the whole reason this class exists is to be believed from a distance.
     * The trace is knee height, against static geometry only, and it costs one
     * ray every few seconds per animal.
     */
    const FVector Here = GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BeastWalk), false, this);
    for (int32 Try = 0; Try < 4; ++Try)
    {
        const float Rad  = FMath::DegreesToRadians(Dice.FRandRange(0.f, 360.f));
        const float Out  = Dice.FRandRange(Alcance * .3f, Alcance);
        const FVector Want(Casa.X + FMath::Cos(Rad) * Out,
                           Casa.Y + FMath::Sin(Rad) * Out, Here.Z);
        FHitResult Blocked;
        if (GetWorld()->LineTraceSingleByChannel(Blocked, Here + FVector(0, 0, 30),
                Want + FVector(0, 0, 30), ECC_WorldStatic, Query))
            continue;
        Destino = Want;
        return;
    }
    // Nowhere to go: stay put and ask again shortly. Standing still for a few
    // seconds is what a penned hen does anyway.
    Destino = Here;
}

void AAHBeast::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Now = GetWorld()->GetTimeSeconds();
    FVector Where = GetActorLocation();

    // ── The crow ─────────────────────────────────────────────────────────
    // Round and round, above everything. No ground trace, no obstacle test and
    // no destination: a circle is already a convincing bird, and a bird is the
    // one animal that is allowed to ignore the scenery.
    if (Especie == EAHBeast::Corvo)
    {
        Volta += DeltaSeconds * (Passo / FMath::Max(Alcance, 200.f));
        const float Bob = FMath::Sin(Now * 1.3f + Salto) * 90.f;
        const FVector Voando(Casa.X + FMath::Cos(Volta) * Alcance,
                             Casa.Y + FMath::Sin(Volta) * Alcance,
                             Chao + Altura + Bob);
        SetActorLocation(Voando);
        // Facing along the circle, and banked into it a little.
        SetActorRotation(FRotator(-6.f, FMath::RadiansToDegrees(Volta) + 90.f, 14.f));
        return;
    }

    // ── The ground ───────────────────────────────────────────────────────
    if (Now >= ProximoChao)
    {
        ProximoChao = Now + .25f;
        if (const auto* Mode = GetWorld()->GetAuthGameMode<AAHGameMode>())
        {
            bool bFound = false;
            const float Under = Mode->TerrainZ(static_cast<float>(Where.X),
                                               static_cast<float>(Where.Y), &bFound);
            if (bFound) Chao = Under;
        }
    }

    // ── The deer bolts ───────────────────────────────────────────────────
    if (Especie == EAHBeast::Veado)
    {
        if (Assustado > 0.f) Assustado -= DeltaSeconds;
        if (Assustado <= 0.f)
        {
            if (const auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
                if (FVector::Dist2D(Where, Hero->GetActorLocation()) < 1000.f)
                {
                    Assustado = 3.5f;
                    // Straight away from him, and further than it normally
                    // strays: a deer that bolts three metres is a cow.
                    Destino = Where + (Where - Hero->GetActorLocation()).GetSafeNormal2D() * 2200.f;
                    ProximoAlvo = Now + 3.5f;
                }
        }
    }

    if (Now >= ProximoAlvo)
    {
        ProximoAlvo = Now + Dice.FRandRange(Especie == EAHBeast::Galinha ? 1.6f : 4.f,
                                            Especie == EAHBeast::Galinha ? 4.2f : 9.f);
        EscolheDestino();
    }

    const float Speed = Passo * (Assustado > 0.f ? 3.0f : 1.f);
    const FVector Flat(Destino.X - Where.X, Destino.Y - Where.Y, 0.f);
    const float   Away = Flat.Size2D();
    if (Away > 12.f)
    {
        const FVector Step = Flat / Away * FMath::Min(Speed * DeltaSeconds, Away);
        Where += Step;
        const FRotator Want(0.f, static_cast<float>(
            FMath::RadiansToDegrees(FMath::Atan2(Flat.Y, Flat.X))), 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 6.f));
    }

    /**
     * The hop.
     *
     * A static mesh sliding across the grass is the giveaway, and it is the one
     * thing here that would have needed a skeleton to fix properly. It does not:
     * a hen lifted a few centimetres on the absolute of a sine while it is
     * moving, and flat on the ground when it stops, reads as a hen walking.
     * Deer get a much smaller one, which reads as a gait rather than a bounce.
     */
    const float Lift = Away > 12.f
        ? FMath::Abs(FMath::Sin(Now * (Especie == EAHBeast::Galinha ? 7.5f : 4.5f) + Salto))
          * (Especie == EAHBeast::Galinha ? 11.f : 6.f)
        : 0.f;
    SetActorLocation(FVector(Where.X, Where.Y, Chao + Lift));
}
