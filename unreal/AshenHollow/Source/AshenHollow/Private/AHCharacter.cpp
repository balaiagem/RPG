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
#include "AHCombatBurst.h"
#include "AHGameMode.h"
#include "AHPlayerController.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"

AAHCharacter::AAHCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    bUseControllerRotationYaw = false;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    Equipment = CreateDefaultSubobject<UAHEquipmentComponent>(TEXT("Equipment"));

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
    WeaponAnimations={Sword.Object,Axe.Object,Mace.Object,Staff.Object};
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

// ── Enemy setup ──────────────────────────────────────────────────────────────

void AAHCharacter::BecomeEnemy()
{
    bEnemy = true;

    // Roll the foe's archetype so no two encounters open the same way.
    FRandomStream Pick; Pick.GenerateNewSeed();
    HeroClass = static_cast<EAHHeroClass>(Pick.RandRange(0, 3));
    Ancestry  = static_cast<EAHAncestry>(Pick.RandRange(0, 3));
    ApplySheet(HeroClass);

    // Foes are a little tougher than a level-1 hero so a duel lasts a few rounds.
    MaxHealth += 6;
    Health = MaxHealth;

    const TCHAR* Names[] = { TEXT("ESPADACHIM"), TEXT("SAQUEADOR"), TEXT("ORÁCULO"), TEXT("FEITICEIRO") };
    EnemyName = Names[FMath::Clamp(static_cast<int32>(HeroClass), 0, 3)];

    GetCharacterMovement()->MaxWalkSpeed = 330.f;
    SpawnDefaultController();
    Equipment->Configure(HeroClass);
}

// ── Turn management ───────────────────────────────────────────────────────────

