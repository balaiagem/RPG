#include "AHArena.h"

namespace
{
    const TCHAR* const PACK = TEXT("/Game/Fantastic_Village_Pack/meshes/props/");

    /**
     * What can be dropped into the arena.
     *
     * Radius and Height here are only a first guess, used for spacing while the
     * layout is being drawn. The game mode replaces both with the mesh's measured
     * bounds the moment the prop exists, so a wrong estimate costs nothing.
     */
    struct FAHPropKind
    {
        const TCHAR* Path;
        float Radius;
        float Height;
        int32 Weight;       // relative chance of being picked
    };

    const FAHPropKind GProps[] =
    {
        { TEXT("container/SM_PROP_barrel_01"),   45.f,  90.f, 4 },
        { TEXT("container/SM_PROP_barrel_03"),   45.f,  95.f, 4 },
        { TEXT("container/SM_PROP_crate_01"),    55.f,  80.f, 4 },
        { TEXT("container/SM_PROP_box_01"),      50.f,  70.f, 3 },
        { TEXT("container/SM_PROP_box_03"),      50.f,  65.f, 3 },
        { TEXT("natural/SM_PROP_hay_01"),        70.f, 100.f, 3 },
        { TEXT("natural/SM_PROP_hay_03"),        70.f, 110.f, 3 },
        { TEXT("natural/SM_PROP_stone_01"),      80.f, 120.f, 2 },
        { TEXT("natural/SM_PROP_stone_02"),      90.f, 140.f, 2 },
        { TEXT("vehicles/SM_PROP_cart_02"),     120.f, 160.f, 2 },
        { TEXT("construction/SM_PROP_market_v01_01"), 130.f, 220.f, 1 },
    };

    int32 TotalWeight()
    {
        int32 Sum = 0;
        for (const FAHPropKind& Kind : GProps) Sum += Kind.Weight;
        return Sum;
    }

    const FAHPropKind& PickKind(FRandomStream& Dice)
    {
        int32 Roll = Dice.RandRange(1, TotalWeight());
        for (const FAHPropKind& Kind : GProps)
        {
            Roll -= Kind.Weight;
            if (Roll <= 0) return Kind;
        }
        return GProps[0];
    }

    /**
     * Shortest 2D distance from Point to the segment A-B, and where along it it fell.
     *
     * Everything here is double on purpose. Vector maths in UE5 is double
     * precision, so pulling a length into a float both narrows it quietly and
     * stops FMath::Clamp deducing a single type from its arguments.
     */
    double DistanceToSegment(const FVector& Point, const FVector& A, const FVector& B, double& OutAlong)
    {
        const FVector2D P(Point.X, Point.Y), Start(A.X, A.Y), End(B.X, B.Y);
        const FVector2D Span = End - Start;
        const double Length2 = Span.SizeSquared();
        OutAlong = Length2 > UE_KINDA_SMALL_NUMBER
                 ? FMath::Clamp(FVector2D::DotProduct(P - Start, Span) / Length2, 0.0, 1.0)
                 : 0.0;
        return FVector2D::Distance(P, Start + Span * OutAlong);
    }
}

int32 AHArena::ArmorBonus(EAHCover Cover)
{
    switch (Cover)
    {
        case EAHCover::Half:          return 2;
        case EAHCover::ThreeQuarters: return 5;
        default:                      return 0;
    }
}

const TCHAR* AHArena::CoverName(EAHCover Cover)
{
    switch (Cover)
    {
        case EAHCover::Half:          return TEXT("COBERTURA PARCIAL");
        case EAHCover::ThreeQuarters: return TEXT("COBERTURA 3/4");
        case EAHCover::Total:         return TEXT("COBERTURA TOTAL");
        default:                      return TEXT("");
    }
}

bool AHArena::HasHighGround(const FVector& From, const FVector& To)
{
    return From.Z - To.Z >= HighGroundStep;
}

EAHCover AHArena::CoverBetween(const TArray<FAHArenaPiece>& Pieces, const FVector& From, const FVector& To)
{
    EAHCover Best = EAHCover::None;
    for (const FAHArenaPiece& Piece : Pieces)
    {
        double Along = 0.0;
        const double Sideways = DistanceToSegment(Piece.Location, From, To, Along);
        if (Sideways > Piece.Radius) continue;

        // Something at your own feet is not cover you are hiding behind, and
        // something at the shooter's feet is not cover for the target either.
        const double Span = FVector::Dist2D(From, To);
        if (Along * Span < 80.0 || (1.0 - Along) * Span < 40.0) continue;

        // Measured against the TARGET's ground, so a barrel that hides a man on
        // the flagstones hides nobody standing on the raised deck above it.
        const double Rise = (Piece.Location.Z + Piece.Height) - To.Z;
        if (Rise >= BodyHeight * .70f)      Best = EAHCover::ThreeQuarters;
        else if (Rise >= BodyHeight * .30f && Best == EAHCover::None)
                                            Best = EAHCover::Half;
    }
    return Best;
}

TArray<FAHArenaPiece> AHArena::Generate(FRandomStream& Dice, const FVector& HeroSpawn, const FVector& FoeSpawn)
{
    TArray<FAHArenaPiece> Pieces;

    // Props never reach the fence line, and never the corridor between the two
    // spawns: an unlucky seed must not be able to wall the fight off before it
    // starts, and the opening approach should always be a choice rather than a
    // detour forced on you.
    const float Edge     = PlayHalfSize - 220.f;
    const float Corridor = 300.f;
    const int32 Wanted   = Dice.RandRange(9, 14);

    for (int32 Attempt = 0; Attempt < Wanted * 14 && Pieces.Num() < Wanted; ++Attempt)
    {
        const FVector Where(Dice.FRandRange(-Edge, Edge), Dice.FRandRange(-Edge, Edge), 0.f);

        double Along = 0.0;
        if (DistanceToSegment(Where, HeroSpawn, FoeSpawn, Along) < Corridor) continue;
        if (FVector::Dist2D(Where, HeroSpawn) < 400.f) continue;
        if (FVector::Dist2D(Where, FoeSpawn)  < 400.f) continue;
        // Off the raised deck and its ramps: a barrel placed at ground level
        // inside the deck would simply be buried in it.
        if (FMath::Abs(Where.X - DeckCentreX) < DeckKeepOutX
         && FMath::Abs(Where.Y)               < DeckKeepOutY) continue;

        const FAHPropKind& Kind = PickKind(Dice);

        bool bCrowded = false;
        for (const FAHArenaPiece& Placed : Pieces)
            if (FVector::Dist2D(Where, Placed.Location) < Placed.Radius + Kind.Radius + 180.f)
            { bCrowded = true; break; }
        if (bCrowded) continue;

        FAHArenaPiece Piece;
        Piece.MeshPath = FString(PACK) + Kind.Path;
        Piece.Location = Where;
        Piece.Yaw      = Dice.FRandRange(0.f, 360.f);
        Piece.Scale    = Dice.FRandRange(.9f, 1.15f);
        Piece.Radius   = Kind.Radius * Piece.Scale;
        Piece.Height   = Kind.Height * Piece.Scale;
        Pieces.Add(Piece);
    }
    return Pieces;
}
