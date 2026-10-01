#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHArena.h"
#include "AHBeast.generated.h"

/**
 * An animal.
 *
 * A static mesh that moves, and deliberately nothing more: no skeleton, no
 * animation instance, no navmesh query, no collision, no sheet of statistics.
 * A hen is forty draw-call-cheap centimetres of geometry that pecks two metres
 * one way and two metres back, and a crow is a silhouette going round in a
 * circle nine metres up.
 *
 * THAT IS THE WHOLE TRICK, and it is worth saying why. What makes a valley look
 * dead is not that it is empty -- it has four thousand props in it -- but that
 * nothing in it MOVES. Movement is what the eye reads as life, and movement is
 * the cheapest thing on this list to fake convincingly: a hen that hops while
 * it walks reads as a hen from a camera thirty metres up, and a deer that bolts
 * when you come within ten metres reads as a deer far better than a correctly
 * animated one standing still would.
 *
 * Collision is off, on purpose and in both directions. It keeps them out of
 * navigation -- a moving obstacle is exactly what makes Recast rebuild tiles
 * forever, and this project has spent ten rounds on navigation already -- and
 * it means a hen can never wedge the player against a wall. What stops them
 * walking through houses is a line trace before each leg of the journey, which
 * costs one trace every few seconds per animal.
 */
UCLASS()
class ASHENHOLLOW_API AAHBeast : public AActor
{
    GENERATED_BODY()
public:
    AAHBeast();
    virtual void Tick(float DeltaSeconds) override;

    /** Becomes the animal the generator described, with its body and its floor. */
    void Settle(const FAHBeastSpot& Spot, int32 Seed, class UStaticMesh* Body, float FloorZ);

    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Corpo;

    EAHBeast Especie = EAHBeast::Galinha;
    /** Where it belongs and how far it strays; for a crow, its circle. */
    FVector  Casa    = FVector::ZeroVector;
    float    Alcance = 300.f;

private:
    FRandomStream Dice;
    /** Where it is walking to, and when to choose somewhere else. */
    FVector Destino     = FVector::ZeroVector;
    float   ProximoAlvo = 0.f;
    /** Ground under it, re-traced a few times a second rather than every frame. */
    float   Chao        = 0.f;
    float   ProximoChao = 0.f;
    /** Crows: where they are round the circle, and how high. */
    float   Volta       = 0.f;
    float   Altura      = 1200.f;
    /** Deer: seconds left of running away. */
    float   Assustado   = 0.f;
    float   Passo       = 70.f;
    /** Phase of the hop, so two hens in a yard are not in lockstep. */
    float   Salto       = 0.f;

    /** Somewhere new to walk, with nothing solid between here and there. */
    void EscolheDestino();
};
