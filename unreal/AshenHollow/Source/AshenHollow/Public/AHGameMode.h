#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AHArena.h"
#include "AHGameMode.generated.h"

UCLASS()
class ASHENHOLLOW_API AAHGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAHGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY() TArray<TObjectPtr<class AAHCharacter>> Order;
    int32 Round = 1;
    int32 ActiveIndex = 0;
    bool bStarted = false;
    bool bFinished = false;
    float TurnStarted = 0.f;
    AAHCharacter* ActiveCharacter() const;

    // ── Arena ─────────────────────────────────────────────────────────────────
    /** Where the two sides start. Ten metres apart, so contact is one move away. */
    static const FVector HeroSpawn;
    static const FVector FoeSpawn;
    /** Obstacles for the current encounter. Re-rolled from a new seed each time. */
    TArray<FAHArenaPiece> Obstacles;
    UPROPERTY() TArray<TObjectPtr<AActor>> ObstacleActors;
    /** Clears the old props and lays out a fresh encounter from Seed. */
    void BuildArena(int32 Seed);
    /**
     * A seed that genuinely differs between launches.
     *
     * FMath::Rand() forwards to rand(), which replays the same sequence from the
     * same process start unless something called srand() first -- so seeding from
     * it would have built the identical arena every single time the game opened,
     * which is the exact opposite of the promise.
     */
    static int32 FreshSeed();
    /** Seed behind the layout on screen. Shown in the HUD so "procedural" is
     *  something you can watch change, rather than something you take on faith. */
    int32 ArenaSeed = 0;
    /** Cover the target standing at To has against a shooter standing at From. */
    EAHCover CoverBetween(const FVector& From, const FVector& To) const;

    bool NextEncounter();
    bool EndTurn(AAHCharacter* Requester);
};
