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
#include "AHGameMode.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

AAHCharacter::AAHCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    bUseControllerRotationYaw = false;

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));

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

// ── Enemy setup ──────────────────────────────────────────────────────────────

void AAHCharacter::BecomeEnemy()
{
    bEnemy = true; Health = MaxHealth = 18; ArmorClass = 13;
    AttackBonus=4; DamageSides=6; DamageModifier=2; InitiativeBonus=2;
    GetCharacterMovement()->MaxWalkSpeed = 330.f;
    SpawnDefaultController();
}

// ── Turn management ───────────────────────────────────────────────────────────

void AAHCharacter::StartTurn()
{
    Turn.Reset();
    if(bRaging && --RageTurns<=0) bRaging=false;
    bTurnActive  = true;  // Enemy preparation delay is handled by Tick, after activation.
    bDodging     = false;
    PreviousLocation = GetActorLocation();
    Feedback = bEnemy ? TEXT("Enemy turn") : TEXT("Your turn - choose an action");
}

void AAHCharacter::FinishTurn()
{
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
    if (!IsAlive()) return;

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

        // Fallback: if the AH_MeleeImpact notify was NOT added to the animation
        // yet, ImpactAt is still pending.  Fire it now rather than never.
        if (!bImpactResolved && ImpactAt > 0.f)
        {
            ResolveImpact();
        }
    }

    // ── Fallback impact timer (fires before animation end if timing allows) ───
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

    // Clamp walk speed to remaining budget so the character decelerates naturally.
    const float MaxSpeed = bEnemy ? 330.f : 480.f;
    GetCharacterMovement()->MaxWalkSpeed =
        (bTurnActive && !IsBusy())
        ? FMath::Min(MaxSpeed, Turn.Movement / FMath::Max(DeltaSeconds, .001f))
        : 0.f;

    // ── Enemy AI ──────────────────────────────────────────────────────────────
    if (!bEnemy || !CanAct() || !Turn.bAction || Now < NextThink) return;
    NextThink = Now + .2f;
    auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if (Mode && Now - Mode->TurnStarted < .65f) return;
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* AI   = Cast<AAIController>(GetController());
    if (!Hero || !Hero->IsAlive() || !AI) return;
    if (FVector::Dist2D(GetActorLocation(), Hero->GetActorLocation()) < 125.f)
    {
        AI->StopMovement();
        TryAttack(Hero);
    }
    else if (Turn.Movement > 1.f)
    {
        AI->MoveToActor(Hero, 5.f);
    }
}

// ── Attack ────────────────────────────────────────────────────────────────────

void AAHCharacter::PlayAttack()
{
    auto* Sequence = Cast<UAnimSequenceBase>((AttackAnimationIndex++ % 2 && AlternateAttackAnimation)
        ? AlternateAttackAnimation.Get() : AttackAnimation.Get());
    if (Sequence && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(
            Sequence, TEXT("DefaultSlot"), .14f, .22f, 1.f, 1);
    }

    const float Length = Sequence ? Sequence->GetPlayLength() : 0.9f;
    AnimationEnds   = GetWorld()->GetTimeSeconds() + FMath::Max(0.9f, Length);
    ImpactAt        = GetWorld()->GetTimeSeconds() + Length * ImpactFraction;
    // An authored impact notify owns timing. Only fall back at the end if it
    // fails to arrive; the generic 35% timer must not preempt a later notify.
    if (Sequence && Sequence->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
        { return Event.Notify && Event.Notify->IsA<UAHNotify_MeleeImpact>(); }))
        ImpactAt = AnimationEnds;
    bIsAttacking    = true;
    bImpactResolved = false;

}

bool AAHCharacter::TryAttack(AAHCharacter* Target)
{
    if (!CanAct() || !Turn.bAction || !IsValid(Target) || !Target->IsAlive() || Target->bEnemy == bEnemy)
        return false;
    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > 190.f)
    {
        Feedback = TEXT("Target is out of melee reach");
        return false;
    }
    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MeleeSight), false, this);
    Query.AddIgnoredActor(Target);
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
    {
        Feedback = TEXT("Target is obstructed");
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
        Target->bDodging ? -1 : 0);
    PendingTarget = Target;
    bAttackedSinceTurnEnd=true;

    PlayAttack();
    Feedback = TEXT("Resolving attack...");
    return true;
}

// ── Impact resolution ─────────────────────────────────────────────────────────

void AAHCharacter::OnMeleeImpactNotify()
{
    // Called by UAHNotify_MeleeImpact — frame-perfect path.
    if (bImpactResolved) return;   // guard against duplicate calls
    if (PendingTarget) UE_LOG(LogTemp, Display, TEXT("AH_CONTACT_NOTIFY %s"), bEnemy ? TEXT("ENEMY") : TEXT("HERO"));
    bImpactResolved = true;
    ImpactAt = -1.f;               // cancel the fallback timer
    ResolveImpact();
}

