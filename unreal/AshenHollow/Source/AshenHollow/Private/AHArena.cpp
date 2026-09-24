#include "AHArena.h"

namespace
{
    const TCHAR* const PROP  = TEXT("/Game/Fantastic_Village_Pack/meshes/props/");
    const TCHAR* const ENV   = TEXT("/Game/Fantastic_Village_Pack/meshes/environment/");
    const TCHAR* const BPBLD = TEXT("/Game/Fantastic_Village_Pack/blueprints/buildings/");
    const TCHAR* const BPROP = TEXT("/Game/Fantastic_Village_Pack/blueprints/props/");
    const TCHAR* const CUBE  = TEXT("/Engine/BasicShapes/Cube");

    struct FAHPropKind { const TCHAR* Path; float Radius; float Height; int32 Weight; };  // Height: standing on flat ground, so it is also its top

    /** Loose obstacles. Radius and Height are spacing estimates only; the game
     *  mode overwrites both with the mesh's measured bounds. */
    const FAHPropKind GCover[] =
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

    const TCHAR* const GHouses[] =
    {
        TEXT("BP_BLD_house_1"),  TEXT("BP_BLD_house_2"),  TEXT("BP_BLD_house_3"),
        TEXT("BP_BLD_house_4"),  TEXT("BP_BLD_house_5"),  TEXT("BP_BLD_house_6"),
        TEXT("BP_BLD_house_7"),  TEXT("BP_BLD_house_8"),  TEXT("BP_BLD_house_9"),
        TEXT("BP_BLD_house_10"), TEXT("BP_BLD_house_11"), TEXT("BP_BLD_house_12"),
        TEXT("BP_BLD_house_13"), TEXT("BP_BLD_house_14"),
    };

    /** Four fence families, so two arenas are not fenced the same way. */
    const TCHAR* const GFences[] =
    {
        TEXT("construction/SM_PROP_fence_v01_01"),
        TEXT("construction/SM_PROP_fence_v02_01"),
        TEXT("construction/SM_PROP_fence_v03_01"),
        TEXT("construction/SM_PROP_fence_v04_01"),
        TEXT("construction/SM_PROP_wall_stone_small_01"),
        TEXT("construction/SM_PROP_wall_wood"),
    };

    const TCHAR* const GStalls[] =
    {
        TEXT("construction/SM_PROP_market_v01_01"), TEXT("construction/SM_PROP_market_v02_01"),
        TEXT("construction/SM_PROP_market_v03_01"), TEXT("construction/SM_PROP_market_v04_01"),
    };

    const TCHAR* const GBraziers[] =
    {
        TEXT("BP_PROP_brazier_01"), TEXT("BP_PROP_brazier_03"),
        TEXT("BP_PROP_brazier_04"), TEXT("BP_PROP_brazier_05"),
    };

    template<typename T, int32 N>
    const T& Pick(FRandomStream& Dice, const T (&Options)[N])
    {
        return Options[Dice.RandRange(0, N - 1)];
    }

    int32 CoverWeight()
    {
        int32 Sum = 0;
        for (const FAHPropKind& Kind : GCover) Sum += Kind.Weight;
        return Sum;
    }

    const FAHPropKind& PickCover(FRandomStream& Dice)
    {
        int32 Roll = Dice.RandRange(1, CoverWeight());
        for (const FAHPropKind& Kind : GCover)
        {
            Roll -= Kind.Weight;
            if (Roll <= 0) return Kind;
        }
        return GCover[0];
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

    /** Yaw that turns something at Where to face the middle of the arena. */
    float FacingCentre(const FVector& Where)
    {
        // Explicit: the angle comes back as a double and this hands back a float.
        return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-Where.Y, -Where.X)));
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

