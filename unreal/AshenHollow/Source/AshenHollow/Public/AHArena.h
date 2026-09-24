#pragma once

#include "CoreMinimal.h"

/**
 * The arena's obstacles, and the cover they give.
 *
 * Two jobs live here. One is the layout: where the barrels, carts and hay bales
 * stand for a given encounter, drawn from a seed so the same seed always builds
 * the same arena and a test can assert against it. The other is the rule: how
 * much an obstacle standing between a shooter and a target is worth.
 *
 * The rule reads the obstacle list rather than tracing the physics world, for one
 * practical reason -- automation tests have no props in their world, and a rule
 * that can only be checked by playing is a rule nobody checks.
 */

/** SRD 5.1 degrees of cover. Total cover cannot be targeted at all. */
enum class EAHCover : uint8 { None, Half, ThreeQuarters, Total };

/**
 * One placed obstacle.
 *
 * Radius and Height start as the generator's estimate and are overwritten with
 * the mesh's measured bounds once it exists in the world, so the cover maths runs
 * against the prop's real size and not against a number someone typed.
 */
struct FAHArenaPiece
{
    FString      MeshPath;
    FVector      Location = FVector::ZeroVector;
    float        Yaw      = 0.f;
    float        Scale    = 1.f;
    float        Radius   = 60.f;      // horizontal footprint, centimetres
    float        Height   = 90.f;      // above the floor it stands on
};

namespace AHArena
{
    /** Half-width of the playable square, in centimetres. */
    static constexpr float PlayHalfSize = 1300.f;

    /**
     * The raised deck, which Scripts/Build-Arena.py builds into the map.
     *
     * These numbers live in two places -- the map comes from an editor script and
     * the props from this generator -- so they have to be kept in step by hand.
     * The generator needs them only to keep obstacles off the deck and its ramps;
     * a prop dropped at ground level inside the deck would be buried in it.
     */
    static constexpr float DeckCentreX = 780.f;
    static constexpr float DeckKeepOutX = 520.f;
    static constexpr float DeckKeepOutY = 520.f;
    static constexpr float DeckTop = 130.f;

    /** Feet-to-eye height used when asking whether an obstacle hides a target. */
    static constexpr float BodyHeight = 180.f;

    /** A shooter this far above a target shoots down on it. Sits above head height
     *  so standing on a crate is not enough; the raised deck is. */
    static constexpr float HighGroundStep = 100.f;

    int32        ArmorBonus(EAHCover Cover);
    const TCHAR* CoverName(EAHCover Cover);

    /**
     * Cover the target at To has from a shooter at From. Both are feet positions.
     * Only obstacles genuinely between the two count, and an obstacle is measured
     * against the TARGET's feet: a crate that hides someone standing on the ground
     * hides nobody standing on the raised deck.
     *
     * Caps at ThreeQuarters on purpose. Whether a shot is blocked outright is
     * already decided by the line-of-sight trace in TryRangedAttack, which also
     * sees walls and houses that were never in this list; two systems answering
     * the same question is how they end up disagreeing.
     */
    EAHCover CoverBetween(const TArray<FAHArenaPiece>& Pieces, const FVector& From, const FVector& To);

    /** True when the shooter is high enough above the target for advantage. */
    bool HasHighGround(const FVector& From, const FVector& To);

    /**
     * The obstacle layout for one encounter.
     *
     * Deterministic in the seed. The straight line between the two spawns is kept
     * clear, so an unlucky roll can never wall the encounter off before it starts.
     */
    TArray<FAHArenaPiece> Generate(FRandomStream& Dice, const FVector& HeroSpawn, const FVector& FoeSpawn);
}