void AAHCharacter::StartTurn()
{
    if(GuardTurns>0 && --GuardTurns==0) ArmorClass-=2;
    bReckless=false;
    TurnStartTime = GetWorld()->GetTimeSeconds();

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
    Turn.Movement=BaseMovement;
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
        !bEnemy && Ancestry==EAHAncestry::Halfling,SaveDiscarded,bSaveLucky);
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
        PendingTarget=nullptr; ImpactAt=-1.f; bImpactResolved=true; AnimationEnds=0.f; bIsAttacking=false;
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
        Turn.Travel(Distance);
        if (Turn.Movement <= 1.f)
        {
            if (GetController()) GetController()->StopMovement();
            GetCharacterMovement()->StopMovementImmediately();
        }
    }
    else
    {
        GetCharacterMovement()->StopMovementImmediately();
    }
    PreviousLocation = GetActorLocation();

    // Leaving a hostile's reach hands it a free swing (SRD 5.1 reaction).
    // This can knock us down, which ends the turn inside the call.
    UpdateThreatState();

    const float MaxSpeed = bEnemy ? 330.f : bDashing ? 650.f : 480.f;
    GetCharacterMovement()->MaxWalkSpeed =
        (bTurnActive && !IsBusy())
        ? FMath::Min(MaxSpeed, Turn.Movement / FMath::Max(DeltaSeconds, .001f))
        : 0.f;

    // ── Enemy AI ──────────────────────────────────────────────────────────────
    // Note: no !Turn.bAction guard here. A foe that has already attacked must
    // still be able to move, or it can never break away from melee.
    if (!bEnemy || !CanAct() || Now < NextThink) return;
    NextThink = Now + .2f;
    auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if (Mode && Now - Mode->TurnStarted < .65f) return;
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* AI   = Cast<AAIController>(GetController());
    if (!Hero || !Hero->IsAlive() || !AI) return;
    if (Now < RetreatUntil) return;

    const float ToHero = FVector::Dist2D(GetActorLocation(), Hero->GetActorLocation());

    // A wounded foe breaks away from melee once per encounter. It does not
    // Disengage, so it eats the hero's opportunity attack on the way out --
    // which is what makes the player's own reaction visible in a duel.
    // This is checked before the ability so the action stays unspent: the
    // GameMode ends a foe's turn 1.2 s after its action is gone, which would
    // cut the retreat short before it ever leaves the hero's reach.
    if (!bHasRetreated && MaxHealth > 0 && Health * 2 < MaxHealth
        && ToHero < ThreatReach && Turn.Movement > 300.f)
    {
        bHasRetreated = true;
        RetreatUntil  = Now + 3.f;
        const FVector Away = GetActorLocation()
            + (GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D() * 450.f;
        AI->MoveToLocation(Away, 25.f, false, true, true);
        Hero->AddLog(EnemyName + TEXT(" está ferido e recua do corpo a corpo."));
        return;
    }

    // Spend the archetype's ability when it is actually worth something, so the
    // rolled foe plays differently and not just with different numbers.
    if (CanUseClassAbility())
    {
        const bool bHurt  = Health * 2 < MaxHealth;
        const bool bWorth =
            HeroClass == EAHHeroClass::Barbarian ? ToHero < 600.f :
            HeroClass == EAHHeroClass::Wizard    ? true
                                                 : bHurt;   // Fighter and Cleric heal
        if (bWorth) { AI->StopMovement(); UseClassAbility(); return; }
    }

    if (ToHero < 125.f)
    {
        AI->StopMovement();
        if (Turn.bAction) TryAttack(Hero);
    }
    else if (Turn.Movement > 1.f)
    {
        AI->MoveToActor(Hero, 5.f);
    }
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

    OpportunityFlashTime = GetWorld()->GetTimeSeconds();
    SetActorRotation(FRotator(0, (Mover->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));

    FAHDiceOutcome Result = UAHDiceRules::RollAttack(
        Dice,
        AttackBonus,
        Mover->ArmorClass,
        1,
        DamageSides,
        DamageModifier + (bRaging ? 2 : 0),
        (Mover->bReckless?1:0)-(Mover->bDodging?1:0),
        !bEnemy && Ancestry == EAHAncestry::Halfling);

    // Cosmetic swing only. A reaction must never make the reacting character
    // busy, so bIsAttacking / AnimationEnds are deliberately left untouched.
    if (auto* Anim = GetMesh()->GetAnimInstance())
    {
        UAnimationAsset* Clip = WeaponAnimations.IsValidIndex(static_cast<int32>(HeroClass))
            ? WeaponAnimations[static_cast<int32>(HeroClass)].Get()
            : AttackAnimation.Get();
        if (auto* Sequence = Cast<UAnimSequenceBase>(Clip))
        {
            Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
            Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), .08f, .18f, 1.35f, 1);
            ReactionEnds = GetWorld()->GetTimeSeconds() + Sequence->GetPlayLength() / 1.35f;
        }
    }

    if (Result.bSuccess)
    {
        Mover->ReceiveHit(Result.Damage);
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

void AAHCharacter::PlayAttack()
{
    MotionLabel=PendingSpellDamage>0?TEXT("CONJURANDO"):TEXT("ATACANDO");
    UAnimationAsset* Clip=PendingSpellDamage>0?CastAnimation.Get():WeaponAnimations[static_cast<uint8>(HeroClass)].Get();
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
    if(PendingSpellDamage>0 && PendingTarget && GetNetMode()!=NM_DedicatedServer)
    {
        float Travel=Length*ImpactFraction;
        if(Sequence)
            for(const FAnimNotifyEvent& Event:Sequence->Notifies)
                if(Event.Notify && Event.Notify->IsA<UAHNotify_MeleeImpact>())
                { Travel=Event.GetTriggerTime(); break; }
        MagicVisual=GetWorld()->SpawnActor<AAHMagicVisual>();
        if(MagicVisual) MagicVisual->Initialize(GetMesh()->GetSocketLocation(TEXT("hand_r")),PendingTarget,Travel);
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

bool AAHCharacter::TryAttack(AAHCharacter* Target)
{
    if (!CanAct() || !Turn.bAction || !IsValid(Target) || !Target->IsAlive() || Target->bEnemy == bEnemy)
        return false;
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

    Turn.SpendAction();
    if (GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));

    PendingRoll = UAHDiceRules::RollAttack(
        Dice,
        AttackBonus,
        Target->ArmorClass,
        1,
        DamageSides,
        DamageModifier + (bRaging?2:0),
        ((bReckless || Target->bReckless)?1:0)-(Target->bDodging?1:0),!bEnemy && Ancestry==EAHAncestry::Halfling);
    PendingTarget = Target;
    bAttackedSinceTurnEnd=true;

    PlayAttack();
    Feedback = TEXT("Resolvendo o ataque...");
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

    auto* Target = PendingTarget.Get();
    PendingTarget  = nullptr;
    const int32 SpellDamage=PendingSpellDamage; PendingSpellDamage=0;
    if (!IsAlive() || !IsValid(Target) || !Target->IsAlive()) return;

    if(SpellDamage>0)
    {
        const int32 Damage=SpellDamage;
        FHitResult Block;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(SpellSight),false,this); Sight.AddIgnoredActor(Target);
        if(FVector::Dist(GetActorLocation(),Target->GetActorLocation())>1800.f ||
           GetWorld()->LineTraceSingleByChannel(Block,GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Sight))
        { AddLog(TEXT("Mísseis cancelados: alvo obstruído ou distante")); return; }
        Target->ReceiveHit(Damage,false);
        LastRollTime=-100.f;
        Feedback=FString::Printf(TEXT("Mísseis Mágicos: %d de dano de força"),Damage);
        AddLog(Feedback);
        return;
    }

    auto Result = PendingRoll;

    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ImpactSight), false, this);
    Query.AddIgnoredActor(Target);
    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > 210.f
        || GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
    {
        Result.bSuccess  = false;
        Result.bCritical = false;
        Result.Damage    = 0;
    }

    auto* Viewer = bEnemy ? Target : this;
    Viewer->LastRoll      = Result;
    Viewer->LastRollLabel = bEnemy ? EnemyName + TEXT(" / ATAQUE") : FString(TEXT("SEU ATAQUE"));
    Viewer->LastRollTime  = GetWorld()->GetTimeSeconds();

    if (Result.bSuccess)
    {
        Target->ReceiveHit(Result.Damage);
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

// ── Other actions ─────────────────────────────────────────────────────────────

void AAHCharacter::ReceiveHit(int32 Damage, bool bPhysical)
{
    if ((!IsAlive() && !bDowned) || Damage <= 0) return;
    if(bRaging && bPhysical) Damage/=2;
    if(GuardTurns>0 && (Damage>=Health+TempHP || Dice.RandRange(1,20)+2<FMath::Max(10,Damage/2)))
    { GuardTurns=0; ArmorClass-=2; AddLog(TEXT("Concentracao encerrada")); }
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
            else AAHCombatBurst::Emit(GetWorld(),HitOrigin,bPhysical?EAHBurst::Steel:EAHBurst::Force);
            return;
        }
    }

    // ── Apply to real HP ──────────────────────────────────────────────────────
    LastDamage = FMath::Min(Health, Damage);
    Health     = FMath::Max(0, Health - Damage);
    ImpactText = FString::Printf(TEXT("-%d"), LastDamage);
    ImpactTextTime = LastDamageTime;
    bImpactHealing = false;
    bLastImpactCritical = false;
    const FVector HitOrigin=GetActorLocation()+FVector(0,0,35);
    if(HitFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),HitFX,HitOrigin,GetActorRotation());
    else AAHCombatBurst::Emit(GetWorld(),HitOrigin,bPhysical?EAHBurst::Steel:EAHBurst::Force);

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
        AddLog(TEXT("Nocauteado! Salvaguardas de morte a seguir."));
        Feedback = TEXT("Nocauteado - role salvaguardas de morte");
    }
    else
    {
        // Hit while already downed = extra failure
        ++DeathFailures;
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

void AAHCharacter::Dash()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    Turn.Movement += BaseMovement;
    bDashing=true;
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()-FVector(0,0,60),EAHBurst::Guard);
    Feedback = FString::Printf(TEXT("Disparada: +%.1f m de movimento"),BaseMovement/100.f);
}

