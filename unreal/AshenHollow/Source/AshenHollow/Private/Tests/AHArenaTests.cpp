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
    int32 AsNumber(EAHArenaKind Kind){ return static_cast<int32>(Kind); }

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArenaLayoutTest, "AshenHollow.Rules.ArenaLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAHArenaLayoutTest::RunTest(const FString&)
{
    const FVector Hero(0.f, -500.f, 110.f);
    const FVector Foe (0.f,  500.f, 110.f);
    // The generator asks how big each mesh is; here every mesh is 2 m of
    // everything, which is enough for the layout rules under test.
    auto Measure = [](const FString&) { return FVector(100.0, 100.0, 100.0); };

    FRandomStream First(1234), Again(1234);
    const FAHArenaPlan A = AHArena::Build(First, Hero, Foe, Measure);
    const FAHArenaPlan B = AHArena::Build(Again, Hero, Foe, Measure);

    TestTrue (TEXT("A seed builds an arena with something in it"), A.Pieces.Num() > 0);
    TestEqual(TEXT("The same seed builds the same arena twice"), A.Pieces.Num(), B.Pieces.Num());
    TestEqual(TEXT("...and the same kind of arena"), AsNumber(A.Kind), AsNumber(B.Kind));
    for (int32 I = 0; I < A.Pieces.Num() && I < B.Pieces.Num(); ++I)
        TestTrue(TEXT("Piece by piece, the same seed agrees with itself"),
                 A.Pieces[I].Location.Equals(B.Pieces[I].Location)
              && A.Pieces[I].MeshPath == B.Pieces[I].MeshPath);

    // Over many seeds: the lane between the two spawns must always stay open,
    // whichever shape was rolled. One sealed arena is an unplayable encounter.
    TSet<int32> KindsSeen;
    for (int32 Seed = 0; Seed < 400; ++Seed)
    {
        FRandomStream Dice(Seed * 7919 + 13);
        const FAHArenaPlan Rolled = AHArena::Build(Dice, Hero, Foe, Measure);
        KindsSeen.Add(AsNumber(Rolled.Kind));
        TestTrue(TEXT("Every arena has pieces"), Rolled.Pieces.Num() > 0);
        for (const FAHArenaPiece& Piece : Rolled.Pieces)
        {
            if (!Piece.bCover) continue;
            const FVector Flat(Piece.Location.X, Piece.Location.Y, 0.f);
            const bool bBeside = Flat.Y < Hero.Y || Flat.Y > Foe.Y;
            TestTrue(TEXT("Nothing solid stands in the lane between the spawns"),
                bBeside || FMath::Abs(Flat.X) >= 250.f);
            TestTrue(TEXT("Nothing solid stands on a spawn point"),
                FVector::Dist2D(Flat, Hero) >= 250.f && FVector::Dist2D(Flat, Foe) >= 250.f);
        }
    }
    TestTrue(TEXT("Every arena shape gets rolled over 400 seeds"),
             KindsSeen.Num() == static_cast<int32>(EAHArenaKind::Count));

    FRandomStream Other(98765);
    const FAHArenaPlan C = AHArena::Build(Other, Hero, Foe, Measure);
    bool bDiffers = C.Pieces.Num() != A.Pieces.Num() || C.Kind != A.Kind;
    for (int32 I = 0; !bDiffers && I < A.Pieces.Num(); ++I)
        bDiffers = !A.Pieces[I].Location.Equals(C.Pieces[I].Location);
    TestTrue(TEXT("A different seed builds a different arena"), bDiffers);
    return true;
}

#endif
