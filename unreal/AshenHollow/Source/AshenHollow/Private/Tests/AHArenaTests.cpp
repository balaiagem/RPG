#include "Misc/AutomationTest.h"
#include "AHArena.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    /**
     * TestEqual wants a type it can print in a failure message, and a scoped enum
     * is not one, so cover is compared as its underlying number.
     */
    int32 AsNumber(EAHCover Cover) { return static_cast<int32>(Cover); }

    /** A knee-high crate: enough to be half cover, nowhere near three quarters. */
    FAHArenaPiece Block(float X, float Y, float Height, float Radius = 70.f, float Z = 0.f)
    {
        FAHArenaPiece Piece;
        Piece.MeshPath = TEXT("/Engine/BasicShapes/Cube");
        Piece.Location = FVector(X, Y, Z);
        Piece.Radius   = Radius;
        Piece.Height   = Height;
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

    FRandomStream First(1234);
    FRandomStream Again(1234);
    const TArray<FAHArenaPiece> A = AHArena::Generate(First, Hero, Foe);
    const TArray<FAHArenaPiece> B = AHArena::Generate(Again, Hero, Foe);

    TestTrue(TEXT("A seed produces obstacles at all"), A.Num() > 0);
    TestEqual(TEXT("The same seed builds the same arena twice"), A.Num(), B.Num());
    for (int32 I = 0; I < A.Num() && I < B.Num(); ++I)
        TestTrue(TEXT("Piece by piece, the same seed agrees with itself"),
                 A[I].Location.Equals(B[I].Location) && A[I].MeshPath == B[I].MeshPath);

    FRandomStream Other(98765);
    const TArray<FAHArenaPiece> C = AHArena::Generate(Other, Hero, Foe);
    bool bDiffers = C.Num() != A.Num();
    for (int32 I = 0; !bDiffers && I < A.Num(); ++I)
        bDiffers = !A[I].Location.Equals(C[I].Location);
    TestTrue(TEXT("A different seed builds a different arena"), bDiffers);

    // The whole point of the corridor rule: a bad roll must never be able to
    // wall the fight off before the two sides have seen each other.
    for (const FAHArenaPiece& Piece : A)
    {
        TestTrue(TEXT("Nothing spawns outside the playable square"),
            FMath::Abs(Piece.Location.X) < AHArena::PlayHalfSize
         && FMath::Abs(Piece.Location.Y) < AHArena::PlayHalfSize);
        TestTrue(TEXT("Nothing spawns on top of either side"),
            FVector::Dist2D(Piece.Location, Hero) >= 400.f
         && FVector::Dist2D(Piece.Location, Foe)  >= 400.f);
        // Distance from the straight line between the spawns, which runs on X=0
        // between the two Y values.
        const bool bBeside = Piece.Location.Y < Hero.Y || Piece.Location.Y > Foe.Y;
        TestTrue(TEXT("The corridor between the spawns stays clear"),
            bBeside || FMath::Abs(Piece.Location.X) >= 300.f);
        TestTrue(TEXT("Nothing is buried in the raised deck"),
            FMath::Abs(Piece.Location.X - AHArena::DeckCentreX) >= AHArena::DeckKeepOutX
         || FMath::Abs(Piece.Location.Y) >= AHArena::DeckKeepOutY);
    }
    return true;
}

#endif
