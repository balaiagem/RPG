#include "AHCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "AHNotify_MeleeImpact.h"
#include "AHAnimInstance.h"
#include "AHMagicVisual.h"
#include "AHEquipmentComponent.h"
#include "AHLifeAudioComponent.h"
#include "AHCombatBurst.h"
#include "AHGameMode.h"
#include "AHPlayerController.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AAHCharacter::AAHCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    bUseControllerRotationYaw = false;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    Equipment = CreateDefaultSubobject<UAHEquipmentComponent>(TEXT("Equipment"));
    LifeAudio = CreateDefaultSubobject<UAHLifeAudioComponent>(TEXT("LifeAudio"));

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    GetMesh()->SetSkeletalMesh(Body.Object);
    GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
    GetMesh()->SetAnimInstanceClass(UAHAnimInstance::StaticClass());
    LocomotionClass = UAHAnimInstance::StaticClass();

    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Attack(TEXT("/Game/AshenHollow/Animation/MM_Attack_01"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> AlternateAttack(TEXT("/Game/AshenHollow/Animation/MM_Attack_02"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Death(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01"));
    AttackAnimation = Attack.Object;
    AlternateAttackAnimation = AlternateAttack.Object;
    DeathAnimation  = Death.Object;
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Hit(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01"));
    HitAnimation = Hit.Object;
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> CastClip(TEXT("/Game/AshenHollow/Animation/AH_Cast")),HealClip(TEXT("/Game/AshenHollow/Animation/AH_Heal")),RageClip(TEXT("/Game/AshenHollow/Animation/AH_Rage")),GuardClip(TEXT("/Game/AshenHollow/Animation/AH_Guard")),EvadeClip(TEXT("/Game/AshenHollow/Animation/AH_Evade"));
    CastAnimation=CastClip.Object; HealAnimation=HealClip.Object; RageAnimation=RageClip.Object; GuardAnimation=GuardClip.Object; EvadeAnimation=EvadeClip.Object;
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> Sword(TEXT("/Game/AshenHollow/Animation/AH_SwordSlash")),Axe(TEXT("/Game/AshenHollow/Animation/AH_AxeCleave")),Mace(TEXT("/Game/AshenHollow/Animation/AH_MaceStrike")),Staff(TEXT("/Game/AshenHollow/Animation/AH_StaffStrike"));
    // One entry per EAHWeaponKind, in enum order. A bow has no authored swing --
    // shooting plays the cast clip -- so its melee bash borrows the staff strike,
    // which is the closest two-handed motion we own.
    WeaponAnimations={Sword.Object,Axe.Object,Mace.Object,Staff.Object,Staff.Object};
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetRelativeRotation(FRotator(-48.0, -45.0, 0.0));
    CameraBoom->TargetArmLength = 2200.0f;
    CameraBoom->TargetOffset   = FVector(0, 180, 0);
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed   = 7.0f;
    CameraBoom->bDoCollisionTest = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("IsometricCamera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->FieldOfView = 43.0f;
    Camera->PostProcessSettings.bOverride_AutoExposureBias=true;
    Camera->PostProcessSettings.AutoExposureBias=0.f;
    Camera->PostProcessSettings.bOverride_BloomIntensity=true;
    Camera->PostProcessSettings.BloomIntensity=.18f;
    Camera->PostProcessSettings.bOverride_BloomThreshold=true;
    Camera->PostProcessSettings.BloomThreshold=1.2f;
    Camera->bUsePawnControlRotation = false;

    GetCharacterMovement()->bOrientRotationToMovement  = true;
    GetCharacterMovement()->RotationRate               = FRotator(0.0, 720.0, 0.0);
    GetCharacterMovement()->MaxWalkSpeed               = 600.0f;
    GetCharacterMovement()->MaxAcceleration            = 2400.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2400.0f;
}

void AAHCharacter::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Dice.GenerateNewSeed();
    PreviousLocation = GetActorLocation();
}

/** Plain-language note explaining a roll that used more than one die. */
static FString AHRollTag(const FAHDiceOutcome& Roll)
{
    FString Tag;
    if (Roll.Advantage > 0)      Tag += FString::Printf(TEXT("  [vantagem, %d descartado]"), Roll.DiscardedRoll);
    else if (Roll.Advantage < 0) Tag += FString::Printf(TEXT("  [desvantagem, %d descartado]"), Roll.DiscardedRoll);
    if (Roll.bLuckyReroll)       Tag += TEXT("  [sorte: 1 natural rerrolado]");
    return Tag;
}

void AAHCharacter::PaintBody(const FLinearColor& Tone)
{
    if (!GetMesh()) return;
    UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/AshenHollow/Kit/Materials/M_AH_Corpo"),
        nullptr, LOAD_NoWarn | LOAD_Quiet);
    // No material: the kit has not been imported into this project yet. Keep
    // the mannequin's own skin rather than turning everybody invisible.
    if (!Cloth) return;
    const int32 Slots = FMath::Max(1, GetMesh()->GetNumMaterials());
    for (int32 Slot = 0; Slot < Slots; ++Slot)
        if (UMaterialInstanceDynamic* Dyed = GetMesh()->CreateDynamicMaterialInstance(Slot, Cloth))
            Dyed->SetVectorParameterValue(TEXT("Cor"), Tone);
}

// ── Enemy setup ──────────────────────────────────────────────────────────────

void AAHCharacter::BecomeEnemy(int32 AppearanceSeed)
{
    bEnemy = true;

    // Roll the foe's archetype so no two encounters open the same way.
    FRandomStream Pick(AppearanceSeed); if (!AppearanceSeed) Pick.GenerateNewSeed();
    EnemyHome = GetActorLocation();
    HeroClass = static_cast<EAHHeroClass>(Pick.RandRange(0, AHRules::ClassCount()-1));
    Ancestry  = static_cast<EAHAncestry>(Pick.RandRange(0, AHRules::AncestryCount()-1));
    ApplySheet(HeroClass);
    InitializeSpellbook();

    // Foes are a little tougher than a level-1 hero so a duel lasts a few rounds.
    // Four rather than six: a wizard opens with eight hit points, and against
    // three of these the old margin was not a duel, it was arithmetic.
    MaxHealth += 4;
    Health = MaxHealth;

    EnemyName = AHRules::Class(HeroClass).FoeName;

    GetCharacterMovement()->MaxWalkSpeed = 330.f;
    SpawnDefaultController();
    Equipment->Configure(AHRules::Class(HeroClass).Weapon);

    /**
     * And a colour, so a bandit is not the same grey shape as the man selling
     * turnips.
     *
     * All of them sit in a dark red-to-brown family on purpose: the archetype
     * tells them apart from each other, and the family tells them apart from
     * everybody who is not trying to kill you. Readability at thirty metres
     * beats variety.
     */
    static const FLinearColor Blood[] =
    {
        FLinearColor(.336f, .086f, .075f), FLinearColor(.258f, .094f, .126f),
        FLinearColor(.400f, .148f, .062f), FLinearColor(.212f, .118f, .154f),
        FLinearColor(.296f, .140f, .086f), FLinearColor(.178f, .102f, .108f),
        FLinearColor(.360f, .180f, .096f), FLinearColor(.240f, .072f, .092f),
    };
    const int32 Shade = FMath::Abs(static_cast<int32>(HeroClass))
                      % static_cast<int32>(UE_ARRAY_COUNT(Blood));
    PaintBody(Blood[Shade]);
}

// ── Turn management ───────────────────────────────────────────────────────────

void AAHCharacter::StartTurn()
{
    if(BlessTurns>0) --BlessTurns;
    if(GuardTurns>0 && --GuardTurns==0) ArmorClass-=2;
    bSneakUsed=false; bSteadyAim=false; bAimMovementLocked=false; MovementSpentThisTurn=0;
    bColossusUsed=false;
    // The arcane Shield stands until the start of your next turn, which is this.
    if(ShieldTurns>0 && --ShieldTurns==0) ArmorClass-=5;
    if(SacredTurns>0) --SacredTurns;
    if(MarkTurns>0 && --MarkTurns==0) MarkedTarget.Reset();
    ++TurnsStarted; bBonusSpellCast=false; bLeveledActionSpellCast=false;
    bReckless=false;
    TurnStartTime = GetWorld()->GetTimeSeconds();
    bPatrolling=false; RepathAt=0; FailedPaths=0; ProgressAt=TurnStartTime; ProgressLocation=GetActorLocation();

    // ── Downed: roll death save instead of acting ─────────────────────────────
    if(bDowned)
    {
        bTurnActive = true;
        if(!bStabilized) RollDeathSave();
        else GetWorldTimerManager().SetTimer(DeathSaveTimer, this, &AAHCharacter::CompleteDeathSaveTurn, 1.8f, false);
        // FinishTurn is called via timer from RollDeathSave after a short pause
        return;
    }

    Turn.Reset();
    Turn.Movement=FMath::Max(0.f,BaseMovement-(FrostTurns>0?300.f:0.f));
    if(bRaging && --RageTurns<=0) bRaging=false;
    bTurnActive  = true;
    bDodging     = false;
    bDashing     = false;
    bDisengaging = false;
    PreviousLocation = GetActorLocation();
    Feedback = bEnemy ? TEXT("Turno do inimigo") : TEXT("Seu turno — escolha uma ação");
}

void AAHCharacter::RollDeathSave()
{
    int32 SaveDiscarded=0; bool bSaveLucky=false;
    const int32 Roll = UAHDiceRules::RollD20Detailed(Dice,0,
        IsLucky(),SaveDiscarded,bSaveLucky);
    DeathSaveRollTime = GetWorld()->GetTimeSeconds();
    LastRoll=FAHDiceOutcome(); LastRoll.NaturalRoll=Roll; LastRoll.Total=Roll; LastRoll.Target=10;
    LastRoll.bLuckyReroll=bSaveLucky;
    if(bSaveLucky) AddLog(TEXT("Sorte: a salvaguarda saiu 1 natural e foi rerrolada."));
    LastRoll.bSuccess=Roll>=10; LastRoll.bCritical=Roll==20;
    LastRollLabel=TEXT("SALVAGUARDA DE MORTE"); LastRollTime=DeathSaveRollTime;

    if(Roll == 20)
    {
        // Natural 20: revive with 1 HP
        bDowned        = false;
        bStabilized    = false;
        DeathSuccesses = 0;
        DeathFailures  = 0;
        Health         = 1;
        bLastDeathSaveSuccess = true;
        GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(LocomotionClass);
        AddLog(TEXT("Milagre! Salvaguarda de morte 20 - recuperado com 1 PV!"));
        Feedback = TEXT("Recuperado com 1 PV!");
        // End the turn immediately (can't act this turn)
        bTurnActive=false;
        GetWorldTimerManager().SetTimer(DeathSaveTimer, this, &AAHCharacter::CompleteDeathSaveTurn, 1.8f, false);
        return;
    }

    if(Roll == 1)
    {
        // Natural 1: two failures
        DeathFailures += 2;
        bLastDeathSaveSuccess = false;
        AddLog(TEXT("Salvaguarda de morte: 1 crítico — 2 falhas!"));
    }
    else if(Roll >= 10)
    {
        ++DeathSuccesses;
        bLastDeathSaveSuccess = true;
        AddLog(FString::Printf(TEXT("Salvaguarda de morte: %d - sucesso (%d/3)"), Roll, DeathSuccesses));
    }
    else
    {
        ++DeathFailures;
        bLastDeathSaveSuccess = false;
        AddLog(FString::Printf(TEXT("Salvaguarda de morte: %d - falha (%d/3)"), Roll, DeathFailures));
    }

    if(DeathSuccesses >= 3)
    {
        bStabilized = true;
        AddLog(TEXT("Estabilizado - sem mais salvaguardas."));
        Feedback = TEXT("Estabilizado.");
    }
    else if(DeathFailures >= 3)
    {
        // Truly dead — play death animation and disable everything
        bDowned = false;
        AddLog(TEXT("3 falhas - personagem morreu."));
        Feedback = TEXT("Morto.");
        if(IsValid(MagicVisual)) MagicVisual->Destroy(); MagicVisual=nullptr;
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation, false);
        PendingSpellId=-1; PendingTarget=nullptr; ImpactAt=-1.f; bImpactResolved=true; AnimationEnds=0.f; bIsAttacking=false;
    }

    // End turn after 1.8 s so the HUD has time to show the result
    GetWorldTimerManager().SetTimer(DeathSaveTimer, this, &AAHCharacter::CompleteDeathSaveTurn, 1.8f, false);
}

void AAHCharacter::CompleteDeathSaveTurn()
{
    if(auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode())) Mode->EndTurn(this);
    else FinishTurn();
}

void AAHCharacter::FinishTurn()
{
    if(bTurnActive && FrostTurns>0) --FrostTurns;
    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        if(It->GuidingSource.Get()==this && TurnsStarted>=It->GuidingExpiresTurn) It->GuidingSource.Reset();
    GetWorldTimerManager().ClearTimer(DeathSaveTimer);
    if(bRaging && !bAttackedSinceTurnEnd && !bDamagedSinceTurnEnd) bRaging=false;
    bAttackedSinceTurnEnd=false; bDamagedSinceTurnEnd=false;
    bTurnActive = false;
    if (GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
}

void AAHCharacter::AddLog(const FString& Message)
{
    CombatLog.Add(Message);
    if (CombatLog.Num() > 5) CombatLog.RemoveAt(0);
}

// ── Tick ──────────────────────────────────────────────────────────────────────

void AAHCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Now = GetWorld()->GetTimeSeconds();

    if (!IsAlive() && !IsDowned()) return;

    if (!bEnemy)
    {
        FVector FocusOffset = FVector::ZeroVector;
        if (const auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode()))
            for (const auto& Participant : Mode->Order)
                if (IsValid(Participant) && Participant->bEnemy && Participant->IsAlive())
                {
                    FocusOffset = (Participant->GetActorLocation() - GetActorLocation()) * .5f;
                    FocusOffset.Z = 0.f;
                    break;
                }
        CameraBoom->TargetOffset = FMath::VInterpTo(CameraBoom->TargetOffset, FocusOffset, DeltaSeconds, 3.f);

        CameraYaw   = FMath::FInterpTo(CameraYaw,   CameraYawTarget,   DeltaSeconds, 8.f);
        CameraReach = FMath::FInterpTo(CameraReach, CameraReachTarget, DeltaSeconds, 7.f);
        if (FMath::Abs(CameraYaw - CameraYawTarget) < .05f)
        {
            // Settled: fold both back into -180..180 together, so a long session
            // of turning the same way never drifts into large angles. Folding the
            // pair at once means there is nothing to see.
            CameraYaw = CameraYawTarget = static_cast<float>(FRotator::NormalizeAxis(CameraYawTarget));
        }
        CameraBoom->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.f));
        CameraBoom->TargetArmLength = CameraReach;
    }

    if (ReactionEnds > 0.f && Now >= ReactionEnds)
    {
        ReactionEnds = 0.f;
        if (!bIsAttacking && GetMesh()->GetAnimInstance())
            GetMesh()->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
    }

    // ── Animation slot finished ───────────────────────────────────────────────
    if (AnimationEnds > 0.f && Now >= AnimationEnds)
    {
        AnimationEnds = 0.f;
        bIsAttacking  = false;
        if (GetMesh()->GetAnimInstance())
            GetMesh()->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);

        if (!bImpactResolved && ImpactAt > 0.f)
        {
            ResolveImpact();
        }
    }

    // ── Fallback impact timer ─────────────────────────────────────────────────
    if (!bImpactResolved && PendingTarget && ImpactAt > 0.f && Now >= ImpactAt)
    {
        ResolveImpact();
    }

    // ── Movement budget ───────────────────────────────────────────────────────
    if (bTurnActive)
    {
        const FVector Step     = GetActorLocation() - PreviousLocation;
        const float   Distance = Step.Size2D();
        if (Distance > Turn.Movement && Distance > UE_SMALL_NUMBER)
        {
            FVector Allowed = PreviousLocation + Step * (Turn.Movement / Distance);
            Allowed.Z = GetActorLocation().Z;
            SetActorLocation(Allowed, false);
        }
        MovementSpentThisTurn+=FMath::Min(Distance,Turn.Movement);
        Turn.Travel(Distance);
        if (Turn.Movement <= 1.f)
        {
            if (GetController()) GetController()->StopMovement();
            GetCharacterMovement()->StopMovementImmediately();
        }
    }
    else if (!bPatrolling)
    {
        GetCharacterMovement()->StopMovementImmediately();
    }
    PreviousLocation = GetActorLocation();

    // Leaving a hostile's reach hands it a free swing (SRD 5.1 reaction).
    // This can knock us down, which ends the turn inside the call.
    UpdateThreatState();

    // Ten metres a second was a sprint, not an explorer. Six is a brisk walk
    // across a 130 m valley and still lets you look at the place.
    const float MaxSpeed = bEnemy ? (bPatrolling ? 140.f : 330.f)
                         : bRoaming ? (bSneaking ? 320.f : 620.f)
                         : bDashing ? 650.f : 480.f;
    GetCharacterMovement()->MaxWalkSpeed =
        ((bTurnActive || bPatrolling) && !IsBusy())
        ? (bPatrolling ? MaxSpeed : FMath::Min(MaxSpeed, Turn.Movement / FMath::Max(DeltaSeconds, .001f)))
        : 0.f;

    TickEnemyAI(Now);
}

