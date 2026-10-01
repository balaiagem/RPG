#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AHArena.h"
#include "AHVillager.generated.h"

/**
 * Somebody who lives here.
 *
 * WHY THIS IS NOT AAHCharacter
 * ----------------------------
 * It would have been fewer lines: AAHCharacter already owns the mannequin, the
 * animation instance, the AI controller and the walk. But every combat rule in
 * this project asks the same question to tell friend from foe -- `Other->bEnemy
 * != bEnemy` -- and there are eleven places that ask it: the threat latch, the
 * opportunity attack, the melee and ranged target tests, the aura sweeps, the
 * nearest-enemy search, the initiative sort. A villager is a character with
 * bEnemy false, which every one of those reads as "on the hero's side", so
 * eleven separate rules would have had to learn about a third kind of person,
 * and the cost of missing one of them is a bandit taking an opportunity swing
 * at a woodcutter, or a hen in the initiative order.
 *
 * A separate class cannot be seen by any of them. That is the whole argument.
 * What it costs is this file: a dozen lines of constructor copied from
 * AAHCharacter, and a wander that does not borrow the turn machinery.
 *
 * What it gives, and what Lucas asked for -- "quero deixar o mapa com vida" --
 * is people. They stroll their own street, stop and look at you when you come
 * near, say something, and run when a fight starts. One of them is carrying an
 * errand.
 */
UCLASS()
class ASHENHOLLOW_API AAHVillager : public ACharacter
{
    GENERATED_BODY()
public:
    AAHVillager();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Takes a spot from the generator and becomes the person standing in it. */
    void Settle(const FAHFolkSpot& Spot, int32 Seed);

    /** "Mira" -- shown over his head and in the conversation. */
    FString Nome;
    /** "Tecela", "Lenhador": what the HUD writes under the name. */
    FString Oficio;
    EAHFolk Trade  = EAHFolk::Aldeao;
    /** True for the one with the errand. There is at most one per valley. */
    bool    bGiver = false;
    /** Where he belongs, and how far he may stray from it. */
    FVector Casa   = FVector::ZeroVector;
    float   Alcance = 600.f;
    /** Which way he looks when he has nothing else to do. */
    float   Parado  = 0.f;

    /**
     * What he is saying right now, and the time it stops being drawn.
     *
     * Speech is a string with a clock on it and nothing else -- no widget, no
     * queue, no state machine. Three quarters of what makes a village feel
     * inhabited is that its people react to you walking past, and that is one
     * line of text over a head for four seconds.
     */
    FString Fala;
    float   FalaAte = 0.f;
    void Diga(const FString& What, float Seconds = 4.5f);

    /** Somebody is talking to him. Advances the errand when he is the giver. */
    void Conversa(class AAHCharacter* Who);

    /** Dresses him in his trade's colour. See AAHCharacter::PaintBody. */
    void Veste(const FLinearColor& Tone);

    /** True while he is running away from a fight. */
    bool bFugindo = false;

private:
    FRandomStream Dice;
    float ProximoPasso = 0.f;
    /** When to look for a fight nearby again, and what was found last time. */
    float ProximoMedo  = 0.f;
    TWeakObjectPtr<class AAHCharacter> MedoDe;
    float ProximaFala  = 0.f;
    /** Set while he has stopped to look at the player, so he stops once. */
    bool  bOlhando = false;
    /** How many times this one has been spoken to, so he does not repeat. */
    int32 Conversas = 0;
};
