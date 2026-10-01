#include "AHVillager.h"
#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHAnimInstance.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

// ── Who they are ─────────────────────────────────────────────────────────────
/**
 * Names, trades and one thing each of them says.
 *
 * Written out rather than generated, because six trades times four names is
 * twenty-four strings and a name generator that produces "Grunlak the
 * Woodcutter" is worse than a short list that produces "Bento, lenhador".
 * Portuguese, like every other word in this game.
 */
namespace
{
    const TCHAR* const GNomes[] =
    {
        TEXT("Mira"), TEXT("Bento"), TEXT("Alzira"), TEXT("Tiago"),
        TEXT("Dulce"), TEXT("Rufino"), TEXT("Inacia"), TEXT("Gervasio"),
        TEXT("Lenita"), TEXT("Anselmo"), TEXT("Joana"), TEXT("Simao"),
        TEXT("Elvira"), TEXT("Damiao"), TEXT("Nica"), TEXT("Olavo"),
    };

    struct FOficio { const TCHAR* Nome; const TCHAR* Falas[3]; };

    const FOficio GOficios[] =
    {
        // Aldeao
        { TEXT("aldea"),      { TEXT("Bom dia. Nao se afaste da estrada."),
                                TEXT("Dizem que tem gente armada nos morros."),
                                TEXT("Se procura pousada, nao temos. Desculpe.") } },
        // Mercador
        { TEXT("mercador"),   { TEXT("Hoje e so o que esta na banca."),
                                TEXT("Moeda eu aceito. Promessa, nao."),
                                TEXT("Leve dois e eu penso no preco.") } },
        // Lenhador
        { TEXT("lenhador"),   { TEXT("O corte esta duro este ano."),
                                TEXT("Cuidado ali dentro: mato fechado, gente pior."),
                                TEXT("Se ouvir o machado, e so eu.") } },
        // Pedreiro
        { TEXT("pedreiro"),   { TEXT("Pedra boa, comprador nenhum."),
                                TEXT("Nao pise perto da face. Ela cede."),
                                TEXT("Trabalho desde antes do sol.") } },
        // Guarda
        { TEXT("guarda"),     { TEXT("Passe, mas passe rapido."),
                                TEXT("Meu posto e aqui. Nao pergunte mais."),
                                TEXT("Vi fogo no morro ontem a noite.") } },
        // Carroceiro (o do recado)
        { TEXT("carroceiro"), { TEXT("Eh! Voce ai! Preciso de uma mao."),
                                TEXT("Minha mochila... eu tive que largar tudo."),
                                TEXT("Eu esperava alguem armado passar por aqui.") } },
    };

    /** Somebody walking past says one of these. Short: it is read in passing. */
    const TCHAR* const GPassando[] =
    {
        TEXT("Bom dia."), TEXT("Deus lhe guarde."), TEXT("Anda com cuidado."),
        TEXT("Forasteiro..."), TEXT("Nao vi voce por aqui antes."),
        TEXT("Boa jornada."),
    };

    /**
     * What each trade wears, and it is a readability rule before it is a
     * palette.
     *
     * Every foe in the game is dressed in the dark red-to-brown family (see
     * BecomeEnemy). Nobody who lives here may be: greens, blues, ochres and
     * bleached linen. Told apart at thirty metres by colour alone, which is
     * the only distance this camera ever works at, and the answer to "tudo
     * cinza" for the half of the screen that is people.
     */
    const FLinearColor GRoupa[] =
    {
        FLinearColor(.451f, .424f, .318f),   // aldeao: undyed wool
        FLinearColor(.180f, .286f, .420f),   // mercador: the one who can afford blue
        FLinearColor(.212f, .365f, .208f),   // lenhador: green
        FLinearColor(.404f, .388f, .360f),   // pedreiro: stone dust
        FLinearColor(.278f, .278f, .318f),   // guarda: slate
        FLinearColor(.545f, .404f, .180f),   // carroceiro: ochre, the errand
    };
}

AAHVillager::AAHVillager()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    bUseControllerRotationYaw = false;

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    // The same body and the same locomotion graph as everybody else. The graph
    // asks the PAWN for its velocity, not AAHCharacter -- so it animates this
    // class unchanged, which is the one thing that made a separate class cheap.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Corpo(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    if (Corpo.Object) GetMesh()->SetSkeletalMesh(Corpo.Object);
    GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
    GetMesh()->SetAnimInstanceClass(UAHAnimInstance::StaticClass());
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GetCharacterMovement()->bOrientRotationToMovement  = true;
    GetCharacterMovement()->RotationRate               = FRotator(0.0, 420.0, 0.0);
    // A stroll, not a patrol. Everything about a walking pace says whether a
    // person is going somewhere or living somewhere.
    GetCharacterMovement()->MaxWalkSpeed               = 145.f;
    GetCharacterMovement()->MaxAcceleration            = 900.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1400.f;
}

void AAHVillager::BeginPlay()
{
    Super::BeginPlay();
    SpawnDefaultController();
}