void AAHCharacter::SecondWind()
{
    if(HeroClass!=EAHHeroClass::Fighter) return;
    if (!CanAct() || bSecondWindUsed || Health == MaxHealth || !Turn.SpendBonus()) return;
    bSecondWindUsed = true;
    const int32 Healing = FMath::Min(MaxHealth - Health, Dice.RandRange(1, 10) + 1);
    Health += Healing;
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
        !bEnemy && Ancestry == EAHAncestry::Halfling);
    LastRollLabel = TEXT("ARCANISMO / TESTE DE PERÍCIA");
    LastRollTime  = GetWorld()->GetTimeSeconds();
    PlayGesture(CastAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()+FVector(0,0,20),EAHBurst::Force);
    AddLog(FString::Printf(TEXT("Arcanismo: %d + 1 = %d / CD 12%s"),
        LastRoll.NaturalRoll, LastRoll.Total, *AHRollTag(LastRoll)));
}

FString AAHCharacter::ClassName(EAHHeroClass Choice)
{
    switch(Choice) {
    case EAHHeroClass::Fighter: return TEXT("GUERREIRO");
    case EAHHeroClass::Barbarian: return TEXT("BÁRBARO");
    case EAHHeroClass::Cleric: return TEXT("CLÉRIGO");
    case EAHHeroClass::Wizard: return TEXT("MAGO");
    default: return TEXT("CLASSE"); }
}
void AAHCharacter::ApplySheet(EAHHeroClass Choice)
{
    HeroClass=Choice; ClassCharges=2;
    switch(Choice) {
    case EAHHeroClass::Fighter: MaxHealth=12; ArmorClass=16; AttackBonus=5; DamageSides=8; DamageModifier=3; InitiativeBonus=1; ClassCharges=1; break;
    case EAHHeroClass::Barbarian: MaxHealth=14; ArmorClass=14; AttackBonus=5; DamageSides=12; DamageModifier=3; InitiativeBonus=2; break;
    case EAHHeroClass::Cleric: MaxHealth=10; ArmorClass=18; AttackBonus=4; DamageSides=6; DamageModifier=2; InitiativeBonus=0; break;
    case EAHHeroClass::Wizard: MaxHealth=8; ArmorClass=12; AttackBonus=2; DamageSides=6; DamageModifier=0; InitiativeBonus=2; break;
    }
    if(Ancestry==EAHAncestry::Human) ++InitiativeBonus;
    if(Ancestry==EAHAncestry::Elf) ++ArmorClass;
    if(Ancestry==EAHAncestry::Dwarf) ++MaxHealth;
    BaseMovement=(Ancestry==EAHAncestry::Dwarf || Ancestry==EAHAncestry::Halfling)?750.f:900.f;
    const float BodyScale=Ancestry==EAHAncestry::Dwarf?.82f:Ancestry==EAHAncestry::Halfling?.65f:1.f;
    GetMesh()->SetRelativeScale3D(FVector(BodyScale));
}
void AAHCharacter::ChooseClass(EAHHeroClass Choice)
{
    if(bCharacterReady || bEnemy || static_cast<uint8>(Choice)>3) return;
    ApplySheet(Choice);
    Health=MaxHealth; bAncestrySelected=true; bCharacterReady=true;
    Equipment->Configure(Choice);
    Feedback=ClassName(Choice)+TEXT(" / nível 1");
}
void AAHCharacter::ChooseAncestry(EAHAncestry Choice)
{
    if(bCharacterReady || bEnemy || static_cast<uint8>(Choice)>3) return;
    Ancestry=Choice; bAncestrySelected=true;
}
FString AAHCharacter::AncestryName(EAHAncestry Choice)
{
    const TCHAR* Names[]={TEXT("HUMANO"),TEXT("ELFO"),TEXT("ANÃO"),TEXT("HALFLING")};
    return Names[FMath::Clamp(static_cast<int32>(Choice),0,3)];
}
FString AAHCharacter::AncestryTrait(EAHAncestry Choice)
{
    const TCHAR* Traits[]={TEXT("Versatilidade: +1 iniciativa | 9 m"),TEXT("Agilidade: +1 CA | 9 m"),TEXT("Tenacidade: +1 PV | 7,5 m"),TEXT("Sorte: rerrola 1 natural uma vez | 7,5 m")};
    return Traits[FMath::Clamp(static_cast<int32>(Choice),0,3)];
}
FString AAHCharacter::ClassAbilityName() const
{
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return TEXT("FÚRIA");
    case EAHHeroClass::Cleric: return TEXT("CURAR");
    case EAHHeroClass::Wizard: return TEXT("MÍSSEIS");
    default: return TEXT("FÔLEGO"); }
}
FString AAHCharacter::ClassAbilityDescription() const
{
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return TEXT("Fúria / bônus / +2 dano corpo a corpo, resistência física / 2 usos por encontro");
    case EAHHeroClass::Cleric: return TEXT("Curar Ferimentos / ação / cura própria 1d8+3 / 2 espaços por encontro");
    case EAHHeroClass::Wizard: return TEXT("Mísseis Mágicos / ação / 3 dardos de 1d4+1 / alvo mais próximo até 18 m / 2 espaços");
    default: return TEXT("Segundo Fôlego / bônus / cura 1d10+1 / 1 uso por encontro"); }
}
bool AAHCharacter::CanUseClassAbility() const
{
    if(!CanAct()) return false;
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return Turn.bBonus && ClassCharges>0 && !bRaging;
    case EAHHeroClass::Cleric: return Turn.bAction && HasSpellSlot() && Health<MaxHealth;
    case EAHHeroClass::Wizard: return Turn.bAction && HasSpellSlot();
    default: return Turn.bBonus && !bSecondWindUsed && Health<MaxHealth;
    }
}
void AAHCharacter::UseClassAbility()
{
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
    if(HeroClass==EAHHeroClass::Cleric)
    {
        Turn.SpendAction(); SpendSpellSlot();
        int32 Healing=3; for(int32 I=0;I<SelectedSpellLevel;++I) Healing+=Dice.RandRange(1,8);
        const int32 Amount=FMath::Min(MaxHealth-Health,Healing);
        Health+=Amount; ImpactText=FString::Printf(TEXT("+%d PV"),Amount);
        PlayGesture(HealAnimation);
        AAHCombatBurst::Emit(GetWorld(),GetActorLocation()-FVector(0,0,45),EAHBurst::Heal);
        ImpactTextTime=GetWorld()->GetTimeSeconds(); bImpactHealing=true; bLastImpactCritical=false;
        Feedback=TEXT("Curar Ferimentos: ")+ImpactText; AddLog(Feedback); return;
    }
    AAHCharacter* Target=nullptr; float Best=1800.f;
    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
    {
        const float Distance=FVector::Dist(GetActorLocation(),It->GetActorLocation());
        if(*It==this || It->bEnemy==bEnemy || !It->IsAlive() || Distance>Best) continue;
        FHitResult Block; FCollisionQueryParams Sight(SCENE_QUERY_STAT(SpellSelection),false,this); Sight.AddIgnoredActor(*It);
        if(GetWorld()->LineTraceSingleByChannel(Block,GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Sight)) continue;
        Target=*It; Best=Distance;
    }
    if(!Target) { Feedback=TEXT("Nenhum alvo visível até 18 m"); return; }
    Turn.SpendAction(); SpendSpellSlot();
    if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0,(Target->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0));
    PendingTarget=Target; PendingSpellDamage=(2+SelectedSpellLevel)*(Dice.RandRange(1,4)+1);
    PlayAttack(); Feedback=TEXT("Conjurando Mísseis Mágicos...");
}
UAbilitySystemComponent* AAHCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }
