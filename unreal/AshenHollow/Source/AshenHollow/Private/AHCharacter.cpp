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
    HeroClass = static_cast<EAHHeroClass>(Pick.RandRange(0, AHRules::ClassCount()-1));
    Ancestry  = static_cast<EAHAncestry>(Pick.RandRange(0, AHRules::AncestryCount()-1));
    ApplySheet(HeroClass);
    InitializeSpellbook();

    // Foes are a little tougher than a level-1 hero so a duel lasts a few rounds.
    MaxHealth += 6;
    Health = MaxHealth;

    EnemyName = AHRules::Class(HeroClass).FoeName;

    GetCharacterMovement()->MaxWalkSpeed = 330.f;
    SpawnDefaultController();
    Equipment->Configure(AHRules::Class(HeroClass).Weapon);
}

// ── Turn management ───────────────────────────────────────────────────────────

void AAHCharacter::StartTurn()
{
    if(BlessTurns>0) --BlessTurns;
    if(GuardTurns>0 && --GuardTurns==0) ArmorClass-=2;
    bSneakUsed=false; bSteadyAim=false; bAimMovementLocked=false; MovementSpentThisTurn=0;
    if(MarkTurns>0 && --MarkTurns==0) MarkedTarget.Reset();
    ++TurnsStarted; bBonusSpellCast=false; bLeveledActionSpellCast=false;
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

    if (CanUseRacialAbility() && ToHero <= BreathReach)
    {
        AI->StopMovement();
        UseRacialAbility();
        return;
    }

    if (HasRangedAttack() && Turn.bAction && ToHero <= RangedReach())
    {
        AI->StopMovement();
        if (TryRangedAttack(Hero)) return;
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

    const bool GuidingAdvantage=Mover->HasGuidingMark();
    Mover->GuidingSource.Reset();
    OpportunityFlashTime = GetWorld()->GetTimeSeconds();
    SetActorRotation(FRotator(0, (Mover->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));

    FAHDiceOutcome Result = UAHDiceRules::RollAttack(
        Dice,
        AttackBonus+(BlessTurns>0?Dice.RandRange(1,4):0),
        Mover->ArmorClass,
        1,
        DamageSides,
        DamageModifier + (bRaging ? 2 : 0),
        ((Mover->bReckless || GuidingAdvantage)?1:0)-(Mover->bDodging?1:0),
        IsLucky());

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
    const int32 Index=static_cast<int32>(AHRules::Class(HeroClass).Weapon);
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
        AttackBonus+(BlessTurns>0?Dice.RandRange(1,4):0),
        Target->ArmorClass,
        1,
        DamageSides,
        DamageModifier + (bRaging?2:0),
        ((bSteadyAim || bReckless || Target->bReckless || Target->HasGuidingMark())?1:0)-(Target->bDodging?1:0),IsLucky());
    Target->GuidingSource.Reset(); bSteadyAim=false;
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
    const FAHClassSheet& Sheet = AHRules::Class(HeroClass);
    if (Sheet.RangedRange <= 0) return false;
    if (!CanAct() || !Turn.bAction) return false;
    if (!IsValid(Target) || !Target->IsAlive() || Target->bEnemy == bEnemy) return false;

    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > Sheet.RangedRange)
    {
        Feedback = FString::Printf(TEXT("Alvo além do alcance de %.0f m"), Sheet.RangedRange/100.f);
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
    PendingRoll = UAHDiceRules::RollAttack(
        Dice, AttackBonus+(BlessTurns>0?Dice.RandRange(1,4):0), Target->ArmorClass, 1,
        Sheet.RangedSides, Sheet.RangedBonus,
        ((bSteadyAim || bReckless || Target->bReckless || Target->HasGuidingMark()) ? 1 : 0) - ((Target->bDodging || bCrowded) ? 1 : 0),
        IsLucky());
    PendingTarget = Target;
    Target->GuidingSource.Reset(); bSteadyAim=false;
    // A little tolerance so the target drifting a step does not void the shot.
    PendingRange  = static_cast<float>(Sheet.RangedRange) + 80.f;
    bAttackedSinceTurnEnd = true;

    PlayAttack();
    Feedback = bCrowded
        ? FString::Printf(TEXT("%s com desvantagem: inimigo em corpo a corpo"), Sheet.RangedName)
        : FString::Printf(TEXT("%s..."), Sheet.RangedName);
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

// ── Other actions ─────────────────────────────────────────────────────────────

void AAHCharacter::ReceiveHit(int32 Damage, EAHDamageType Type, int32 RadiantBonus, bool bCriticalHit)
{
    if ((!IsAlive() && !bDowned) || Damage+RadiantBonus <= 0) return;
    const FAHAncestrySheet& Blood = AHRules::Ancestry(Ancestry);
    if (bRaging && Type == EAHDamageType::Physical) Damage/=2;
    if (Blood.bFireResistant && Type == EAHDamageType::Fire) Damage/=2;
    Damage+=RadiantBonus;
    if (Damage <= 0) return;
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
    return 8 + UAHDiceRules::ProficiencyBonus(Level) + AHRules::Class(HeroClass).CastingModifier;
}

int32 AAHCharacter::SpellAttackBonus() const
{
    return UAHDiceRules::ProficiencyBonus(Level) + AHRules::Class(HeroClass).CastingModifier;
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