const TCHAR* AHArena::KindName(EAHArenaKind Kind)
{
    switch (Kind)
    {
        case EAHArenaKind::MarketStreet: return TEXT("RUA DO MERCADO");
        case EAHArenaKind::Farmstead:    return TEXT("TERREIRO");
        case EAHArenaKind::Ruins:        return TEXT("RUINAS");
        default:                         return TEXT("PRACA DA VILA");
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
        if (!Piece.bCover) continue;

        double Along = 0.0;
        const double Sideways = DistanceToSegment(Piece.Location, From, To, Along);
        if (Sideways > Piece.Radius) continue;

        // Something at your own feet is not cover you are hiding behind, and
        // something at the shooter's feet is not cover for the target either.
        const double Span = FVector::Dist2D(From, To);
        if (Along * Span < 80.0 || (1.0 - Along) * Span < 40.0) continue;

        // Measured against the TARGET's ground, so a barrel that hides a man on
        // the flagstones hides nobody standing on the raised deck above it.
        const double Rise = Piece.TopZ - To.Z;
        if (Rise >= BodyHeight * .70)                            Best = EAHCover::ThreeQuarters;
        else if (Rise >= BodyHeight * .30 && Best == EAHCover::None) Best = EAHCover::Half;
    }
    return Best;
}

