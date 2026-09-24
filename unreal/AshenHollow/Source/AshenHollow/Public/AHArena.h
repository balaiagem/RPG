#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * The arena: what it is made of, and the cover it gives.
 *
 * The whole encounter space is drawn here from a seed -- houses, walls, braziers,
 * market stalls, the raised deck and the loose obstacles -- rather than only the
 * props. The map asset keeps just what has to be baked: the ground everyone walks
 * on, the navigation bounds, the spawn point and the light actors themselves.
 *
 * The cover rule reads this list rather than tracing the physics world, for one
 * practical reason: automation tests have no props in their world, and a rule
 * that can only be checked by playing is a rule nobody checks.
 */

/** SRD 5.1 degrees of cover. Total cover cannot be targeted at all. */
enum class EAHCover : uint8 { None, Half, ThreeQuarters, Total };

/** The shape an arena takes. One is rolled per encounter. */
enum class EAHArenaKind : uint8 { VillageSquare, MarketStreet, Farmstead, Ruins, Count };

/**
 * One placed thing.
 *
 * MeshPath may name a static mesh or a blueprint; the game mode works out which.
 * Radius and Height start as the generator's estimate and are replaced with the
 * real measured bounds once the thing exists, so the cover maths runs against the
 * prop's actual size and never against a number somebody typed.
 */
struct FAHArenaPiece
{
    FString  MeshPath;
    FVector  Location     = FVector::ZeroVector;
    FRotator Rotation     = FRotator::ZeroRotator;
    FVector  Scale        = FVector::OneVector;
    float    Radius       = 60.f;
    /**
     * World height of this piece's top surface -- not its height above its own
     * origin. A deck's origin is its middle, so "origin plus height" overshot the
     * top by half; where the top is has one meaning and the game mode measures it.
     */
    float    TopZ         = 90.f;
    /** Counts towards cover. False for ramps, ground decoration and grass. */
    bool     bCover       = true;
    /** Dropped so its base rests on the floor. False for boxes placed by maths. */
    bool     bSitOnGround = true;
    /** A warm point light is spawned here too (braziers, campfires, lamps). */
    bool     bLight       = false;
    /** Keep the asset's own material rather than overriding it. */
    FString  MaterialPath;
};

/** Everything one encounter needs, rolled together so it reads as one place. */
struct FAHArenaPlan
{
    EAHArenaKind Kind = EAHArenaKind::VillageSquare;
    FString      Name;                 // shown in the HUD
    TArray<FAHArenaPiece> Pieces;
    // Light varies with the arena, so two encounters are not the same hour of
    // the same day. Far cheaper than new geometry and it changes the mood more.
    float SunPitch       = -34.f;
    float SunYaw         = -55.f;
    float SunTemperature = 5200.f;
    float SkyIntensity   = 1.7f;
};

namespace AHArena
{
    /** Half-width of the playable square, in centimetres. */
    static constexpr float PlayHalfSize = 1300.f;

    /** Feet-to-eye height used when asking whether an obstacle hides a target. */
    static constexpr float BodyHeight = 180.f;

    /** A shooter this far above a target shoots down on it. Above head height, so
     *  standing on a crate is not enough; the raised deck is. */
    static constexpr float HighGroundStep = 100.f;

    /** Walking surface of the raised deck. */
    static constexpr float DeckTop = 130.f;

    int32        ArmorBonus(EAHCover Cover);
    const TCHAR* CoverName(EAHCover Cover);
    const TCHAR* KindName(EAHArenaKind Kind);

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
     * Builds one whole arena.
     *
     * Deterministic in the seed. Measure is asked for a mesh's half-extent and
     * must answer for any path the generator names: fences can only be tiled
     * without gaps or overlaps by someone who knows how long a fence panel
     * actually is, and that is a question about the asset, not a constant.
     *
     * The straight line between the two spawns is always left clear, so an
     * unlucky roll can never wall the encounter off before it starts.
     */
    FAHArenaPlan Build(FRandomStream& Dice, const FVector& HeroSpawn, const FVector& FoeSpawn,
                       TFunctionRef<FVector(const FString&)> Measure);
}