void AAHVillager::Settle(const FAHFolkSpot& Spot, int32 Seed)
{
    Dice.Initialize(Seed ? Seed : 1);
    Trade   = Spot.Trade;
    bGiver  = Spot.bGiver;
    Casa    = Spot.Where;
    Alcance = Spot.Range;
    Parado  = Spot.Yaw;

    const int32 Which = static_cast<int32>(Trade);
    const int32 Slot  = FMath::Clamp(Which, 0,
        static_cast<int32>(UE_ARRAY_COUNT(GOficios)) - 1);
    Nome   = GNomes[Dice.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(GNomes)) - 1)];
    Oficio = GOficios[Slot].Nome;

    SetActorRotation(FRotator(0.f, Parado, 0.f));
    ProximoPasso = Dice.FRandRange(1.f, 6.f);
    ProximaFala  = 0.f;

    // A little variation inside the trade's colour, so a village is people
    // rather than a uniform. Hashed off the same stream, so the same woman is
    // the same woman every time you walk back into the street.
    FLinearColor Worn = GRoupa[FMath::Clamp(Which, 0,
        static_cast<int32>(UE_ARRAY_COUNT(GRoupa)) - 1)];
    const float Wear = Dice.FRandRange(.82f, 1.16f);
    Worn = FLinearColor(FMath::Min(Worn.R * Wear, 1.f),
                        FMath::Min(Worn.G * Wear, 1.f),
                        FMath::Min(Worn.B * Wear, 1.f), 1.f);
    Veste(Worn);
}

void AAHVillager::Veste(const FLinearColor& Tone)
{
    if (!GetMesh()) return;
    UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/AshenHollow/Kit/Materials/M_AH_Corpo"),
        nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Cloth) return;          // kit not imported: keep the grey mannequin
    const int32 Slots = FMath::Max(1, GetMesh()->GetNumMaterials());
    for (int32 Slot = 0; Slot < Slots; ++Slot)
        if (UMaterialInstanceDynamic* Dyed = GetMesh()->CreateDynamicMaterialInstance(Slot, Cloth))
            Dyed->SetVectorParameterValue(TEXT("Cor"), Tone);
}

void AAHVillager::Diga(const FString& What, float Seconds)
{
    Fala    = What;
    FalaAte = GetWorld()->GetTimeSeconds() + Seconds;
}

void AAHVillager::Conversa(AAHCharacter* Who)
{
    if (!IsValid(Who)) return;
    auto* Mode = GetWorld()->GetAuthGameMode<AAHGameMode>();
    const int32 Slot = FMath::Clamp(static_cast<int32>(Trade), 0,
        static_cast<int32>(UE_ARRAY_COUNT(GOficios)) - 1);

    // Stop and look at whoever is talking. Nothing reads as "ignored" faster
    // than a person who answers you with his back turned.
    if (auto* Brain = Cast<AAIController>(GetController())) Brain->StopMovement();
    bOlhando = true;
    const FVector Toward = Who->GetActorLocation() - GetActorLocation();
    SetActorRotation(FRotator(0.f, static_cast<float>(
        FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X))), 0.f));

    // ── The errand ───────────────────────────────────────────────────────
    if (bGiver && Mode)
    {
        if (Mode->ErrandStage == 0)
        {
            Mode->ErrandStage = 1;
            Diga(TEXT("Minha mochila ficou na masmorra. Traga ela!"), 7.f);
            Who->AddLog(FString::Printf(
                TEXT("%s, carroceiro: \"Fugi dos bandidos e larguei minha mochila la dentro."), *Nome));
            Who->AddLog(TEXT("Tem tudo que eu tenho nela. Traga de volta e eu recompenso.\""));
            Who->AddLog(TEXT("MISSAO ACEITA: recuperar a mochila do carroceiro."));
            return;
        }
        if (Mode->ErrandStage == 1)
        {
            Diga(TEXT("Ainda esta la dentro. Por favor."), 6.f);
            Who->AddLog(FString::Printf(TEXT("%s: \"A mochila ainda esta na masmorra.\""), *Nome));
            return;
        }
        if (Mode->ErrandStage == 2)
        {
            Mode->ErrandStage = 3;
            Diga(TEXT("Voce trouxe! Que os santos lhe paguem."), 8.f);
            Who->AddLog(FString::Printf(
                TEXT("%s: \"Voce trouxe! Nao tenho ouro, mas tenho pao, agua e o que sei.\""), *Nome));
            // Paid in the two currencies this game actually has.
            Who->GainExperience(250);
            if (Who->IsAlive() && Who->Health < Who->MaxHealth)
                Who->ApplyHealing(FMath::Max(2, Who->MaxHealth / 3));
            Who->AddLog(TEXT("MISSAO CUMPRIDA: +250 de experiencia, e um descanso na estrada."));
            return;
        }
        Diga(TEXT("Se precisar de agua, e ali no odre."), 5.f);
        Who->AddLog(FString::Printf(TEXT("%s: \"Boa estrada, e obrigado outra vez.\""), *Nome));
        return;
    }

    if (Mode && Mode->TalkSideQuest(this,Who)) return;

    // Everybody else: three lines each, in order, then the first again. Enough
    // that talking to the same person twice is not the same sentence, and
    // little enough that nothing here pretends to be a conversation system.
    const FString Linha = GOficios[Slot].Falas[Conversas % 3];
    ++Conversas;
    Diga(Linha, 6.f);
    Who->AddLog(FString::Printf(TEXT("%s, %s: \"%s\""), *Nome, *Oficio, *Linha));
}