FAHArenaPlan AHArena::Build(FRandomStream& Dice, const FVector& HeroSpawn, const FVector& FoeSpawn,
                            TFunctionRef<FVector(const FString&)> Measure)
{
    FAHArenaPlan Plan;
    Plan.Kind = static_cast<EAHArenaKind>(Dice.RandRange(0, static_cast<int32>(EAHArenaKind::Count) - 1));
    Plan.Name = KindName(Plan.Kind);

    // The hour of the day is part of the roll. It costs nothing next to geometry
    // and does more than geometry to stop two arenas feeling like one place.
    Plan.SunPitch       = Dice.FRandRange(-54.f, -20.f);
    Plan.SunYaw         = Dice.FRandRange(-180.f, 180.f);
    Plan.SunTemperature = Dice.FRandRange(4100.f, 7400.f);
    Plan.SkyIntensity   = Dice.FRandRange(1.2f, 2.5f);

    // ── Placement helpers ────────────────────────────────────────────────────
    auto Put = [&Plan](const FString& Path, const FVector& Where, float Yaw, float Scale,
                       bool bCover, float Radius, float TopZ)
    {
        FAHArenaPiece Piece;
        Piece.MeshPath = Path;
        Piece.Location = Where;
        Piece.Rotation = FRotator(0.f, Yaw, 0.f);
        Piece.Scale    = FVector(Scale);
        Piece.bCover   = bCover;
        Piece.Radius   = Radius;
        Piece.TopZ     = TopZ;
        Plan.Pieces.Add(Piece);
    };

    auto Sized = [&Measure](const FString& Path) { return Measure(Path) * 2.0; };

    // The lane between the two spawns is sacred: nothing an archetype builds may
    // cross it. Checked here, once, rather than in each shape -- a stone wall in
    // the ruins could otherwise seal the fight off before it began.
    auto InLane = [&](const FVector& Where, double Margin)
    {
        double Along = 0.0;
        return DistanceToSegment(Where, HeroSpawn, FoeSpawn, Along) < Margin
            || FVector::Dist2D(Where, HeroSpawn) < Margin
            || FVector::Dist2D(Where, FoeSpawn)  < Margin;
    };

    // Somewhere inside the arena that is clear of the lane and of both spawns.
    // Used for anything big -- decks, wells -- where landing on a spawn point
    // would be worse than landing somewhere slightly dull.
    auto ClearSpot = [&](float Span) -> FVector
    {
        for (int32 Try = 0; Try < 40; ++Try)
        {
            const FVector Where(Dice.FRandRange(-Span, Span), Dice.FRandRange(-Span, Span), 0.f);
            if (!InLane(Where, 620.0)) return Where;
        }
        return FVector(Span * .85f, Span * .85f, 0.f);   // a corner is always clear
    };

    // Tiling a fence without gaps or overlaps needs the panel's real length, and
    // that is a question about the asset rather than a constant worth guessing.
    auto FenceRun = [&](const FString& Path, const FVector& A, const FVector& B)
    {
        const FVector Size   = Sized(Path);
        const bool    bLongX = Size.X >= Size.Y;
        const double  Panel  = FMath::Max(bLongX ? Size.X : Size.Y, 80.0);
        const double  Length = FVector::Dist2D(A, B);
        const int32   Count  = FMath::Max(1, static_cast<int32>(FMath::RoundToInt(Length / Panel)));
        const double  Angle  = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
        const double  Yaw    = bLongX ? Angle : Angle - 90.0;
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector Where = FMath::Lerp(A, B, (I + 0.5) / Count);
            if (InLane(Where, 300.0)) continue;      // leave the lane open
            Put(Path, Where, static_cast<float>(Yaw), 1.f,
                true, static_cast<float>(FMath::Max(Size.X, Size.Y) * .5), static_cast<float>(Size.Z));
        }
    };

    // Decks stay axis-aligned. Variety comes from where and how big they are;
    // turning them as well would only make the ramp maths easier to get wrong,
    // and a ramp the navmesh refuses is worth less than a ramp facing north.
    TArray<FVector> DeckKeepOut;   // X,Y centre and the radius to stay clear of
    auto Deck = [&](const FVector& Centre, float HalfX, float HalfY)
    {
        FAHArenaPiece Slab;
        Slab.MeshPath     = CUBE;
        Slab.Location     = FVector(Centre.X, Centre.Y, DeckTop * .5f);
        Slab.Scale        = FVector(HalfX * 2.f / 100.f, HalfY * 2.f / 100.f, DeckTop / 100.f);
        Slab.Radius       = FMath::Max(HalfX, HalfY);
        Slab.TopZ         = DeckTop;
        Slab.bSitOnGround = false;
        Slab.MaterialPath = TEXT("/Game/Fantastic_Village_Pack/materials/MI_stonebrick_01");
        Plan.Pieces.Add(Slab);

        // The ramp always climbs from the middle of the arena, so the deck can
        // never be stranded behind itself.
        const FVector Inward = (-Centre).GetSafeNormal2D();
        const double  Reach  = FMath::Abs(Inward.X) * HalfX + FMath::Abs(Inward.Y) * HalfY;
        const double  Run    = 400.0;
        const FVector Low    = Centre + Inward * (Reach + Run);
        const FVector High   = Centre + Inward * Reach;

        FAHArenaPiece Ramp;
        Ramp.MeshPath     = CUBE;
        Ramp.Scale        = FVector(FMath::Sqrt(Run * Run + DeckTop * DeckTop) / 100.0, 3.0, .34);
        Ramp.Rotation     = FRotator(FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(DeckTop), Run)),
                                     FMath::RadiansToDegrees(FMath::Atan2(-Inward.Y, -Inward.X)), 0.f);
        // Sunk a little so it overlaps floor and deck instead of leaving a lip.
        Ramp.Location     = FVector((Low.X + High.X) * .5, (Low.Y + High.Y) * .5, DeckTop * .5 - 22.0);
        Ramp.bCover       = false;      // walkable, and too low to hide behind
        Ramp.bSitOnGround = false;
        Ramp.MaterialPath = TEXT("/Game/Fantastic_Village_Pack/materials/MI_stonebrick_01");
        Plan.Pieces.Add(Ramp);

        DeckKeepOut.Add(FVector(Centre.X, Centre.Y, FMath::Max(HalfX, HalfY) + Run + 220.0));
    };

    auto Houses = [&](int32 Count, float Ring, float FromAngle, float ToAngle)
    {
        for (int32 I = 0; I < Count; ++I)
        {
            const float Angle = FMath::DegreesToRadians(
                FMath::Lerp(FromAngle, ToAngle, Count > 1 ? (float)I / (Count - 1) : .5f)
                + Dice.FRandRange(-9.f, 9.f));
            const FVector Where(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 0.f);
            // Houses sit well outside the fight, so they are scenery: whether a
            // shot is blocked by one is the line-of-sight trace's business.
            Put(FString(BPBLD) + Pick(Dice, GHouses), Where,
                FacingCentre(Where) + Dice.FRandRange(-12.f, 12.f), 1.f, false, 0.f, 0.f);
        }
    };

    auto Brazier = [&](const FVector& Where)
    {
        FAHArenaPiece Fire;
        Fire.MeshPath = FString(BPROP) + Pick(Dice, GBraziers);
        Fire.Location = Where;
        Fire.bCover   = false;
        Fire.bLight   = true;
        Plan.Pieces.Add(Fire);
    };

    const float Edge  = PlayHalfSize - 220.f;
    const float Ring  = PlayHalfSize + 480.f;
    const float Fence = PlayHalfSize + 120.f;
    const FString FenceKind = FString(PROP) + Pick(Dice, GFences);

    // ── The four shapes ──────────────────────────────────────────────────────
    switch (Plan.Kind)
    {
    case EAHArenaKind::MarketStreet:
    {
        // A street: buildings down both long sides, open at the ends, stalls in
        // two rows with the fighting lane between them.
        Houses(Dice.RandRange(3, 4), Ring,  40.f, 140.f);
        Houses(Dice.RandRange(3, 4), Ring, 220.f, 320.f);
        FenceRun(FenceKind, FVector(-Fence, -Fence, 0), FVector(-Fence, Fence, 0));
        FenceRun(FenceKind, FVector( Fence, -Fence, 0), FVector( Fence, Fence, 0));
        for (int32 Side = -1; Side <= 1; Side += 2)
            for (int32 I = 0; I < Dice.RandRange(2, 4); ++I)
            {
                const FString Stall = FString(PROP) + Pick(Dice, GStalls);
                const FVector Where(Dice.FRandRange(-Edge * .8f, Edge * .8f), Side * Dice.FRandRange(620.f, 1000.f), 0.f);
                // A stall abreast of a spawn would sit right in the lane.
                if (InLane(Where, 300.0)) continue;
                const FVector Size = Sized(Stall);
                Put(Stall, Where, Side > 0 ? 180.f : 0.f, 1.f, true,
                    static_cast<float>(FMath::Max(Size.X, Size.Y) * .5), static_cast<float>(Size.Z));
            }
        Put(FString(BPROP) + TEXT("BP_PROP_cart_01"), FVector(-820.f, -260.f, 0.f), Dice.FRandRange(0.f, 360.f), 1.f, true, 120.f, 160.f);
        Deck(ClearSpot(950.f), 240.f, 330.f);
        for (int32 I = 0; I < 4; ++I)
            Brazier(FVector(Dice.FRandRange(-Edge, Edge), (I % 2 ? 1 : -1) * (PlayHalfSize - 90.f), 0.f));
        break;
    }
    case EAHArenaKind::Farmstead:
    {
        // Open ground with a steading on one side, fences cutting across the
        // outfield and hay everywhere.
        const float Face = Dice.FRandRange(0.f, 360.f);
        Houses(Dice.RandRange(2, 3), Ring, Face - 35.f, Face + 35.f);
        for (int32 I = 0; I < 4; ++I)
        {
            const float A0 = Dice.FRandRange(0.f, 360.f), Len = Dice.FRandRange(500.f, 1100.f);
            const FVector From(FMath::Cos(FMath::DegreesToRadians(A0)) * Fence,
                               FMath::Sin(FMath::DegreesToRadians(A0)) * Fence, 0.f);
            FenceRun(FenceKind, From, From + FVector(Dice.FRandRange(-Len, Len), Dice.FRandRange(-Len, Len), 0.f));
        }
        for (int32 I = 0; I < Dice.RandRange(4, 7); ++I)
            Put(FString(ENV) + TEXT("SM_ENV_TREE_village_LOD0"),
                FVector(Dice.FRandRange(-2600.f, 2600.f), Dice.FRandRange(-2600.f, 2600.f), 0.f),
                Dice.FRandRange(0.f, 360.f), Dice.FRandRange(.85f, 1.25f), false, 0.f, 0.f);
        Put(FString(BPROP) + TEXT("BP_PROP_well"), ClearSpot(900.f), 0.f, 1.f, true, 110.f, 140.f);
        Deck(ClearSpot(950.f), 330.f, 250.f);
        Brazier(ClearSpot(900.f));
        break;
    }
    case EAHArenaKind::Ruins:
    {
        // Broken walls instead of a village: more stone, more gaps, two low
        // platforms of rubble and no fence to speak of.
        Houses(Dice.RandRange(1, 2), Ring + 300.f, Dice.FRandRange(0.f, 360.f), Dice.FRandRange(0.f, 360.f));
        const FString Stone = FString(PROP) + TEXT("construction/SM_PROP_wall_stone_small_01");
        for (int32 I = 0; I < Dice.RandRange(4, 6); ++I)
        {
            const FVector From(Dice.FRandRange(-Edge, Edge), Dice.FRandRange(-Edge, Edge), 0.f);
            FenceRun(Stone, From, From + FVector(Dice.FRandRange(-600.f, 600.f), Dice.FRandRange(-600.f, 600.f), 0.f));
        }
        for (int32 I = 0; I < Dice.RandRange(5, 9); ++I)
        {
            const FVector Rubble(Dice.FRandRange(-Edge, Edge), Dice.FRandRange(-Edge, Edge), 0.f);
            if (InLane(Rubble, 300.0)) continue;
            Put(FString(PROP) + TEXT("natural/SM_PROP_stone_02"), Rubble,
                Dice.FRandRange(0.f, 360.f), Dice.FRandRange(.8f, 1.4f), true, 100.f, 150.f);
        }
        Deck(ClearSpot(950.f), 260.f, 260.f);
        for (int32 I = 0; I < 3; ++I) Brazier(ClearSpot(Edge));
        break;
    }
    default:
    {
        // The square: enclosed on all four sides, braziers at the corners.
        Houses(Dice.RandRange(6, 8), Ring, 0.f, 300.f);
        FenceRun(FenceKind, FVector(-Fence, -Fence, 0), FVector( Fence, -Fence, 0));
        FenceRun(FenceKind, FVector(-Fence,  Fence, 0), FVector( Fence,  Fence, 0));
        FenceRun(FenceKind, FVector(-Fence, -Fence, 0), FVector(-Fence,  Fence, 0));
        FenceRun(FenceKind, FVector( Fence, -Fence, 0), FVector( Fence,  Fence, 0));
        for (int32 SX = -1; SX <= 1; SX += 2)
            for (int32 SY = -1; SY <= 1; SY += 2)
                Brazier(FVector(SX * (PlayHalfSize - 80.f), SY * (PlayHalfSize - 80.f), 0.f));
        Put(FString(BPROP) + TEXT("BP_PROP_well"), ClearSpot(800.f), 0.f, 1.f, true, 110.f, 140.f);
        Deck(ClearSpot(980.f), 260.f, 380.f);
        break;
    }
    }

    // ── Loose cover, common to every shape ───────────────────────────────────
    const float Corridor = 300.f;
    const int32 Wanted   = Dice.RandRange(9, 15);
    int32 Placed = 0;
    for (int32 Attempt = 0; Attempt < Wanted * 16 && Placed < Wanted; ++Attempt)
    {
        const FVector Where(Dice.FRandRange(-Edge, Edge), Dice.FRandRange(-Edge, Edge), 0.f);

        if (InLane(Where, Corridor)) continue;

        bool bBlocked = false;
        for (const FVector& Keep : DeckKeepOut)
            if (FVector::Dist2D(Where, FVector(Keep.X, Keep.Y, 0.f)) < Keep.Z) { bBlocked = true; break; }
        if (bBlocked) continue;

        const FAHPropKind& Kind  = PickCover(Dice);
        const float        Scale = Dice.FRandRange(.9f, 1.15f);
        const float        Reach = Kind.Radius * Scale;
        for (const FAHArenaPiece& Other : Plan.Pieces)
            if (Other.bCover && FVector::Dist2D(Where, Other.Location) < Other.Radius + Reach + 170.f)
            { bBlocked = true; break; }
        if (bBlocked) continue;

        Put(FString(PROP) + Kind.Path, Where, Dice.FRandRange(0.f, 360.f), Scale,
            true, Reach, Kind.Height * Scale);
        ++Placed;
    }
    return Plan;
}