void AAHCharacter::ResolveImpact()
{
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
        { AddLog(TEXT("Misseis cancelados: alvo obstruido ou distante")); return; }
        Target->ReceiveHit(Damage,false);
        LastRollTime=-100.f;
        Feedback=FString::Printf(TEXT("Misseis Magicos: %d de dano de forca"),Damage);
        AddLog(Feedback);
        return;
    }

    auto Result = PendingRoll;

    // Cancel the attack if the target moved out of range or behind cover.
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
    Viewer->LastRollLabel = bEnemy ? TEXT("THORNBOUND / ATTACK") : TEXT("YOUR ATTACK");
    Viewer->LastRollTime  = GetWorld()->GetTimeSeconds();

    if (Result.bSuccess)
    {
        Target->ReceiveHit(Result.Damage);
        Result.Damage=Target->LastDamage;
        Viewer->LastRoll=Result;
    }
    Target->bImpactHealing = false;
    Target->bLastImpactCritical = Result.bCritical;
    Target->ImpactTextTime = GetWorld()->GetTimeSeconds();
    Target->ImpactText = Result.bSuccess
        ? FString::Printf(TEXT("%s-%d"), Result.bCritical ? TEXT("CRITICO  ") : TEXT(""), Target->LastDamage)
        : TEXT("ERROU");

    Viewer->AddLog(FString::Printf(TEXT("%s  %d + %d = %d  /  %s  %d dmg"),
        bEnemy ? TEXT("Foe") : TEXT("You"),
        Result.NaturalRoll, Result.Modifier, Result.Total,
        Result.bSuccess ? TEXT("HIT") : TEXT("MISS"),
        Result.Damage));
    Feedback = Result.bSuccess ? TEXT("Attack resolved") : TEXT("Attack missed");

    UE_LOG(LogTemp, Display,
        TEXT("AH_IMPACT %s d20=%d total=%d AC=%d hit=%d damage=%d"),
        bEnemy ? TEXT("ENEMY") : TEXT("HERO"),
        Result.NaturalRoll, Result.Total, Result.Target,
        Result.bSuccess, Result.Damage);
}

// ── Other actions ─────────────────────────────────────────────────────────────

void AAHCharacter::ReceiveHit(int32 Damage, bool bPhysical)
{
    if (!IsAlive() || Damage <= 0) return;
    if(bRaging && bPhysical) Damage/=2;
    bDamagedSinceTurnEnd=true;
    LastDamage     = FMath::Min(Health, Damage);
    LastDamageTime = GetWorld()->GetTimeSeconds();
    Health         = FMath::Max(0, Health - Damage);
    ImpactText = FString::Printf(TEXT("-%d"), LastDamage);
    ImpactTextTime = LastDamageTime;
    bImpactHealing = false;
    bLastImpactCritical = false;
    if (IsAlive())
    {
        // Cosmetic recoil does not displace the capsule or spend movement.
        // Do not interrupt a committed attack with an incoming reaction.
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

    FinishTurn();
    PendingTarget   = nullptr;
    ImpactAt        = -1.f;
    bImpactResolved = true;
    AnimationEnds   = 0.f;
    bIsAttacking    = false;
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation, false);
}

void AAHCharacter::Dodge()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    bDodging = true;
    Feedback = TEXT("Dodging until your next turn");
    AddLog(TEXT("Dodge: incoming attacks have disadvantage"));
}

void AAHCharacter::Dash()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    Turn.Movement += 900.f;
    Feedback = TEXT("Dash: +9 m movement");
}

void AAHCharacter::SecondWind()
{
    if(HeroClass!=EAHHeroClass::Fighter) return;
    if (!CanAct() || bSecondWindUsed || Health == MaxHealth || !Turn.SpendBonus()) return;
    bSecondWindUsed = true;
    const int32 Healing = FMath::Min(MaxHealth - Health, Dice.RandRange(1, 10) + 1);
    Health += Healing;
    ImpactText = FString::Printf(TEXT("+%d PV"), Healing);
    ImpactTextTime = GetWorld()->GetTimeSeconds();
    bImpactHealing = true;
    bLastImpactCritical = false;
    Feedback = FString::Printf(TEXT("Second Wind: +%d HP"), Healing);
    AddLog(Feedback);
}

void AAHCharacter::CheckArcana()
{
    if (!CanAct() || !Turn.SpendAction()) return;
    LastRoll      = UAHDiceRules::RollCheck(Dice, 1, 12);
    LastRollLabel = TEXT("ARCANA / ABILITY CHECK");
    LastRollTime  = GetWorld()->GetTimeSeconds();
    AddLog(FString::Printf(TEXT("Arcana: %d + 1 = %d / DC 12"),
        LastRoll.NaturalRoll, LastRoll.Total));
}