// ── Reactions ─────────────────────────────────────────────────────────────────

void AAHCharacter::UpdateThreatState()
{
    InReachOf.RemoveAll([](const TObjectPtr<AAHCharacter>& Entry) { return !IsValid(Entry.Get()); });
    if (!IsAlive() || bDowned) { InReachOf.Reset(); return; }

    // Collect first: a reaction can knock this character down, and resolving one
    // must not invalidate the actor iterator.
    TArray<AAHCharacter*, TInlineAllocator<4>> Reactors;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        AAHCharacter* Other = *It;
        if (!IsValid(Other) || Other == this || Other->bEnemy == bEnemy) continue;

        const float Reach     = FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());
        const bool  bStanding = Other->IsAlive() && !Other->bDowned;

        if (!InReachOf.Contains(Other))
        {
            // Latch on well inside the reach, so a foe that merely grazes the
            // boundary on its way past cannot arm a reaction against us.
            if (bStanding && Reach <= ThreatReach - 15.f) InReachOf.Add(Other);
            continue;
        }

        if (bStanding && Reach <= ThreatReach) continue;   // still threatened

        InReachOf.Remove(Other);
        // Only a departure under our own power, on our own turn, provokes.
        if (bStanding && bTurnActive && !bDisengaging) Reactors.Add(Other);
    }

    for (AAHCharacter* Reactor : Reactors)
    {
        if (!IsAlive()) break;
        Reactor->TryOpportunityAttack(this);
    }
}

bool AAHCharacter::TryOpportunityAttack(AAHCharacter* Mover, bool bConfirmed)
{
    if (!IsValid(Mover) || Mover == this || Mover->bEnemy == bEnemy) return false;
    if (!IsAlive() || bDowned || IsBusy() || !Turn.bReaction)          return false;
    if (!Mover->IsAlive() || Mover->bDowned)                           return false;

    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OpportunitySight), false, this);
    Query.AddIgnoredActor(Mover);
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(),
            Mover->GetActorLocation(), ECC_Visibility, Query))
        return false;

    if(!bEnemy && !bConfirmed)
        if(auto* PC=Cast<AAHPlayerController>(GetController())) return PC->OfferReaction(Mover);
    if (!Turn.SpendReaction()) return false;

    const bool GuidingAdvantage=Mover->HasGuidingMark();
    Mover->GuidingSource.Reset();
    OpportunityFlashTime = GetWorld()->GetTimeSeconds();
    SetActorRotation(FRotator(0, (Mover->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));

    FAHDiceOutcome Result = UAHDiceRules::RollAttack(
        Dice,
        AttackRollBonus()+(BlessTurns>0?Dice.RandRange(1,4):0),
        Mover->ArmorClass,
        WeaponDice(),
        DamageSides,
        MeleeDamageBonus(),
        ((Mover->bReckless || GuidingAdvantage)?1:0)-(Mover->bDodging?1:0),
        IsLucky());
    ApplySubclassCrit(Result, DamageSides);

    // Cosmetic swing only. A reaction must never make the reacting character
    // busy, so bIsAttacking / AnimationEnds are deliberately left untouched.
    if (auto* Anim = GetMesh()->GetAnimInstance())
    {
        UAnimationAsset* Clip = WeaponClip();
        if (auto* Sequence = Cast<UAnimSequenceBase>(Clip))
        {
            Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
            Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), .08f, .18f, 1.35f, 1);
            ReactionEnds = GetWorld()->GetTimeSeconds() + Sequence->GetPlayLength() / 1.35f;
        }
    }

    if (Result.bSuccess)
    {
        // A melee hit on an unconscious creature within reach is a critical (PHB 292).
        if(Mover->bDowned && !Mover->bStabilized) Result.bCritical=true;
        const int32 Radiant=AddWeaponRiders(Mover,Result,false);
        Mover->ReceiveHit(Result.Damage,EAHDamageType::Physical,Radiant,Result.bCritical);
        Result.Damage = Mover->LastDamage;
    }
    else
    {
        AAHCombatBurst::Emit(GetWorld(), Mover->GetActorLocation() + FVector(0, 0, 25), EAHBurst::Guard);
    }

    Mover->ImpactText = Result.bSuccess
        ? FString::Printf(TEXT("OPORTUNIDADE -%d"), Mover->LastDamage)
        : TEXT("OPORTUNIDADE ERROU");
    Mover->ImpactTextTime      = GetWorld()->GetTimeSeconds();
    Mover->bImpactHealing      = false;
    Mover->bLastImpactCritical = Result.bCritical;

    // The player must always see the die that resolved the reaction.
    AAHCharacter* Viewer = bEnemy ? Mover : this;
    Viewer->LastRoll      = Result;
    Viewer->LastRollLabel = bEnemy ? TEXT("OPORTUNIDADE INIMIGA") : TEXT("SEU ATAQUE DE OPORTUNIDADE");
    Viewer->LastRollTime  = GetWorld()->GetTimeSeconds();
    Viewer->AddLog(FString::Printf(TEXT("Oportunidade: %d + %d = %d  /  %s  %d de dano%s"),
        Result.NaturalRoll, Result.Modifier, Result.Total,
        Result.bSuccess ? TEXT("ACERTO") : TEXT("ERRO"), Result.Damage, *AHRollTag(Result)));

    UE_LOG(LogTemp, Display,
        TEXT("AH_OPPORTUNITY %s d20=%d total=%d AC=%d hit=%d damage=%d"),
        bEnemy ? TEXT("ENEMY") : TEXT("HERO"),
        Result.NaturalRoll, Result.Total, Result.Target, Result.bSuccess, Result.Damage);
    return true;
}

// ── Attack ────────────────────────────────────────────────────────────────────

namespace
{
    /** A projectile's look and head count, chosen from the attack that fired it. */
    struct FAHShotLook { EAHProjectileLook Look; int32 Heads; };

    /**
     * Every ranged attack used to draw the same three arcane spheres, so an arrow,
     * a fire bolt and a magic missile were the same picture. The decision is made
     * from the attack rather than the caster: a wizard's class-level Fire Bolt and
     * a ranger's arrow both arrive here with no spell id, and only the weapon on
     * the sheet tells them apart.
     */
    FAHShotLook ShotLook(int32 SpellId, int32 SpellRank, EAHWeaponKind Weapon)
    {
        if(SpellId < 0)
            return Weapon==EAHWeaponKind::Bow
                 ? FAHShotLook{ EAHProjectileLook::Arrow, 1 }
                 : FAHShotLook{ EAHProjectileLook::Fire,  1 };
        switch(static_cast<EAHSpell>(SpellId))
        {
            case EAHSpell::MagicMissile:  return { EAHProjectileLook::Arcane,   2+SpellRank };
            case EAHSpell::ScorchingRay:  return { EAHProjectileLook::Fire,     3 };
            case EAHSpell::FireBolt:
            case EAHSpell::BurningHands:  return { EAHProjectileLook::Fire,     1 };
            case EAHSpell::RayOfFrost:    return { EAHProjectileLook::Frost,    1 };
            case EAHSpell::GuidingBolt:
            case EAHSpell::SacredFlame:   return { EAHProjectileLook::Radiant,  1 };
            case EAHSpell::InflictWounds: return { EAHProjectileLook::Necrotic, 1 };
            default:                      return { EAHProjectileLook::Arcane,   1 };
        }
    }
}

UAnimationAsset* AAHCharacter::WeaponClip() const
{
    // EAHWeaponKind indexes WeaponAnimations. The bare subscript that used to sit
    // in PlayAttack would have asserted the first time a new weapon kind was added
    // without its clip, so every reader goes through this guard instead.
    const auto* Worn=bEnemy?nullptr:AHItems::Find(Equipped[static_cast<int32>(EAHSlot::MaoPrincipal)]);
    const int32 Index=static_cast<int32>(Worn?Worn->Arte:AHRules::Class(HeroClass).Weapon);
    return WeaponAnimations.IsValidIndex(Index) ? WeaponAnimations[Index].Get() : AttackAnimation.Get();
}
void AAHCharacter::PlayAttack()
{
    const bool bShot = PendingRange > 0.f || PendingSpellId>=0;
    MotionLabel=PendingSpellDamage>0?TEXT("CONJURANDO"):bShot?TEXT("MIRANDO"):TEXT("ATACANDO");
    UAnimationAsset* Clip=(PendingSpellDamage>0||bShot)?CastAnimation.Get():WeaponClip();
    auto* Sequence=Cast<UAnimSequenceBase>(Clip?Clip:AttackAnimation.Get());
    if (Sequence && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(
            Sequence, TEXT("DefaultSlot"), .14f, .22f, 1.f, 1);
    }

    const float Length = Sequence ? Sequence->GetPlayLength() : 0.9f;
    AnimationEnds   = GetWorld()->GetTimeSeconds() + FMath::Max(0.9f, Length);
    ImpactAt        = GetWorld()->GetTimeSeconds() + Length * ImpactFraction;
    if (Sequence && Sequence->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
        { return Event.Notify && Event.Notify->IsA<UAHNotify_MeleeImpact>(); }))
        ImpactAt = AnimationEnds;
    bIsAttacking    = true;
    bImpactResolved = false;
    if((PendingSpellDamage>0||bShot) && PendingTarget && GetNetMode()!=NM_DedicatedServer)
    {
        float Travel=Length*ImpactFraction;
        if(Sequence)
            for(const FAnimNotifyEvent& Event:Sequence->Notifies)
                if(Event.Notify && Event.Notify->IsA<UAHNotify_MeleeImpact>())
                { Travel=Event.GetTriggerTime(); break; }
        const FAHShotLook Shot=ShotLook(PendingSpellId,PendingSpellRank,AHRules::Class(HeroClass).Weapon);
        MagicVisual=GetWorld()->SpawnActor<AAHMagicVisual>();
        if(MagicVisual)
            MagicVisual->Initialize(GetMesh()->GetSocketLocation(TEXT("hand_r")),PendingTarget,
                                    Travel,Shot.Heads,Shot.Look);
        // The bow has its own skeleton and its own release clip, so the string
        // actually snaps forward when the arrow leaves.
        if(Shot.Look==EAHProjectileLook::Arrow && Equipment) Equipment->PlayShot();
    }
}

void AAHCharacter::PlayGesture(UAnimationAsset* Asset)
{
    MotionLabel=Asset==HealAnimation?TEXT("CURANDO"):Asset==RageAnimation?TEXT("FURIA"):Asset==GuardAnimation?TEXT("DEFENDENDO"):Asset==EvadeAnimation?TEXT("EVADINDO"):TEXT("CONJURANDO");
    auto* Sequence=Cast<UAnimSequenceBase>(Asset);
    if(!Sequence || !GetMesh()->GetAnimInstance()) return;
    if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    GetMesh()->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),.12f,.2f,1.f,1);
    AnimationEnds=GetWorld()->GetTimeSeconds()+Sequence->GetPlayLength();
    bIsAttacking=true; bImpactResolved=true; ImpactAt=-1.f;
}

bool AAHCharacter::TryAttack(AAHCharacter* Target, bool bBonusAction)
{
    if (!CanAct() || !IsValid(Target) || !Target->IsAlive() || Target->bEnemy == bEnemy)
        return false;
    if (bBonusAction ? !Turn.bBonus : !Turn.bAction) return false;
    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > 190.f)
    {
        Feedback = TEXT("Alvo fora do alcance corpo a corpo");
        return false;
    }
    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MeleeSight), false, this);
    Query.AddIgnoredActor(Target);
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
    {
        Feedback = TEXT("Alvo obstruído");
        return false;
    }

    if (bBonusAction) Turn.SpendBonus(); else Turn.SpendAction();
    if (GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));

    // Cover counts in melee too, by the book. At 190 cm of reach it will rarely
    // fire, but a rule that only half applies is worse than one that does not.
    const EAHCover MeleeCover = Target->CoverFrom(this);
    // Assassinate: nobody has found their feet in the first round.
    const bool bAssassin = HeroClass==EAHHeroClass::Rogue && Level>=3 && CombatRound()==1;
    PendingRoll = UAHDiceRules::RollAttack(
        Dice,
        AttackRollBonus()+(BlessTurns>0?Dice.RandRange(1,4):0),
        Target->ArmorClass + AHArena::ArmorBonus(MeleeCover),
        WeaponDice(),
        DamageSides,
        MeleeDamageBonus(),
        ((bHidden || bSteadyAim || bAssassin || bReckless || Target->bReckless || Target->HasGuidingMark() || HasHighGroundOn(Target))?1:0)-(Target->bDodging?1:0),IsLucky());
    ApplySubclassCrit(PendingRoll, DamageSides);
    Target->GuidingSource.Reset(); bSteadyAim=false; bHidden=false;
    PendingTarget = Target;
    bAttackedSinceTurnEnd=true;

    PlayAttack();
    Feedback = TEXT("Resolvendo o ataque...");
    return true;
}

FString AAHCharacter::RacialAbilityName() const
{
    return AHRules::Ancestry(Ancestry).bBreathWeapon ? TEXT("SOPRO") : FString();
}

bool AAHCharacter::CanUseRacialAbility() const
{
    return AHRules::Ancestry(Ancestry).bBreathWeapon && !bBreathUsed && CanAct() && Turn.bAction;
}

void AAHCharacter::UseRacialAbility()
{
    if (!CanUseRacialAbility()) return;
    Turn.SpendAction();
    bBreathUsed = true;

    // Face the nearest hostile so the cone is forgiving about where you clicked.
    AAHCharacter* Nearest=nullptr; float Best=BreathReach;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        AAHCharacter* Other=*It;
        if (!IsValid(Other)||Other==this||Other->bEnemy==bEnemy||!Other->IsAlive()) continue;
        const float Distance=FVector::Dist2D(GetActorLocation(),Other->GetActorLocation());
        if (Distance<Best) { Best=Distance; Nearest=Other; }
    }
    if (Nearest)
        SetActorRotation(FRotator(0,(Nearest->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0));

    // Saving throws are not modelled yet, so the breath simply lands.
    const int32 Damage = Dice.RandRange(1,6) + Dice.RandRange(1,6);
    const FVector Forward = GetActorForwardVector();
    int32 Caught = 0;
    TArray<AAHCharacter*, TInlineAllocator<4>> InCone;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        AAHCharacter* Other=*It;
        if (!IsValid(Other)||Other==this||Other->bEnemy==bEnemy||!Other->IsAlive()) continue;
        const FVector To=Other->GetActorLocation()-GetActorLocation();
        if (To.Size2D()>BreathReach) continue;
        if (FVector::DotProduct(Forward,To.GetSafeNormal2D())<0.35f) continue;   // ~70 degree cone
        InCone.Add(Other);
    }
    for (AAHCharacter* Burned : InCone) { Burned->ReceiveHit(Damage,EAHDamageType::Fire); ++Caught; }

    PlayGesture(CastAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()+Forward*130.f+FVector(0,0,40),EAHBurst::Rage);
    Feedback = FString::Printf(TEXT("Sopro dracônico: %d de dano de fogo em %d alvo(s)"),Damage,Caught);
    AddLog(Feedback);
}

FVector AAHCharacter::FootLocation() const
{
    // Cover and height are questions about the ground someone stands on, and
    // GetActorLocation on a Character answers with the capsule's middle.
    return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()
        ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f);
}

EAHCover AAHCharacter::CoverFrom(const AAHCharacter* Shooter) const
{
    if (!IsValid(Shooter) || !GetWorld()) return EAHCover::None;
    const auto* Arena = GetWorld()->GetAuthGameMode<AAHGameMode>();
    return Arena ? Arena->CoverBetween(Shooter->FootLocation(), FootLocation()) : EAHCover::None;
}

bool AAHCharacter::HasHighGroundOn(const AAHCharacter* Target) const
{
    return IsValid(Target) && AHArena::HasHighGround(FootLocation(), Target->FootLocation());
}

bool AAHCharacter::IsThreatenedInMelee() const
{
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        const AAHCharacter* Other = *It;
        if (!IsValid(Other) || Other == this || Other->bEnemy == bEnemy) continue;
        if (!Other->IsAlive() || Other->bDowned) continue;
        if (FVector::Dist2D(GetActorLocation(), Other->GetActorLocation()) <= ThreatReach) return true;
    }
    return false;
}

bool AAHCharacter::TryRangedAttack(AAHCharacter* Target)
{
    if (RangedRange <= 0) return false;
    if (!CanAct() || !Turn.bAction) return false;
    if (!IsValid(Target) || !Target->IsAlive() || Target->bEnemy == bEnemy) return false;

    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > RangedRange)
    {
        Feedback = FString::Printf(TEXT("Alvo além do alcance de %.0f m"), RangedRange/100.f);
        return false;
    }
    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RangedSight), false, this);
    Query.AddIgnoredActor(Target);
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation()+FVector(0,0,40),
            Target->GetActorLocation()+FVector(0,0,40), ECC_Visibility, Query))
    {
        Feedback = TEXT("Sem linha de visão até o alvo");
        return false;
    }

    Turn.SpendAction();
    if (GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0, (Target->GetActorLocation()-GetActorLocation()).Rotation().Yaw, 0));

    // Shooting with someone in your face is a disadvantaged shot.
    const bool bCrowded = IsThreatenedInMelee();
    // Advantage sources stay grouped in one ||: high ground and reckless attack
    // together are still advantage, never two steps of it.
    const EAHCover ShotCover = Target->CoverFrom(this);
    const bool bShotAssassin = HeroClass==EAHHeroClass::Rogue && Level>=3 && CombatRound()==1;
    PendingRoll = UAHDiceRules::RollAttack(
        Dice, RangedAttackBonus()+(SacredTurns>0?2:0)+(BlessTurns>0?Dice.RandRange(1,4):0),
        Target->ArmorClass + AHArena::ArmorBonus(ShotCover), 1,
        RangedSides, RangedBonus,
        ((bHidden || bSteadyAim || bShotAssassin || bReckless || Target->bReckless || Target->HasGuidingMark() || HasHighGroundOn(Target)) ? 1 : 0)
            - ((Target->bDodging || bCrowded) ? 1 : 0),
        IsLucky());
    ApplySubclassCrit(PendingRoll, RangedSides);
    PendingTarget = Target;
    Target->GuidingSource.Reset(); bSteadyAim=false; bHidden=false;
    // A little tolerance so the target drifting a step does not void the shot.
    PendingRange  = static_cast<float>(RangedRange) + 80.f;
    bAttackedSinceTurnEnd = true;

    PlayAttack();
    // Say what the dice were told. A modifier the player cannot see reads as the
    // game cheating, which is how advantage and halfling luck read before.
    FString Why;
    if (bCrowded) Why += TEXT(" · desvantagem: inimigo em corpo a corpo");
    if (ShotCover != EAHCover::None)
        Why += FString::Printf(TEXT(" · %s do alvo: +%d CA"),
                               AHArena::CoverName(ShotCover), AHArena::ArmorBonus(ShotCover));
    if (HasHighGroundOn(Target)) Why += TEXT(" · vantagem: terreno elevado");
    Feedback = FString::Printf(TEXT("Disparo%s"), *Why);
    return true;
}

// ── Impact resolution ─────────────────────────────────────────────────────────

void AAHCharacter::OnMeleeImpactNotify()
{
    if (bImpactResolved) return;
    if (PendingTarget) UE_LOG(LogTemp, Display, TEXT("AH_CONTACT_NOTIFY %s"), bEnemy ? TEXT("ENEMY") : TEXT("HERO"));
    bImpactResolved = true;
    ImpactAt = -1.f;
    ResolveImpact();
}

void AAHCharacter::ResolveImpact()
{
    if(IsValid(MagicVisual)) MagicVisual->Destroy();
    MagicVisual=nullptr;
    bImpactResolved = true;
    ImpactAt = -1.f;

    if(PendingSpellId>=0) { ResolveSpellImpact(); return; }
    auto* Target = PendingTarget.Get();
    PendingTarget  = nullptr;
    const int32 SpellDamage=PendingSpellDamage; PendingSpellDamage=0;
    const float ShotRange=PendingRange; PendingRange=0.f;
    if (!IsAlive() || !IsValid(Target) || !Target->IsAlive()) return;

    if(SpellDamage>0)
    {
        const int32 Damage=SpellDamage;
        FHitResult Block;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(SpellSight),false,this); Sight.AddIgnoredActor(Target);
        if(FVector::Dist(GetActorLocation(),Target->GetActorLocation())>1800.f ||
           GetWorld()->LineTraceSingleByChannel(Block,GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Sight))
        { AddLog(TEXT("Mísseis cancelados: alvo obstruído ou distante")); return; }
        Target->ReceiveHit(Damage,EAHDamageType::Force);
        LastRollTime=-100.f;
        Feedback=FString::Printf(TEXT("Mísseis Mágicos: %d de dano de força"),Damage);
        AddLog(Feedback);
        return;
    }

    auto Result = PendingRoll;

    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ImpactSight), false, this);
    Query.AddIgnoredActor(Target);
    const float ReachLimit = ShotRange > 0.f ? ShotRange : 210.f;
    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > ReachLimit
        || GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
    {
        Result.bSuccess  = false;
        Result.bCritical = false;
        Result.Damage    = 0;
    }

    auto* Viewer = bEnemy ? Target : this;
    Viewer->LastRoll      = Result;
    Viewer->LastRollLabel = ShotRange > 0.f
        ? (bEnemy ? EnemyName + TEXT(" / DISPARO") : FString(TEXT("SEU DISPARO")))
        : (bEnemy ? EnemyName + TEXT(" / ATAQUE")  : FString(TEXT("SEU ATAQUE")));
    Viewer->LastRollTime  = GetWorld()->GetTimeSeconds();

    // Last chance to not be hit: stop here and ask.
    //
    // The prompt pauses the game, so this swing cannot finish inside the call
    // that asked. It is parked on this character and picked up again by
    // ResumeShieldedImpact, which re-enters this function with the answer
    // already folded into PendingRoll -- bShieldAsked is what stops that second
    // pass from asking all over again.
    if (Result.bSuccess && !bShieldAsked && !Target->bEnemy
        && AHSpells::ForClass(EAHSpell::Shield, Target->HeroClass))
    {
        const FString Refusal = Target->ShieldRefusal();
        UE_LOG(LogTemp, Display,
            TEXT("AH_SHIELD %s (total %d, CA %d, reacao %d, espacos %d)"),
            Refusal.IsEmpty() ? TEXT("OFERECIDO") : *Refusal,
            Result.Total, Result.Target, Target->Turn.bReaction ? 1 : 0, Target->ClassCharges);

        if (Refusal.IsEmpty())
        {
            auto* Defender = Cast<AAHPlayerController>(Target->GetController());
            if (Defender && Defender->OfferReaction(this, EAHReaction::Shield))
            {
                HeldTarget    = Target;
                HeldRoll      = Result;
                HeldShotRange = ShotRange;
                bShieldAsked  = true;
                PendingTarget = nullptr;
                return;
            }
            // The pause was refused. Put the Shield up anyway rather than let
            // the only reaction spell in the game quietly do nothing -- erring
            // towards the spell working is the right side to err on.
            Target->RaiseShield(Result);
            Viewer->LastRoll = Result;
        }
        else
        {
            // A rule that correctly does nothing still has to say so, or it is
            // indistinguishable from a rule that is broken.
            Target->AddLog(FString::Printf(TEXT("Escudo nao oferecido: %s"), *Refusal));
        }
    }

    if (Result.bSuccess)
    {
        // A melee hit on an unconscious creature within reach is a critical (PHB 292).
        // A shot from range is not, so the auto-crit is gated on being in melee.
        if(Target->bDowned && !Target->bStabilized && ShotRange<=0.f) Result.bCritical=true;
        const int32 Radiant=AddWeaponRiders(Target,Result,ShotRange>0.f);
        Target->ReceiveHit(Result.Damage,EAHDamageType::Physical,Radiant,Result.bCritical);
        Result.Damage=Target->LastDamage;
        Viewer->LastRoll=Result;
    }
    else if(!Target->IsBusy())
    {
        Target->PlayGesture(Target->bDodging?Target->GuardAnimation.Get():Target->EvadeAnimation.Get());
        AAHCombatBurst::Emit(GetWorld(),Target->GetActorLocation()+FVector(0,0,25),EAHBurst::Guard);
    }
    Target->bImpactHealing = false;
    Target->bLastImpactCritical = Result.bCritical;
    Target->ImpactTextTime = GetWorld()->GetTimeSeconds();
    Target->ImpactText = Result.bSuccess
        ? FString::Printf(TEXT("%s-%d"), Result.bCritical ? TEXT("CRITICO  ") : TEXT(""), Target->LastDamage)
        : TEXT("ERROU");

    Viewer->AddLog(FString::Printf(TEXT("%s  %d + %d = %d  /  %s  %d de dano%s"),
        bEnemy ? TEXT("Inimigo") : TEXT("Você"),
        Result.NaturalRoll, Result.Modifier, Result.Total,
        Result.bSuccess ? TEXT("ACERTO") : TEXT("ERRO"),
        Result.Damage, *AHRollTag(Result)));
    Feedback = Result.bSuccess ? TEXT("Ataque resolvido") : TEXT("O ataque errou");

    UE_LOG(LogTemp, Display,
        TEXT("AH_IMPACT %s d20=%d total=%d AC=%d hit=%d damage=%d"),
        bEnemy ? TEXT("ENEMY") : TEXT("HERO"),
        Result.NaturalRoll, Result.Total, Result.Target,
        Result.bSuccess, Result.Damage);
}

void AAHCharacter::ResumeShieldedImpact(bool bShield)
{
    auto* Target = HeldTarget.Get();
    HeldTarget = nullptr;
    if (!IsValid(Target)) { bShieldAsked = false; return; }

    PendingTarget = Target;
    PendingRoll   = HeldRoll;
    PendingRange  = HeldShotRange;
    if (bShield) Target->RaiseShield(PendingRoll);
    else         Target->AddLog(TEXT("Escudo recusado: a reacao fica guardada"));

    // Straight back through the same door. Everything the second pass needs is
    // in the pending fields, and bShieldAsked keeps it from asking again.
    bImpactResolved = false;
    ResolveImpact();
    bShieldAsked = false;
}

// ── Other actions ─────────────────────────────────────────────────────────────

void AAHCharacter::ReceiveHit(int32 Damage, EAHDamageType Type, int32 RadiantBonus, bool bCriticalHit)
{
    if ((!IsAlive() && !bDowned) || Damage+RadiantBonus <= 0) return;
    const FAHAncestrySheet& Blood = AHRules::Ancestry(Ancestry);
    if (bRaging && Type == EAHDamageType::Physical) Damage/=2;
    if (Blood.bFireResistant && Type == EAHDamageType::Fire) Damage/=2;
    Damage+=RadiantBonus;
    if (Damage <= 0) return;
    if(LifeAudio) LifeAudio->Impact();
    if(bEnemy)
        if(auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>())
        { if(Mode->IsExploring()) Mode->EngageWith(this); Mode->DrawInBystanders(this); }
    if((GuardTurns>0 || BlessTurns>0 || MarkTurns>0) && (Damage>=Health+TempHP || !UAHDiceRules::RollCheck(Dice,ConcentrationModifier()+(BlessTurns>0?Dice.RandRange(1,4):0),FMath::Max(10,Damage/2),0,IsLucky()).bSuccess))
    { if(GuardTurns>0) ArmorClass-=2; GuardTurns=BlessTurns=MarkTurns=0; MarkedTarget.Reset(); AddLog(TEXT("Concentracao encerrada")); }
    bDamagedSinceTurnEnd=true;
    LastDamageTime = GetWorld()->GetTimeSeconds();

    // ── TempHP absorbs damage first ───────────────────────────────────────────
    if(TempHP > 0)
    {
        const int32 Absorbed = FMath::Min(TempHP, Damage);
        TempHP  -= Absorbed;
        Damage  -= Absorbed;
        if(Damage <= 0)
        {
            LastDamage = 0;
            ImpactText = TEXT("BLOQUEADO");
            ImpactTextTime = LastDamageTime;
            bImpactHealing = false; bLastImpactCritical = false;
            const FVector HitOrigin=GetActorLocation()+FVector(0,0,35);
            if(HitFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),HitFX,HitOrigin,GetActorRotation());
            else AAHCombatBurst::Emit(GetWorld(),HitOrigin,Type==EAHDamageType::Physical?EAHBurst::Steel:EAHBurst::Force);
            return;
        }
    }

    // ── Apply to real HP ──────────────────────────────────────────────────────
    LastDamage = FMath::Min(Health, Damage);
    const int32 Overkill = Damage - LastDamage;   // damage left over after HP ran out
    Health     = FMath::Max(0, Health - Damage);
    ImpactText = FString::Printf(TEXT("-%d"), LastDamage);
    // Half-orc: one refusal to drop, per rest. Checked before the downed
    // transition below so the character simply stays standing on 1 HP.
    if (Health <= 0 && !bDowned && Blood.bRelentless && !bRelentlessUsed)
    {
        bRelentlessUsed = true;
        Health = 1;
        ImpactText = FString::Printf(TEXT("-%d  RESISTE!"), LastDamage);
        Feedback = TEXT("Perseverança implacável: de pé com 1 PV.");
        AddLog(Feedback);
    }
    ImpactTextTime = LastDamageTime;
    bImpactHealing = false;
    bLastImpactCritical = false;
    const FVector HitOrigin=GetActorLocation()+FVector(0,0,35);
    if(HitFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),HitFX,HitOrigin,GetActorRotation());
    else AAHCombatBurst::Emit(GetWorld(),HitOrigin,Type==EAHDamageType::Physical?EAHBurst::Steel:EAHBurst::Force);

    if (IsAlive())
    {
        // ── Cosmetic hit animation ────────────────────────────────────────────
        if (!bIsAttacking)
            if (auto* Anim = GetMesh()->GetAnimInstance())
                if (auto* Sequence = Cast<UAnimSequenceBase>(HitAnimation))
                {
                    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
                    Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), .06f, .18f, 1.f, 1);
                    ReactionEnds = GetWorld()->GetTimeSeconds() + Sequence->GetPlayLength();
                }
        return;
    }

    if(bEnemy && !bLootDropped)
        if(auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>()) { bLootDropped=true; Mode->DropLoot(this); }

    // ── Massive damage: leftover damage meeting your maximum kills outright ──
    // PHB 197. No death saves, no stabilising.
    if(Health<=0 && !bDowned && Overkill>=MaxHealth)
    {
        bDowned=false; bStabilized=false;
        DeathSuccesses=0; DeathFailures=3;
        bIsAttacking=false; AnimationEnds=0.f;
        FinishTurn();
        PendingTarget=nullptr; ImpactAt=-1.f; bImpactResolved=true;
        if(IsValid(MagicVisual)) MagicVisual->Destroy(); MagicVisual=nullptr;
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation,false);
        ImpactText=FString::Printf(TEXT("-%d  FATAL!"),LastDamage);
        AddLog(TEXT("Dano massivo: morte instantanea, sem salvaguardas."));
        // Said out loud, with the numbers, because "it did not roll death
        // saves" and "it killed me outright by the massive-damage rule" look
        // identical from the outside -- and one of them is a bug while the
        // other is PHB 197. The log has to be able to tell them apart.
        UE_LOG(LogTemp, Warning,
               TEXT("AH_DOWN %s MORTE DIRETA: dano %d, sobra %d, PV max %d (regra de dano massivo)"),
               *GetName(), LastDamage, Overkill, MaxHealth);
        Feedback=TEXT("Morto por dano massivo.");
        return;
    }

    // ── HP reached 0: transition to downed state (D&D death saves) ───────────
    if(!bDowned)
    {
        bDowned        = true;
        bStabilized    = false;
        DeathSuccesses = 0;
        DeathFailures  = 0;
        bIsAttacking   = false;
        AnimationEnds  = 0.f;
        FinishTurn();
        PendingTarget  = nullptr;
        ImpactAt       = -1.f;
        bImpactResolved= true;
        if(IsValid(MagicVisual)) MagicVisual->Destroy(); MagicVisual=nullptr;
        // Play collapse / fall animation; do NOT disable movement yet so we can still be targeted
        if (DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation, false);
        UE_LOG(LogTemp, Warning,
               TEXT("AH_DOWN %s NOCAUTEADO: dano %d, PV max %d -- salvaguardas comecam no proximo turno dele"),
               *GetName(), LastDamage, MaxHealth);
        AddLog(TEXT("Nocauteado! Salvaguardas de morte a seguir."));
        Feedback = TEXT("Nocauteado - role salvaguardas de morte");
    }
    else
    {
        // Hit while already downed. A critical costs two failures (PHB 197).
        DeathFailures += bCriticalHit ? 2 : 1;
        if(bStabilized) DeathSuccesses=0;
        bStabilized=false;
        AddLog(TEXT("Atingido enquanto nocauteado - falha adicional!"));
        if(DeathFailures >= 3)
        {
            bDowned = false;
            GetCharacterMovement()->DisableMovement();
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            if(IsValid(MagicVisual)) MagicVisual->Destroy(); MagicVisual=nullptr;
            AddLog(TEXT("Morto."));
        }
    }
}

void AAHCharacter::Dodge()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    bDodging = true;
    PlayGesture(GuardAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation(),EAHBurst::Guard);
    Feedback = TEXT("Esquivando até seu próximo turno");
    AddLog(TEXT("Esquiva: ataques recebidos têm desvantagem"));
}

void AAHCharacter::Disengage()
{
    if (!CanAct() || bDisengaging || !Turn.SpendAction()) return;
    bDisengaging = true;
    PlayGesture(EvadeAnimation);
    AAHCombatBurst::Emit(GetWorld(), GetActorLocation() - FVector(0, 0, 45), EAHBurst::Guard);
    Feedback = TEXT("Desengajar: seu movimento não provoca ataques de oportunidade neste turno");
    AddLog(TEXT("Desengajar: movimento livre de reações neste turno"));
}

void AAHCharacter::RotateCamera(float Degrees)
{
    if (bEnemy) return;
    CameraYawTarget += Degrees;
}

void AAHCharacter::ZoomCamera(float Steps)
{
    if (bEnemy) return;
    // Clamped so the view can never end up inside the character or so far out
    // that the action stops being readable.
    CameraReachTarget = FMath::Clamp(CameraReachTarget + Steps * 260.f, 1200.f, 3600.f);
}

void AAHCharacter::ToggleSneak()
{
    if(bEnemy || !IsAlive()) return;
    bSneaking=!bSneaking;
    Feedback = bSneaking
        ? TEXT("Furtivo: passo curto, os acampamentos notam voce muito mais perto. Ataque daqui para emboscar.")
        : TEXT("De pe: passo normal.");
    AddLog(Feedback);
}

void AAHCharacter::Dash()
{
    if (!CanAct() || bAimMovementLocked || !Turn.SpendAction()) return;
    Turn.Movement += FMath::Max(0.f,BaseMovement-(FrostTurns>0?300.f:0.f));
    bDashing=true;
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()-FVector(0,0,60),EAHBurst::Guard);
    Feedback = FString::Printf(TEXT("Disparada: +%.1f m de movimento"),BaseMovement/100.f);
}

int32 AAHCharacter::SpellSaveDC() const
{
    return bEnemy ? 8 + UAHDiceRules::ProficiencyBonus(Level) + AHRules::Class(HeroClass).CastingModifier : SpellDC;
}

int32 AAHCharacter::SpellAttackBonus() const
{
    return bEnemy ? UAHDiceRules::ProficiencyBonus(Level) + AHRules::Class(HeroClass).CastingModifier : SpellAttack;
}

int32 AAHCharacter::ApplyHealing(int32 Amount)
{
    // Nothing brings back a character who already failed three saves or was
    // killed outright by massive damage.
    if (Amount<=0 || (DeathFailures>=3 && !bDowned)) return 0;

    const bool bWasDowned = bDowned;
    const int32 Healed = FMath::Min(Amount, MaxHealth-Health);
    Health += Healed;

    if (bWasDowned && Health>0)
    {
        // Any healing ends the dying state and wipes the tally (PHB 197).
        bDowned=false; bStabilized=false;
        DeathSuccesses=0; DeathFailures=0;
        GetWorldTimerManager().ClearTimer(DeathSaveTimer);
        GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        if (LocomotionClass) GetMesh()->SetAnimInstanceClass(LocomotionClass);
        AddLog(TEXT("De pe novamente!"));
    }
    return Healed;
}

