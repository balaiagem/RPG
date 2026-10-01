#include "Misc/AutomationTest.h"
#include "AHArena.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    /**
     * TestEqual wants a type it can print in a failure message, and a scoped enum
     * is not one, so cover is compared as its underlying number.
     */
    int32 AsNumber(EAHCover Cover)   { return static_cast<int32>(Cover); }

    /** A knee-high crate: enough to be half cover, nowhere near three quarters. */
    FAHArenaPiece Block(float X, float Y, float Height, float Radius = 70.f, float Z = 0.f)
    {
        FAHArenaPiece Piece;
        Piece.MeshPath = TEXT("/Engine/BasicShapes/Cube");
        Piece.Location = FVector(X, Y, Z);
        Piece.Radius   = Radius;
        Piece.TopZ     = Height;
        return Piece;
    }

    /** A bridge is road with a river under it, and it has to walk like road. */
    bool Paved(const FAHArenaPlan& Plan, int32 Cx, int32 Cy)
    {
        const EAHCell Kind = Plan.At(Cx, Cy);
        return Kind == EAHCell::Road || Kind == EAHCell::Plaza || Kind == EAHCell::Bridge;
    }

    /**
     * How many paved cells you can walk to from the arrival cell.
     *
     * The point of the whole rewrite is that the valley has roads you can
     * follow, so "is every stretch of road joined to the one you arrive on"
     * is the invariant worth spending a breadth-first search on. A road that
     * starts in the middle of a field and goes nowhere is the sort of thing
     * that looks generated, and it is invisible from any single screenshot.
     */
    int32 WalkableFrom(const FAHArenaPlan& Plan, int32 StartX, int32 StartY)
    {
        const int32 Side = AHArena::GridSide;
        TArray<bool> Seen;
        Seen.Init(false, Side * Side);
        if (!Paved(Plan, StartX, StartY)) return 0;

        TArray<int32> Edge;
        Edge.Add(StartX * Side + StartY);
        Seen[Edge[0]] = true;
        int32 Reached = 0;
        const int32 StepX[4] = { 1, 0, -1, 0 };
        const int32 StepY[4] = { 0, 1, 0, -1 };
        while (Edge.Num() > 0)
        {
            const int32 Key = Edge.Pop();
            ++Reached;
            for (int32 Dir = 0; Dir < 4; ++Dir)
            {
                const int32 Nx = Key / Side + StepX[Dir];
                const int32 Ny = Key % Side + StepY[Dir];
                if (!AHArena::InGrid(Nx, Ny) || !Paved(Plan, Nx, Ny)) continue;
                if (Seen[Nx * Side + Ny]) continue;
                Seen[Nx * Side + Ny] = true;
                Edge.Add(Nx * Side + Ny);
            }
        }
        return Reached;
    }

    /** Reachability from the default arrival cell, for the plain layout test. */
    int32 Walkable(const FAHArenaPlan& Plan)
    { return WalkableFrom(Plan, AHArena::SpawnCellX, AHArena::SpawnCellY); }

    /**
     * A stand-in Landscape.
     *
     * The test has no world to trace, so it supplies its own terrain: broad
     * hills with a level shelf at each of the six sites, which is the shape
     * the real heightmap has. The offline harness reads the actual heightmap
     * instead; this one only has to be terrain-like enough for the placement
     * rules to be worth testing.
     */
    float FakeGround(float X, float Y)
    {
        for (int32 Which = 0; Which < AHArena::SiteCount; ++Which)
        {
            const AHArena::FAHSite& Site = AHArena::Site(Which);
            if (FVector::Dist2D(FVector(X, Y, 0.f), FVector(Site.X, Site.Y, 0.f)) < 3500.f)
                return 1200.f + Which * 700.f;
        }
        return 1200.f + 2600.f * FMath::Sin(X / 9000.f) * FMath::Cos(Y / 11000.f)
                      + 900.f  * FMath::Sin(X / 3100.f + Y / 2700.f);
    }

    int32 Count(const FAHArenaPlan& Plan, EAHCell Kind)
    {
        int32 Many = 0;
        for (const EAHCell Cell : Plan.Cells) if (Cell == Kind) ++Many;
        return Many;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArenaCoverTest, "AshenHollow.Rules.Cover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAHArenaCoverTest::RunTest(const FString&)
{
    const FVector Shooter(0.f, -600.f, 0.f);
    const FVector Target (0.f,  600.f, 0.f);

    TestEqual(TEXT("Nothing in the way is no cover"),
        AsNumber(AHArena::CoverBetween({}, Shooter, Target)), AsNumber(EAHCover::None));

    // 60 cm of crate against a 180 cm body: over a third, under three quarters.
    TestEqual(TEXT("A low crate between the two is half cover"),
        AsNumber(AHArena::CoverBetween({ Block(0.f, 200.f, 60.f) }, Shooter, Target)), AsNumber(EAHCover::Half));
    TestEqual(TEXT("Half cover is worth +2 AC"), AHArena::ArmorBonus(EAHCover::Half), 2);

    TestEqual(TEXT("A tall cart between the two is three-quarters cover"),
        AsNumber(AHArena::CoverBetween({ Block(0.f, 200.f, 150.f) }, Shooter, Target)), AsNumber(EAHCover::ThreeQuarters));
    TestEqual(TEXT("Three-quarters cover is worth +5 AC"),
        AHArena::ArmorBonus(EAHCover::ThreeQuarters), 5);

    // The rule is about the line of fire, not about mere proximity.
    TestEqual(TEXT("A crate well off to the side covers nobody"),
        AsNumber(AHArena::CoverBetween({ Block(900.f, 200.f, 150.f) }, Shooter, Target)), AsNumber(EAHCover::None));
    TestEqual(TEXT("A crate behind the target covers nobody"),
        AsNumber(AHArena::CoverBetween({ Block(0.f, 900.f, 150.f) }, Shooter, Target)), AsNumber(EAHCover::None));

    // Paving is painted on the ground and never hides anyone, whatever its size.
    {
        FAHArenaPiece Road = Block(0.f, 200.f, 150.f, 400.f);
        Road.bCover = false;
        Road.bFlat  = true;
        TestEqual(TEXT("A road across the line of fire is not cover"),
            AsNumber(AHArena::CoverBetween({ Road }, Shooter, Target)), AsNumber(EAHCover::None));
    }

    // Height is measured against the target's own ground: the same crate that
    // hides a man on the flagstones hides nobody standing on the raised deck.
    const FVector Raised(0.f, 600.f, AHArena::DeckTop);
    TestEqual(TEXT("A crate on the floor does not cover someone up on the deck"),
        AsNumber(AHArena::CoverBetween({ Block(0.f, 200.f, 60.f) }, Shooter, Raised)), AsNumber(EAHCover::None));

    TestTrue (TEXT("The deck is high ground over the floor"),
        AHArena::HasHighGround(Raised, Target));
    TestFalse(TEXT("Flat ground is nobody's high ground"),
        AHArena::HasHighGround(Shooter, Target));
    TestFalse(TEXT("Being below is not high ground"),
        AHArena::HasHighGround(Target, Raised));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArenaGridTest, "AshenHollow.Rules.ArenaGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAHArenaGridTest::RunTest(const FString&)
{
    const FVector Home(AHArena::SpawnX, AHArena::SpawnY, 110.f);
    // The generator asks how big each mesh is; here every mesh is 2 m of
    // everything, which is enough for the layout rules under test.
    auto Measure = [](const FString&) { return FVector(100.0, 100.0, 100.0); };

    TestEqual(TEXT("The arrival point is the first of the levelled sites"),
        Home, FVector(AHArena::Site(0).X, AHArena::Site(0).Y, 110.f));

    FRandomStream First(1234), Again(1234);
    const FAHArenaPlan A = AHArena::Build(First, Home, Measure, FakeGround);
    const FAHArenaPlan B = AHArena::Build(Again, Home, Measure, FakeGround);

    TestEqual(TEXT("A valley is a full grid of cells"), A.Cells.Num(), AHArena::GridSide * AHArena::GridSide);
    TestTrue (TEXT("A seed builds a world with something in it"), A.Pieces.Num() > 0);
    TestTrue (TEXT("A world has somebody living in it"), A.Camps.Num() > 0);
    TestEqual(TEXT("The same seed builds the same world twice"), A.Pieces.Num(), B.Pieces.Num());
    TestEqual(TEXT("...with the same camps"), A.Camps.Num(), B.Camps.Num());
    TestTrue (TEXT("...and the same ground under them"), A.Cells == B.Cells);
    int32 Disagreed = 0;
    for (int32 I = 0; I < A.Pieces.Num() && I < B.Pieces.Num(); ++I)
        if (!A.Pieces[I].Location.Equals(B.Pieces[I].Location)
            || A.Pieces[I].MeshPath != B.Pieces[I].MeshPath) ++Disagreed;
    TestEqual(TEXT("Piece by piece, the same seed agrees with itself"), Disagreed, 0);
    TestTrue (TEXT("...and the ground comes out the same too"), A.Heights == B.Heights);

    // Over many seeds, the rules that make a valley playable rather than merely
    // populated. Three hundred worlds is a second of test time and it is the
    // only way a one-in-fifty layout bug is ever going to be caught.
    for (int32 Seed = 0; Seed < 30; ++Seed)
    {
        FRandomStream Dice(Seed * 7919 + 13);
        const FAHArenaPlan World = AHArena::Build(Dice, Home, Measure, FakeGround);

        TestTrue(TEXT("Every world has camps"), World.Camps.Num() > 0);
        // The ground is not decoration and is not budgeted: the budget caps
        // what the valley is dressed with, and then the valley is given a floor.
        TestTrue(TEXT("Every world stays inside its budget"),
                 World.Pieces.Num() < AHArena::PieceBudget + 4 * AHArena::GridSide * 3
                                    + AHArena::HeightSide * AHArena::HeightSide);

        // You arrive on the road, and every road in the valley is joined to it.
        TestTrue(TEXT("The player arrives standing on the road"),
                 Paved(World, AHArena::SpawnCellX, AHArena::SpawnCellY));
        const int32 Roads = Count(World, EAHCell::Road) + Count(World, EAHCell::Plaza)
                          + Count(World, EAHCell::Bridge);
        TestEqual(TEXT("Every stretch of road can be walked to from the arrival cell"),
                  Walkable(World), Roads);
        TestTrue(TEXT("There is enough road to be worth following"), Roads >= 6);

        // A house with no street in front of it is the blob of buildings the
        // grid was written to get rid of.
        int32 OffStreet = 0;
        for (int32 Cx = 0; Cx < AHArena::GridSide; ++Cx)
            for (int32 Cy = 0; Cy < AHArena::GridSide; ++Cy)
            {
                if (World.At(Cx, Cy) != EAHCell::Village) continue;
                if (!Paved(World, Cx + 1, Cy) && !Paved(World, Cx - 1, Cy)
                    && !Paved(World, Cx, Cy + 1) && !Paved(World, Cx, Cy - 1)) ++OffStreet;
            }
        TestEqual(TEXT("Every house stands on a street"), OffStreet, 0);

        int32 OnSpawn = 0, Cramped = 0;
        for (const FAHArenaPiece& Piece : World.Pieces)
        {
            if (!Piece.bCover) continue;
            const FVector Flat(Piece.Location.X, Piece.Location.Y, 0.f);
            if (FVector::Dist2D(Flat, Home) < AHArena::SpawnClearance) ++OnSpawn;
            for (const FAHCampSpot& Camp : World.Camps)
            {
                // Indoors the walls are meant to be close -- a dungeon room is
                // four metres across and its walls are the design. Outdoors a
                // fight needs six and a half metres of clear ground or the
                // camp is buried in whatever arrived after it.
                const double Keep = Camp.bIndoor ? 150.0 : AHArena::CampClearance;
                if (FVector::Dist2D(Flat, Camp.Where) < Keep) ++Cramped;
            }
        }
        // Every dungeon has to be worth entering: three rooms means an
        // entrance, a fight, and something at the end.
        for (const FAHLandmark& Mark : World.Landmarks)
        {
            if (Mark.Kind != EAHSite::Masmorra) continue;
            int32 Fights = 0;
            for (const FAHCampSpot& Camp : World.Camps)
                if (Camp.bIndoor && FVector::Dist2D(Camp.Where, Mark.Where) < 2500.0) ++Fights;
            TestTrue(TEXT("Every dungeon holds at least two fights"), Fights >= 2);
        }
        TestEqual(TEXT("Nothing solid lands on the arrival point"), OnSpawn, 0);
        TestEqual(TEXT("Every camp keeps room to fight in"), Cramped, 0);
        // -- The ground ---------------------------------------------------
        // What used to be checked here was a step limit on a height field the
        // generator invented and kept walkable by construction. The Landscape
        // has cliffs on purpose; the rule that matters is that nothing is
        // BUILT on one.
        {
            TestEqual(TEXT("The ground is a full field of samples"),
                      World.Heights.Num(), AHArena::HeightSide * AHArena::HeightSide);
            TestTrue(TEXT("The player arrives on ground the terrain actually has"),
                     FMath::Abs(World.GroundAt(Home) - FakeGround(
                         static_cast<float>(Home.X), static_cast<float>(Home.Y))) < 400.f);
        }

        // -- Nothing inside anything else, nothing in the carriageway -------
        // Counted and asserted ONCE per world rather than once per pair. A
        // valley holds well over a thousand solid things, and a TestTrue for
        // every pair of them is a hundred million calls into the automation
        // framework -- a check so expensive nobody would leave it switched on
        // is a check that is not really there.
        {
            TArray<const FAHArenaPiece*> Solid;
            int32 InTheRoad = 0, Stacked = 0, OnASlope = 0;
            for (const FAHArenaPiece& Piece : World.Pieces)
            {
                if (Piece.bFlat || Piece.bInvisible || Piece.bSmoke) continue;
                if (Piece.Radius <= 0.f) continue;
                if (!Piece.MaterialPath.IsEmpty()) continue;            // a ground tile
                // Fences, ruin walls and bridge spans tile against themselves
                // by design; everything else must keep its distance.
                if (Piece.MeshPath.Contains(TEXT("fence"))) continue;
                if (Piece.MeshPath.Contains(TEXT("wall_stone"))) continue;
                if (Piece.MeshPath.Contains(TEXT("bridge"))) continue;
                Solid.Add(&Piece);
                // On a hillside: the one failure the player reads as a bug
                // without being able to say why.
                {
                    const float Under = World.GroundAt(Piece.Location);
                    float Tilt = 0.f;
                    for (int32 Dir = 0; Dir < 4; ++Dir)
                    {
                        const float Dx = (Dir == 0) - (Dir == 2), Dy = (Dir == 1) - (Dir == 3);
                        Tilt = FMath::Max(Tilt, FMath::Abs(World.GroundAt(
                            Piece.Location + FVector(Dx * 260.f, Dy * 260.f, 0.f)) - Under));
                    }
                    if (Tilt > AHArena::BuildableRise) ++OnASlope;
                }

                int32 Px = 0, Py = 0;
                AHArena::CellOf(Piece.Location, Px, Py);
                for (int32 Ox = Px - 1; Ox <= Px + 1; ++Ox)
                    for (int32 Oy = Py - 1; Oy <= Py + 1; ++Oy)
                    {
                        if (!AHArena::InGrid(Ox, Oy)) continue;
                        if (World.At(Ox, Oy) != EAHCell::Road) continue;
                        if (FVector::Dist2D(Piece.Location, AHArena::CellCentre(Ox, Oy))
                            < AHArena::RoadWidth * .5f + FMath::Min(Piece.Radius, 400.f)) ++InTheRoad;
                    }
            }
            // Swept along X, so each piece is only compared with the handful
            // that are near enough in one axis to possibly touch it.
            Solid.StableSort([](const FAHArenaPiece& A, const FAHArenaPiece& B)
                             { return A.Location.X < B.Location.X; });
            for (int32 I = 0; I < Solid.Num(); ++I)
                for (int32 J = I + 1; J < Solid.Num(); ++J)
                {
                    if (Solid[J]->Location.X - Solid[I]->Location.X > 260.0) break;
                    const float Want = FMath::Min(Solid[I]->Radius, 130.f)
                                     + FMath::Min(Solid[J]->Radius, 130.f) - 30.f;
                    if (Want <= 0.f) continue;
                    if (FVector::Dist2D(Solid[I]->Location, Solid[J]->Location) < Want) ++Stacked;
                }
            TestEqual(TEXT("Nothing stands on a hillside"), OnASlope, 0);
            TestEqual(TEXT("Nothing stands in the carriageway"), InTheRoad, 0);
            TestEqual(TEXT("Nothing stands inside anything else"), Stacked, 0);
        }

        // -- The river ---------------------------------------------------
        // Four rules, and every one of them fails silently in the editor: you
        // only find out by walking into a wall that is not there, or by not
        // being able to cross a bridge, or by an arrow stopping over water.
        {
            TArray<bool> Walled;
            int32 DryBlocker = 0, ThingInWater = 0, OpenWater = 0;
            Walled.Init(false, AHArena::GridSide * AHArena::GridSide);
            for (const FAHArenaPiece& Piece : World.Pieces)
            {
                int32 Px = 0, Py = 0;
                AHArena::CellOf(Piece.Location, Px, Py);
                if (!AHArena::InGrid(Px, Py)) continue;
                const EAHCell Under = World.At(Px, Py);
                if (Piece.bInvisible)
                {
                    Walled[Px * AHArena::GridSide + Py] = true;
                    if (Under != EAHCell::Water) ++DryBlocker;
                }
                else if (Piece.bCover && Under == EAHCell::Water) ++ThingInWater;
            }
            for (int32 Cx = 0; Cx < AHArena::GridSide; ++Cx)
                for (int32 Cy = 0; Cy < AHArena::GridSide; ++Cy)
                    if (World.At(Cx, Cy) == EAHCell::Water
                        && !Walled[Cx * AHArena::GridSide + Cy]) ++OpenWater;
            TestEqual(TEXT("An invisible blocker only ever stands in the river"), DryBlocker, 0);
            TestEqual(TEXT("Nothing solid stands in the river"), ThingInWater, 0);
            TestEqual(TEXT("Every stretch of river keeps you out of it"), OpenWater, 0);
        }
        for (const FAHCampSpot& Camp : World.Camps)
        {
            int32 Kx = 0, Ky = 0;
            AHArena::CellOf(Camp.Where, Kx, Ky);
            TestTrue(TEXT("Nobody camps in the water"),
                     World.At(Kx, Ky) != EAHCell::Water && World.At(Kx, Ky) != EAHCell::Bridge);
        }
        for (const FAHCampSpot& Camp : World.Camps)
            TestTrue(TEXT("Camps stand inside the world"),
                     FMath::Abs(Camp.Where.X) < AHArena::PlayHalfSize
                  && FMath::Abs(Camp.Where.Y) < AHArena::PlayHalfSize);
    }

    // The gates went with the comarcas. There is no neighbouring region to walk
    // into any more -- the world is one island of a kilometre, bounded by the
    // mountains the heightmap put round it -- so the game asks for no gates and
    // there is nothing left here to test. The machinery stays in the generator
    // because it costs nothing switched off, and because a second island one
    // day is not a silly idea.

    FRandomStream Other(98765);
    const FAHArenaPlan C = AHArena::Build(Other, Home, Measure, FakeGround);
    bool bDiffers = C.Cells != A.Cells || C.Camps.Num() != A.Camps.Num();
    for (int32 I = 0; !bDiffers && I < A.Pieces.Num() && I < C.Pieces.Num(); ++I)
        bDiffers = !A.Pieces[I].Location.Equals(C.Pieces[I].Location);
    TestTrue(TEXT("A different seed builds a different world"), bDiffers);
    return true;
}

#endif