void AAHVillager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Now = GetWorld()->GetTimeSeconds();
    if (FalaAte > 0.f && Now > FalaAte) { Fala.Reset(); FalaAte = 0.f; }

    auto* Brain = Cast<AAIController>(GetController());
    if (!Brain) return;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

    // ── Running away ─────────────────────────────────────────────────────
    /**
     * A fight starting next to somebody who keeps sweeping his step is the
     * single most lifeless thing a village can do.
     *
     * The nearest living hostile inside twelve metres is enough to send him
     * off: he picks a reachable point on the far side of himself from it and
     * goes, at a speed that is plainly not a stroll. He does not fight, he
     * cannot be hurt by anything in this build, and once the foe is gone he
     * walks home -- which is why bFugindo is cleared by distance rather than
     * by a timer.
     */
    // Asked twice a second, not sixty times. Ten villagers each walking every
    // character in the world every frame is the kind of arithmetic that turns
    // "the map has people in it now" into "the game got slower".
    if (Now >= ProximoMedo)
    {
        ProximoMedo = Now + .5f;
        MedoDe.Reset();
        float PerigoPerto = 1200.f;
        for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
        {
            AAHCharacter* Other = *It;
            if (!IsValid(Other) || !Other->bEnemy || !Other->IsAlive()) continue;
            const float Away = FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());
            if (Away < PerigoPerto) { PerigoPerto = Away; MedoDe = Other; }
        }
    }
    AAHCharacter* Perigo = MedoDe.Get();
    if (Perigo && !Perigo->IsAlive()) Perigo = nullptr;
    if (Perigo)
    {
        GetCharacterMovement()->MaxWalkSpeed = 430.f;
        if (!bFugindo)
        {
            bFugindo = true;
            Diga(Dice.RandRange(0, 1) ? TEXT("Corre! Corre!") : TEXT("Socorro!"), 4.f);
        }
        if (Now >= ProximoPasso && Nav)
        {
            ProximoPasso = Now + 2.2f;
            const FVector Fora = GetActorLocation()
                + (GetActorLocation() - Perigo->GetActorLocation()).GetSafeNormal2D() * 1400.f;
            FNavLocation Longe;
            if (Nav->ProjectPointToNavigation(Fora, Longe, FVector(600.f, 600.f, 400.f)))
                Brain->MoveToLocation(Longe.Location, 60.f, false, true, false, false, nullptr, true);
        }
        return;
    }
    if (bFugindo)
    {
        bFugindo = false;
        GetCharacterMovement()->MaxWalkSpeed = 145.f;
        ProximoPasso = Now;          // walk home now, not in six seconds
    }

    // ── Stopping to look at the player ───────────────────────────────────
    const float Perto = Hero ? FVector::Dist2D(GetActorLocation(), Hero->GetActorLocation())
                             : 100000.f;
    if (Perto < 340.f)
    {
        if (!bOlhando)
        {
            bOlhando = true;
            Brain->StopMovement();
            // A greeting, but not every time he is walked past: the same six
            // words on a loop is worse than silence.
            if (Now >= ProximaFala)
            {
                ProximaFala = Now + Dice.FRandRange(22.f, 40.f);
                Diga(GPassando[Dice.RandRange(0,
                    static_cast<int32>(UE_ARRAY_COUNT(GPassando)) - 1)], 3.6f);
            }
        }
        const FVector Toward = Hero->GetActorLocation() - GetActorLocation();
        const FRotator Want(0.f, static_cast<float>(
            FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X))), 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 5.f));
        return;
    }
    if (bOlhando && Perto > 520.f) { bOlhando = false; ProximoPasso = Now + 1.f; }

    // ── The stroll ───────────────────────────────────────────────────────
    // A guard and a trader have a post: Alcance is zero for them, and standing
    // still facing the way the generator pointed them IS the behaviour.
    if (Alcance < 60.f)
    {
        const FRotator Want(0.f, Parado, 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 3.f));
        return;
    }
    if (Now < ProximoPasso || !Nav) return;
    ProximoPasso = Now + Dice.FRandRange(4.5f, 10.f);
    FNavLocation Passo;
    if (Nav->GetRandomReachablePointInRadius(Casa, Alcance, Passo))
        Brain->MoveToLocation(Passo.Location, 45.f, false, true, false, false, nullptr, true);
}