void AAHCharacter::SecondWind()
{
    if(HeroClass!=EAHHeroClass::Fighter) return;
    if (!CanAct() || bSecondWindUsed || Health == MaxHealth || !Turn.SpendBonus()) return;
    bSecondWindUsed = true;
    const int32 Healing = ApplyHealing(Dice.RandRange(1, 10) + Level);   // 1d10 + level
    PlayGesture(HealAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()-FVector(0,0,45),EAHBurst::Heal);
    ImpactText = FString::Printf(TEXT("+%d PV"), Healing);
    ImpactTextTime = GetWorld()->GetTimeSeconds();
    bImpactHealing = true;
    bLastImpactCritical = false;
    Feedback = FString::Printf(TEXT("Segundo Fôlego: +%d PV"), Healing);
    AddLog(Feedback);
}

void AAHCharacter::CheckArcana()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    LastRoll      = UAHDiceRules::RollCheck(Dice, 1, 12, 0,
        IsLucky());
    LastRollLabel = TEXT("ARCANISMO / TESTE DE PERÍCIA");
    LastRollTime  = GetWorld()->GetTimeSeconds();
    PlayGesture(CastAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()+FVector(0,0,20),EAHBurst::Force);
    AddLog(FString::Printf(TEXT("Arcanismo: %d + 1 = %d / CD 12%s"),
        LastRoll.NaturalRoll, LastRoll.Total, *AHRollTag(LastRoll)));
}

FString AAHCharacter::ClassName(EAHHeroClass Choice)
{
    return AHRules::Class(Choice).Name;
}
void AAHCharacter::ApplySheet(EAHHeroClass Choice)
{
    HeroClass=Choice;
    const FAHClassSheet& Sheet=AHRules::Class(Choice);
    MaxHealth=Sheet.MaxHealth; ArmorClass=Sheet.ArmorClass; AttackBonus=Sheet.AttackBonus;
    DamageSides=Sheet.DamageSides; DamageModifier=Sheet.DamageModifier;
    InitiativeBonus=Sheet.InitiativeBonus; ClassCharges=Sheet.ClassCharges;
    /**
     * The ranged option, copied onto the character.
     *
     * It used to be read straight off the class table at the moment of the
     * shot, which said "a ranger can shoot because he is a ranger". Now it
     * says "because he is holding a bow" -- so these three are the fallback
     * for anybody with no kit, which is every foe in the world, and
     * RecomputeSheet overwrites them for anybody who has one.
     */
    RangedRange=Sheet.RangedRange; RangedSides=Sheet.RangedSides; RangedBonus=Sheet.RangedBonus;

    // Fighting Style. Archery and Duelling are read at the roll, because the
    // roll is the only place they mean anything; Defense is armour, so it has to
    // be folded into the number everything else reads.
    const FAHFightingStyle& Style=AHRules::Style(Choice);
    if(Style.Level>0 && Level>=Style.Level) ArmorClass+=Style.ArmorBonus;

    // Draconic Resilience: scales that are armour and blood that is thicker.
    // Level 1, so it belongs on the sheet rather than in the level-up path.
    if(Choice==EAHHeroClass::Sorcerer) { ArmorClass+=1; MaxHealth+=Level; }

    const FAHAncestrySheet& Blood=AHRules::Ancestry(Ancestry);
    InitiativeBonus+=Blood.InitiativeBonus;
    ArmorClass     +=Blood.ArmorBonus;
    MaxHealth      +=Blood.HealthPerLevel;
    DamageModifier +=Blood.DamageBonus;
    BaseMovement    =Blood.Movement;
    GetMesh()->SetRelativeScale3D(FVector(Blood.BodyScale));
}
void AAHCharacter::ChooseClass(EAHHeroClass Choice)
{
    if(bCharacterReady || bEnemy || static_cast<int32>(Choice)>=AHRules::ClassCount()) return;
    ApplySheet(Choice);
    Health=MaxHealth; bAncestrySelected=true; bCharacterReady=true;
    InitializeSpellbook();
    Equipment->Configure(AHRules::Class(HeroClass).Weapon);

    /**
     * And then the sheet takes over from the class table.
     *
     * ApplySheet above still runs and still writes the old constants -- it is
     * what every foe uses, and it is what this character falls back to if
     * anything here fails. RecomputeSheet then overwrites those numbers with
     * the ones his abilities and his kit actually earn. Two passes is one more
     * than necessary and it is the cheapest insurance there is: the character
     * is never in a half-built state.
     */
    if(!bAbilitiesChosen)
    {
        const int32* Suggested = AHSheet::Recommended(HeroClass);
        for(int32 A=0;A<static_cast<int32>(EAHAbility::Count);++A)
            Abilities.Score[A]=Suggested[A];
    }
    TakeKit(StartingKit < 0 ? 0 : StartingKit);
    UE_LOG(LogTemp, Display,
           TEXT("AH_FICHA %s: FOR %d DES %d CON %d INT %d SAB %d CAR %d -> CA %d, PV %d, ataque +%d, 1d%d%+d"),
           *ClassName(Choice),
           Abilities.Raw(EAHAbility::Forca), Abilities.Raw(EAHAbility::Destreza),
           Abilities.Raw(EAHAbility::Constituicao), Abilities.Raw(EAHAbility::Inteligencia),
           Abilities.Raw(EAHAbility::Sabedoria), Abilities.Raw(EAHAbility::Carisma),
           ArmorClass, MaxHealth, AttackBonus, DamageSides, DamageModifier);
    // The class's own colour, which is the one the HUD has always used for it,
    // so the figure on the ground matches the panel that describes him.
    PaintBody(AHRules::Class(HeroClass).Colour);
    Feedback=ClassName(Choice)+TEXT(" / nível 1");
}
void AAHCharacter::ChooseAncestry(EAHAncestry Choice)
{
    if(bCharacterReady || bEnemy || static_cast<int32>(Choice)>=AHRules::AncestryCount()) return;
    Ancestry=Choice; bAncestrySelected=true;
}
FString AAHCharacter::AncestryName(EAHAncestry Choice)
{
    return AHRules::Ancestry(Choice).Name;
}
FString AAHCharacter::AncestryTrait(EAHAncestry Choice)
{
    return AHRules::Ancestry(Choice).Trait;
}
FString AAHCharacter::ClassAbilityName() const
{
    if(HeroClass==EAHHeroClass::Paladin && Level==1) return TEXT("CURAR");
    if(HeroClass==EAHHeroClass::Ranger && Level==1) return TEXT("DISPARO");
    if(AHRules::Class(HeroClass).bCaster && MaxSpellSlots(1)>0) return AHSpells::Get(SelectedSpell).Name;
    return AHRules::Class(HeroClass).AbilityName;
}
FString AAHCharacter::ClassAbilityDescription() const
{
    if(HeroClass==EAHHeroClass::Paladin && Level==1) return TEXT("Imposicao das maos: acao, cura ate esgotar a reserva");
    if(HeroClass==EAHHeroClass::Ranger && Level==1) return TEXT("Arco longo: acao, ataque a distancia, alcance 36 m");
    if(AHRules::Class(HeroClass).bCaster) return AHSpells::Get(SelectedSpell).Description;
    return AHRules::Class(HeroClass).AbilityDescription;
}
bool AAHCharacter::CanUseClassAbility() const
{
    if(!CanAct() || bPreparingSpells) return false;
    if(HeroClass==EAHHeroClass::Paladin && Level==1) return Turn.bAction && LayOnHands>0 && Health<MaxHealth;
    if(HeroClass==EAHHeroClass::Ranger && Level==1) return Turn.bAction;
    if(HeroClass==EAHHeroClass::Rogue) return Level>=2 && Turn.bBonus;
    if(AHRules::Class(HeroClass).bCaster)
    {
        const auto& S=AHSpells::Get(SelectedSpell);
        return IsSpellAvailable(SelectedSpell) && (S.Rank==0 || (PreparedSpells.Contains(SelectedSpell) && SelectedSpellLevel>=S.Rank && HasSpellSlot())) &&
            (S.bBonus?Turn.bBonus && !bLeveledActionSpellCast:Turn.bAction && !(S.Rank>0 && bBonusSpellCast));
    }
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return Turn.bBonus && ClassCharges>0 && !bRaging;
    case EAHHeroClass::Cleric: return Turn.bAction && HasSpellSlot() && Health<MaxHealth;
    case EAHHeroClass::Wizard: return Turn.bAction && HasSpellSlot();
    default: return Turn.bBonus && !bSecondWindUsed && Health<MaxHealth;
    }
}
void AAHCharacter::UseClassAbility()
{
    if(HeroClass==EAHHeroClass::Rogue || (HeroClass==EAHHeroClass::Paladin && Level==1)) { UseClassUtility(); return; }
    if(HeroClass==EAHHeroClass::Ranger && Level==1)
    {
        AAHCharacter* Target=nullptr; float Best=3600.f;
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        { const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation()); if(It->bEnemy!=bEnemy && It->IsAlive() && D<Best) { Target=*It; Best=D; } }
        if(Target) TryRangedAttack(Target); return;
    }
    if(AHRules::Class(HeroClass).bCaster)
    {
        // Enemy archetypes also use the same resource and spell resolution path.
        if(PreparedSpells.IsEmpty() && bEnemy) InitializeSpellbook();
        AAHCharacter* Target=nullptr; float Best=AHSpells::Get(SelectedSpell).Range;
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        {
            const float Distance=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
            if(It->bEnemy==bEnemy || !It->IsAlive() || Distance>Best) continue;
            FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(SpellAutoTarget),false,this); Q.AddIgnoredActor(*It);
            if(GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Q)) continue;
            Target=*It; Best=Distance;
        }
        CastSpell(SelectedSpell,Target); return;
    }
    if(!CanUseClassAbility())
    {
        if(CanAct() && (HeroClass==EAHHeroClass::Fighter || HeroClass==EAHHeroClass::Cleric) && Health>=MaxHealth)
            Feedback=TEXT("Vida cheia: a cura não é necessária. Você pode mover ou atacar.");
        return;
    }
    if(HeroClass==EAHHeroClass::Fighter) { SecondWind(); return; }
    if(HeroClass==EAHHeroClass::Barbarian)
    {
        Turn.SpendBonus(); --ClassCharges; bRaging=true; RageTurns=10;
        TempHP = FMath::Max(TempHP, 5);  // Barbarians gain 5 temp HP when raging
        PlayGesture(RageAnimation);
        AAHCombatBurst::Emit(GetWorld(),GetActorLocation()-FVector(0,0,45),EAHBurst::Rage);
        Feedback=TEXT("Fúria ativa: +2 dano, resistência física e 5 PV temporários"); AddLog(Feedback); return;
    }

}
UAbilitySystemComponent* AAHCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }

// ── The character sheet ──────────────────────────────────────────────────────

void AAHCharacter::RecomputeSheet(bool bFull)
{
    /**
     * Foes are left alone, and that is the safety property this whole feature
     * rests on.
     *
     * Every enemy in the world is built by BecomeEnemy from the class table's
     * hand-tuned constants, and eighteen of them can be standing at once. If
     * the ability system reached them too, the day it landed would be the day
     * every fight in the game changed difficulty for reasons nobody could
     * point at. They get abilities when they get a kit, and not before.
     */
    if (bEnemy) return;

    const AHSheet::FAHDerived Made =
        AHSheet::Derive(HeroClass, Ancestry, Level, Abilities, Equipped);

    const int32 WasMax = FMath::Max(1, MaxHealth);
    MaxHealth       = Made.MaxHealth + AidBonus + (Feat==1?2*Level:0);
    ArmorClass      = Made.ArmorClass + (GuardTurns>0?2:0) + (ShieldTurns>0?5:0) + MageArmorBonus;
    AttackBonus     = Made.AttackBonus;
    DamageSides     = Made.DamageSides;
    DamageModifier  = Made.DamageModifier;
    InitiativeBonus = Made.InitiativeBonus + (Feat==2?5:0);
    BaseMovement    = Made.Movement + (Feat==3?300:0);
    RangedRange     = Made.RangedRange;
    RangedSides     = Made.RangedSides;
    // A bow's damage bonus is the same Dexterity the shot is aimed with.
    RangedBonus     = Made.RangedRange > 0 ? Made.DamageModifier : 0;
    SpellAttack     = Made.SpellAttack;
    SpellDC         = Made.SpellDC;
    bOverloaded     = Made.bOverloaded;

    // Draconic Resilience is a level-1 class feature rather than a piece of
    // equipment, so it survives the rewrite exactly where it was.

    // A wounded character keeps his wound. Changing armour must not heal you,
    // and a level up must not hand you a full bar either -- it hands you the
    // points it added.
    Health = bFull ? MaxHealth : Health<=0 ? 0
                   : FMath::Clamp(Health + (MaxHealth - WasMax), 1, MaxHealth);

    // The model in his hands follows the weapon in his hands.
    if (Equipment) Equipment->Configure(Made.Art);
}

FString AAHCharacter::Equip(const FString& Id)
{
    const FAHItemData* Thing = AHItems::Find(Id);
    if (!Thing || Thing->Slot == EAHSlot::Nenhum) return FString();
    const int32 Where = static_cast<int32>(Thing->Slot);
    const FString Came = Equipped[Where];
    Equipped[Where] = Id;

    /**
     * Two hands means two hands.
     *
     * A greatsword and a shield is the commonest illegal loadout there is,
     * and the honest place to refuse it is here rather than in the screen
     * that shows it -- a rule that only a button enforces is a rule the next
     * button forgets.
     */
    if (Thing->bDuasMaos && Thing->Slot == EAHSlot::MaoPrincipal)
    {
        const int32 OffHand = static_cast<int32>(EAHSlot::MaoSecundaria);
        if (!Equipped[OffHand].IsEmpty())
        {
            Carry(Equipped[OffHand]);
            Equipped[OffHand].Reset();
            AddLog(TEXT("Precisa das duas maos: o que estava na outra foi para a mochila."));
        }
    }
    if (Thing->Slot == EAHSlot::MaoSecundaria)
    {
        const FAHItemData* Main = AHItems::Find(Equipped[static_cast<int32>(EAHSlot::MaoPrincipal)]);
        if (Main && Main->bDuasMaos)
        {
            Equipped[Where] = Came;              // put it back: no room
            AddLog(TEXT("Sua arma ocupa as duas maos."));
            return Id;
        }
    }
    RecomputeSheet();
    return Came;
}

void AAHCharacter::Unequip(EAHSlot Slot)
{
    const auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>();
    if(bEnemy || !IsAlive() || IsBusy() || (Mode && !Mode->IsExploring())) return;
    const int32 Where = static_cast<int32>(Slot);
    if (Where < 0 || Where >= static_cast<int32>(EAHSlot::Count)) return;
    if (Equipped[Where].IsEmpty()) return;
    Carry(Equipped[Where]);
    Equipped[Where].Reset();
    RecomputeSheet();
}

void AAHCharacter::Carry(const FString& Id, int32 Many)
{
    if (Id.IsEmpty() || Many <= 0 || !AHItems::Find(Id)) return;
    for (FAHCarried& Held : Backpack)
        if (Held.Id == Id) { Held.Many += Many; return; }
    Backpack.Add({ Id, Many });
}

void AAHCharacter::TakeKit(int32 Which)
{
    StartingKit = FMath::Clamp(Which, 0, AHItems::KitsPerClass - 1);
    for (int32 Slot = 0; Slot < static_cast<int32>(EAHSlot::Count); ++Slot)
        Equipped[Slot].Reset();
    Backpack.Reset();

    const AHItems::FAHKit& Kit = AHItems::Kit(HeroClass, StartingKit);
    for (int32 I = 0; I < 8 && Kit.Itens[I]; ++I)
    {
        const FAHItemData* Thing = AHItems::Find(FString(Kit.Itens[I]));
        if (!Thing) continue;
        const int32 Where = static_cast<int32>(Thing->Slot);
        // Worn if its slot is free, carried otherwise -- which is how a kit
        // with two daggers puts one in your hand and one in the pack.
        if (Thing->Slot != EAHSlot::Nenhum && Equipped[Where].IsEmpty())
            Equipped[Where] = FString(Thing->Id);
        else
            Carry(FString(Thing->Id));
    }
    RecomputeSheet(/*bFull*/ true);
}

void AAHCharacter::PickClass(EAHHeroClass Choice)
{
    if(bCharacterReady || bEnemy || static_cast<int32>(Choice)>=AHRules::ClassCount()) return;
    HeroClass    = Choice;
    bClassPicked = true;
    // The suggested spread, until the player moves something. Changing your
    // mind about the class should change the suggestion with it -- and must
    // not throw away points you have already spent yourself.
    if(!bAbilitiesChosen)
    {
        const int32* Suggested = AHSheet::Recommended(HeroClass);
        for(int32 A=0;A<static_cast<int32>(EAHAbility::Count);++A)
            Abilities.Score[A]=Suggested[A];
    }
}

bool AAHCharacter::BuyAbility(EAHAbility Which, int32 Delta)
{
    if(bCharacterReady || bEnemy || Delta==0) return false;
    const int32 Slot = static_cast<int32>(Which);
    if(Slot<0 || Slot>=static_cast<int32>(EAHAbility::Count)) return false;

    const int32 Was = Abilities.Score[Slot];
    const int32 Want = Was + (Delta>0 ? 1 : -1);
    if(Want < AHSheet::BuyFloor || Want > AHSheet::BuyCeil) return false;

    Abilities.Score[Slot] = Want;
    // Refused rather than clamped: a screen that silently does nothing when
    // you are out of points is a screen you poke at; one where the button is
    // dark tells you why.
    if(AHSheet::Spent(Abilities) > AHSheet::BuyBudget)
    {
        Abilities.Score[Slot] = Was;
        return false;
    }
    bAbilitiesChosen = true;
    return true;
}

void AAHCharacter::BeginWith(int32 Kit)
{
    if(bCharacterReady || bEnemy || !bClassPicked || !bAncestrySelected || !bPointsDone || !AHSheet::Legal(Abilities)) return;
    StartingKit      = FMath::Clamp(Kit, 0, AHItems::KitsPerClass - 1);
    bAbilitiesChosen = true;
    ChooseClass(HeroClass);
}