FString AAHCharacter::ClassName(EAHHeroClass Choice)
{
    switch(Choice) {
    case EAHHeroClass::Fighter: return TEXT("GUERREIRO");
    case EAHHeroClass::Barbarian: return TEXT("BARBARO");
    case EAHHeroClass::Cleric: return TEXT("CLERIGO");
    case EAHHeroClass::Wizard: return TEXT("MAGO");
    default: return TEXT("CLASSE"); }
}
void AAHCharacter::ChooseClass(EAHHeroClass Choice)
{
    if(bCharacterReady || bEnemy || static_cast<uint8>(Choice)>3) return;
    HeroClass=Choice; ClassCharges=2;
    switch(Choice) {
    case EAHHeroClass::Fighter: MaxHealth=12; ArmorClass=16; AttackBonus=5; DamageSides=8; DamageModifier=3; InitiativeBonus=1; ClassCharges=1; break;
    case EAHHeroClass::Barbarian: MaxHealth=14; ArmorClass=14; AttackBonus=5; DamageSides=12; DamageModifier=3; InitiativeBonus=2; break;
    case EAHHeroClass::Cleric: MaxHealth=10; ArmorClass=18; AttackBonus=4; DamageSides=6; DamageModifier=2; InitiativeBonus=0; break;
    case EAHHeroClass::Wizard: MaxHealth=8; ArmorClass=12; AttackBonus=2; DamageSides=6; DamageModifier=0; InitiativeBonus=2; break;
    }
    Health=MaxHealth; bCharacterReady=true;
    Feedback=ClassName(Choice)+TEXT(" / nivel 1");
}
FString AAHCharacter::ClassAbilityName() const
{
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return TEXT("FURIA");
    case EAHHeroClass::Cleric: return TEXT("CURAR");
    case EAHHeroClass::Wizard: return TEXT("MISSEIS");
    default: return TEXT("FOLEGO"); }
}
FString AAHCharacter::ClassAbilityDescription() const
{
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return TEXT("Furia / bonus / +2 dano corpo a corpo, resistencia fisica / 2 usos por encontro");
    case EAHHeroClass::Cleric: return TEXT("Curar Ferimentos / acao / cura propria 1d8+3 / 2 espacos por encontro");
    case EAHHeroClass::Wizard: return TEXT("Misseis Magicos / acao / 3 dardos de 1d4+1 / alvo mais proximo ate 18 m / 2 espacos");
    default: return TEXT("Segundo Folego / bonus / cura 1d10+1 / 1 uso por encontro"); }
}
bool AAHCharacter::CanUseClassAbility() const
{
    if(!CanAct()) return false;
    switch(HeroClass) {
    case EAHHeroClass::Barbarian: return Turn.bBonus && ClassCharges>0 && !bRaging;
    case EAHHeroClass::Cleric: return Turn.bAction && ClassCharges>0 && Health<MaxHealth;
    case EAHHeroClass::Wizard: return Turn.bAction && ClassCharges>0;
    default: return Turn.bBonus && !bSecondWindUsed && Health<MaxHealth;
    }
}
void AAHCharacter::UseClassAbility()
{
    if(!CanUseClassAbility()) return;
    if(HeroClass==EAHHeroClass::Fighter) { SecondWind(); return; }
    if(HeroClass==EAHHeroClass::Barbarian)
    {
        Turn.SpendBonus(); --ClassCharges; bRaging=true; RageTurns=10;
        Feedback=TEXT("Furia ativa: +2 dano e resistencia fisica"); AddLog(Feedback); return;
    }
    if(HeroClass==EAHHeroClass::Cleric)
    {
        Turn.SpendAction(); --ClassCharges;
        const int32 Amount=FMath::Min(MaxHealth-Health,Dice.RandRange(1,8)+3);
        Health+=Amount; ImpactText=FString::Printf(TEXT("+%d PV"),Amount);
        ImpactTextTime=GetWorld()->GetTimeSeconds(); bImpactHealing=true; bLastImpactCritical=false;
        Feedback=TEXT("Curar Ferimentos: ")+ImpactText; AddLog(Feedback); return;
    }
    AAHCharacter* Target=nullptr; float Best=1800.f;
    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
    {
        const float Distance=FVector::Dist(GetActorLocation(),It->GetActorLocation());
        if(!It->bEnemy || !It->IsAlive() || Distance>Best) continue;
        FHitResult Block; FCollisionQueryParams Sight(SCENE_QUERY_STAT(SpellSelection),false,this); Sight.AddIgnoredActor(*It);
        if(GetWorld()->LineTraceSingleByChannel(Block,GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Sight)) continue;
        Target=*It; Best=Distance;
    }
    if(!Target) { Feedback=TEXT("Nenhum alvo visivel ate 18 m"); return; }
    Turn.SpendAction(); --ClassCharges;
    if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0,(Target->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0));
    PendingTarget=Target; PendingSpellDamage=3*(Dice.RandRange(1,4)+1);
    PlayAttack(); Feedback=TEXT("Conjurando Misseis Magicos...");
}
UAbilitySystemComponent* AAHCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }
