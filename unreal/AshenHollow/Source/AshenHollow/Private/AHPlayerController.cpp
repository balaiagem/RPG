#include "AHPlayerController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AITypes.h"
#include "DrawDebugHelpers.h"
#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHVillager.h"
#include "AHCombatHUD.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"

AAHPlayerController::AAHPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    bShouldPerformFullTickWhenPaused=true;
    PathFollowing = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("PathFollowing"));
}

void AAHPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ApplyPerformance();
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
}

void AAHPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    auto* Input = CastChecked<UEnhancedInputComponent>(InputComponent);
    Mapping = NewObject<UInputMappingContext>(this);
    MoveAction = NewObject<UInputAction>(this);
    StopAction = NewObject<UInputAction>(this);
    MoveAction->ValueType = EInputActionValueType::Boolean;
    StopAction->ValueType = EInputActionValueType::Boolean;
    Mapping->MapKey(MoveAction, EKeys::RightMouseButton);
    Mapping->MapKey(StopAction, EKeys::SpaceBar);
    Input->BindAction(MoveAction, ETriggerEvent::Started,   this, &AAHPlayerController::MoveToCursor);
    Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAHPlayerController::HoldMove);
    Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AAHPlayerController::ReleaseMove);
    Input->BindAction(MoveAction, ETriggerEvent::Canceled,  this, &AAHPlayerController::ReleaseMove);
    Input->BindAction(StopAction, ETriggerEvent::Started, this, &AAHPlayerController::Stop);
    auto Bind = [this, Input](FKey Key, void (AAHPlayerController::*Handler)())
    {
        auto* Action = NewObject<UInputAction>(Mapping);
        Action->ValueType = EInputActionValueType::Boolean;
        Action->bTriggerWhenPaused=(Key==EKeys::Y || Key==EKeys::N || Key==EKeys::P || Key==EKeys::J || Key==EKeys::LeftMouseButton);
        // The left button is shared with the HUD's own hit boxes, which is how a
        // class and an ancestry get chosen. Not swallowing it costs nothing and
        // removes the only way this could break the menus.
        Action->bConsumeInput = (Key != EKeys::LeftMouseButton);
        Mapping->MapKey(Action, Key);
        Input->BindAction(Action, ETriggerEvent::Started, this, Handler);
    };
    Bind(EKeys::LeftMouseButton, &AAHPlayerController::SelectTarget);
    // Two keys for the same thing on purpose. Tab is what anyone raised on a
    // party RPG reaches for, and F is there in case Slate eats Tab for focus.
    Bind(EKeys::Tab, &AAHPlayerController::CycleTarget);
    Bind(EKeys::F,   &AAHPlayerController::CycleTarget);
    Bind(EKeys::Z, &AAHPlayerController::ToggleSneak);
    Bind(EKeys::Q, &AAHPlayerController::AttackNearest);
    Bind(EKeys::E, &AAHPlayerController::Heal);
    Bind(EKeys::K, &AAHPlayerController::ToggleSpellbook);
    Bind(EKeys::C, &AAHPlayerController::Check);
    Bind(EKeys::Y, &AAHPlayerController::AcceptReaction);
    Bind(EKeys::N, &AAHPlayerController::DeclineReaction);
    Bind(EKeys::F5, &AAHPlayerController::Restart);
    Bind(EKeys::Enter, &AAHPlayerController::EndTurn);
    Bind(EKeys::R, &AAHPlayerController::Dash);
    Bind(EKeys::X, &AAHPlayerController::DisengageAction);
    Bind(EKeys::T, &AAHPlayerController::BreathAction);
    // G for "gente": talking to somebody. E is already the heal and F is the
    // target cycle, so the obvious two keys were both spoken for.
    Bind(EKeys::G, &AAHPlayerController::Falar);
    // P de personagem: a ficha e o que voce esta carregando.
    Bind(EKeys::P, &AAHPlayerController::ToggleSheet);
    Bind(EKeys::J, &AAHPlayerController::ToggleJournal);
    Bind(EKeys::F6, &AAHPlayerController::CyclePerformance);
    Bind(EKeys::A, &AAHPlayerController::RotateCameraLeft);
    Bind(EKeys::D, &AAHPlayerController::RotateCameraRight);
    Bind(EKeys::MouseScrollUp,   &AAHPlayerController::ZoomCameraIn);
    Bind(EKeys::MouseScrollDown, &AAHPlayerController::ZoomCameraOut);
    if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(Mapping, 0);
    }
}

void AAHPlayerController::SelectTarget()
{
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero) return;

    // Logged, every time. Two rounds went by with "picking a target does
    // nothing" and no way to tell which of five links in the chain was broken.
    // Now the log says which one, and it costs one line per click.
    if (auto* HUD = Cast<AAHCombatHUD>(GetHUD()); HUD && HUD->IsPointerOverInterface())
    {
        UE_LOG(LogTemp, Display, TEXT("AH_PICK ignorado: cursor sobre a interface"));
        return;
    }

    // Spelled out rather than auto*: deducing a pointer from a ternary whose
    // other arm is nullptr is exactly the shape that cost this project ten
    // cascading errors once already.
    FHitResult Hit;
    AAHCharacter* Picked = GetHitResultUnderCursor(ECC_Visibility, false, Hit)
                         ? Cast<AAHCharacter>(Hit.GetActor()) : nullptr;
    UE_LOG(LogTemp, Display, TEXT("AH_PICK clique: ator=%s inimigo=%d"),
           Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nenhum"),
           (Picked && Picked->bEnemy) ? 1 : 0);

    if (Picked && Picked->bEnemy && Picked->IsAlive())
    {
        ChosenTarget = Picked;
        Hero->Feedback = FString::Printf(TEXT("Alvo travado: %s"), *Picked->EnemyName);
        Hero->AddLog(Hero->Feedback);
    }
    else if (ChosenTarget)
    {
        // Only say so when there was something to let go of. Clicking the ground
        // is the commonest thing a player does; it should not narrate itself.
        ChosenTarget = nullptr;
        Hero->Feedback = TEXT("Alvo liberado");
    }
}

void AAHPlayerController::CycleTarget()
{
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->IsAlive()) return;

    // Sorted by distance, so pressing it again and again walks outwards through
    // the fight in an order the player can predict.
    TArray<AAHCharacter*> Foes;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
        if (It->bEnemy && It->IsAlive()) Foes.Add(*It);
    if (Foes.Num() == 0) { Hero->Feedback = TEXT("Nenhum inimigo a vista"); return; }

    const FVector Here = Hero->GetActorLocation();
    Foes.Sort([&Here](const AAHCharacter& A, const AAHCharacter& B)
    {
        return FVector::DistSquared2D(Here, A.GetActorLocation())
             < FVector::DistSquared2D(Here, B.GetActorLocation());
    });

    int32 Next = 0;
    if (IsValid(ChosenTarget))
    {
        const int32 Current = Foes.IndexOfByKey(ChosenTarget.Get());
        if (Current != INDEX_NONE) Next = (Current + 1) % Foes.Num();
    }
    ChosenTarget = Foes[Next];
    Hero->Feedback = FString::Printf(TEXT("Alvo travado: %s  (%d de %d, %.0f m)"),
        *ChosenTarget->EnemyName, Next + 1, Foes.Num(),
        FVector::Dist2D(Here, ChosenTarget->GetActorLocation()) / 100.f);
    Hero->AddLog(Hero->Feedback);
    UE_LOG(LogTemp, Display, TEXT("AH_PICK tecla: %s"), *ChosenTarget->EnemyName);
}

AAHCharacter* AAHPlayerController::CurrentTarget() const
{
    if (IsValid(ChosenTarget) && ChosenTarget->IsAlive()) return ChosenTarget.Get();
    if (IsValid(HoveredEnemy) && HoveredEnemy->IsAlive()) return HoveredEnemy.Get();

    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero) return nullptr;
    // Bounded on purpose. Falling back to "the nearest enemy anywhere" would
    // reach a sleeping camp across the valley, and thirty metres is already
    // further than any spell in the book.
    AAHCharacter* Nearest = nullptr;
    float Best = 3000.f;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->bEnemy || !It->IsAlive()) continue;
        const float Distance = FVector::Dist2D(Hero->GetActorLocation(), It->GetActorLocation());
        if (Distance < Best) { Best = Distance; Nearest = *It; }
    }
    return Nearest;
}

void AAHPlayerController::MoveToCursor()
{
    FHitResult Hit;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->IsAlive() || !Hero->bTurnActive) return;
    if (auto* HUD = Cast<AAHCombatHUD>(GetHUD()); HUD && HUD->IsPointerOverInterface()) return;
    FVector RayStart, RayDirection;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MoveCursor),false,Hero);
    // Movement can click through foliage without removing trunks from combat
    // visibility or physical collision.
    if(!DeprojectMousePositionToWorld(RayStart,RayDirection) || !GetWorld()->LineTraceSingleByChannel(Hit,RayStart,RayStart+RayDirection*100000.f,ECC_GameTraceChannel1,Query)) { Hero->Feedback=TEXT("Clique em uma superfície do cenário"); return; }
    /**
     * Clicking on somebody who lives here walks up to him and talks, and who
     * was clicked is asked TWO ways on purpose.
     *
     * The same click that attacks a bandit, because the player should not have
     * to know which kind of person he is pointing at -- the game knows. Inside
     * three and a half metres it is a conversation; further away it is a walk
     * that ends in one.
     *
     * The movement trace runs on GameTraceChannel1 -- the channel the trees
     * ignore, so you can click through foliage -- and whether a Pawn capsule
     * answers a custom channel depends on the Pawn profile's response to it,
     * which is a line in an ini file and not a promise. If the capsule does not
     * answer, the ray lands on the grass BEHIND the person, and clicking
     * straight at a villager would quietly do nothing at all. So the actor is
     * asked first and, failing that, whoever is standing within two metres of
     * where the ray actually landed.
     */
    auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    AAHVillager* Who = Cast<AAHVillager>(Hit.GetActor());
    if (!IsValid(Who) && Mode)
    {
        float Nearest = 220.f;
        for (const TObjectPtr<AAHVillager>& Body : Mode->Folk)
        {
            if (!IsValid(Body)) continue;
            const float Gap = FVector::Dist2D(Hit.ImpactPoint, Body->GetActorLocation());
            if (Gap < Nearest) { Nearest = Gap; Who = Body.Get(); }
        }
    }
    if (IsValid(Who))
    {
        const float Away = FVector::Dist2D(Hero->GetActorLocation(), Who->GetActorLocation());
        if (Mode && Away < 360.f) { Mode->TalkToNearest(); return; }
        // Stop a little short of him: walking INTO somebody to talk to him ends
        // with two capsules pushing each other around.
        const FVector Toward = (Hero->GetActorLocation() - Who->GetActorLocation()).GetSafeNormal2D();
        bTalkOnArrival = true;
        RequestMoveToLocation(Who->GetActorLocation() + Toward * 180.f);
        return;
    }

    if (auto* Enemy = Cast<AAHCharacter>(Hit.GetActor()); Enemy && Enemy->bEnemy && Enemy->IsAlive())
    {
        bQueuedMove=false;
        bTalkOnArrival = false;
        // Attacking somebody is also choosing them, so the next spell goes to
        // the same foe without a second click.
        ChosenTarget = Enemy;
        // Outside a fight, picking a foe starts one rather than swinging. The
        // player joins the turn order and then acts, which is what "combat
        // begins" has to mean in a game where the order decides everything.
        // Mode was already fetched above, for the villager test. Fetching it
        // again here would be a local hiding a local, which this toolchain
        // treats as an error (C4456) and has cost this project a build already.
        if (Mode && Mode->IsExploring())
        {
            if (Mode->EngageWith(Enemy)) { AttackTarget = nullptr; return; }
        }
        if(Hero->IsBusy()) { Hero->Feedback=TEXT("Aguarde o fim da ação para atacar"); return; }
        if (!Hero->Turn.bAction) { Hero->Feedback=TEXT("Ação já utilizada neste turno"); return; }
        // A shooter fires from where it stands rather than walking into reach.
        if (Hero->HasRangedAttack() && Hero->TryRangedAttack(Enemy)) { AttackTarget=nullptr; return; }
        AttackTarget = Enemy;
        UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Enemy);
        return;
    }
    bTalkOnArrival = false;
    RequestMoveToLocation(Hit.ImpactPoint);
}

void AAHPlayerController::HoldMove()
{
    bHoldingMove = true;
}

void AAHPlayerController::ReleaseMove()
{
    bHoldingMove = false;
    NextHoldPath = 0.f;
}

/**
 * Talks to whoever is standing near, on a key.
 *
 * On a key as well as on a click for exactly the reason CycleTarget exists: a
 * click has to survive Slate, the HUD's hit boxes and the input stack, and when
 * that chain broke before, the answer was "clicking does nothing" with no way
 * to tell where it broke. A key binding travels the same road as Q and E.
 */
void AAHPlayerController::ToggleSheet()
{
    const auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(IsReactionPending() || !Hero || !Hero->bCharacterReady || !Hero->IsAlive()) return;
    bSheetOpen = !bSheetOpen;
    bQuestJournal=false;
    bProgressionSheet=false;
    bHoldingMove=false; bQueuedMove=false; bWalkingBlind=false; bTalkOnArrival=false;
    StopMovement(); AttackTarget=nullptr;
    bSpellbookOpen=false;
    SetPause(bSheetOpen);
}

void AAHPlayerController::ToggleJournal()
{
    if(!bSheetOpen) { ToggleSheet(); if(bSheetOpen) bQuestJournal=true; }
    else bQuestJournal=!bQuestJournal;
}

void AAHPlayerController::Falar()
{
    if(auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>())
        if(Mode->TakeNearbyLoot(Cast<AAHCharacter>(GetPawn()))) return;
    if (auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode())) Mode->TalkToNearest();
}

bool AAHPlayerController::RequestMoveToLocation(const FVector& Location)
{
    // Every way out of this function says why, out loud.
    //
    // Two of them used to be silent -- no pawn or no active turn, and no
    // navigable destination is only written to the HUD -- and "the character
    // just stands there" is the same symptom for all of them. A refusal nobody
    // can read costs a whole build cycle to tell apart from the next one.
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero || !Hero->IsAlive() || !Hero->bTurnActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("AH_MOVE recusado: heroi=%d vivo=%d turno=%d"),
               Hero ? 1 : 0, (Hero && Hero->IsAlive()) ? 1 : 0, (Hero && Hero->bTurnActive) ? 1 : 0);
        bQueuedMove=false; return false;
    }
    if(Hero->Turn.Movement<=1.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("AH_MOVE recusado: sem movimento (%.0f)"), Hero->Turn.Movement);
        bQueuedMove=false; Hero->Feedback=TEXT("Sem movimento restante"); return false;
    }
    AttackTarget=nullptr;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Destination;
    if (!Nav || !Nav->ProjectPointToNavigation(Location, Destination, FVector(80, 80, 150)))
    {
        if(!bQueuedMove || FVector::Dist2D(QueuedMove,Location)>65)
            QueuedMoveExpires=GetWorld()->GetTimeSeconds()+3.f;
        QueuedMove=Location; bQueuedMove=true; bWalkingBlind=false;
        Hero->Feedback=TEXT("Preparando caminho...");
        return false;
    }
    if(Hero->IsBusy())
    {
        bQueuedMove=true; QueuedMove=Destination.Location; QueuedMoveExpires=GetWorld()->GetTimeSeconds()+2.0;
        Hero->Feedback=TEXT("Movimento preparado para o fim da ação"); return true;
    }
    bQueuedMove=false;
    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Hero->GetNavAgentLocation(),Destination.Location,Hero);
    if(!Path || !Path->IsValid() || Path->IsPartial())
    {
        UE_LOG(LogTemp, Warning, TEXT("AH_MOVE recusado: caminho=%d valido=%d parcial=%d"),
               Path ? 1 : 0, (Path && Path->IsValid()) ? 1 : 0, (Path && Path->IsPartial()) ? 1 : 0);
        Hero->Feedback=TEXT("Nao ha caminho livre ate esse ponto"); return false;
    }
    FAIMoveRequest Move(Destination.Location);
    Move.SetAcceptanceRadius(4.f); Move.SetReachTestIncludesAgentRadius(false); Move.SetReachTestIncludesGoalRadius(false);
    bWalkingBlind=false;
    PathFollowing->RequestMove(Move,Path->GetPath());
    // The gold ring marks a DECISION, so a held button does not stamp forty of
    // them across the grass on the way there.
    if(!bQuietMove)
        DrawDebugCircle(GetWorld(), Destination.Location + FVector(0, 0, 5), 35, 32,
            FColor(220, 176, 90), false, 0.7f, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
    return true;
}

void AAHPlayerController::Stop()
{
    bQueuedMove=false;
    bWalkingBlind=false;
    AttackTarget = nullptr;
    StopMovement();
    if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->Dodge();
}

void AAHPlayerController::PlayerTick(float DeltaTime)
{
    // Asked BEFORE Super, and that is the whole bug.
    //
    // APlayerController::PlayerTick runs TickPlayerInput, and ProcessInputStack
    // clears every key's press events on its way out. So a WasInputKeyJustPressed
    // asked AFTER Super::PlayerTick can never see the click that happened this
    // frame -- which is exactly why picking a target silently did nothing.
    //
    // The binding above is the primary path and this is the belt to its braces;
    // both firing in one frame picks the same foe twice, which costs nothing.
    const bool bClicked = WasInputKeyJustPressed(EKeys::LeftMouseButton);
    Super::PlayerTick(DeltaTime);
    if(IsReactionPending()) return;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if(bWalkingBlind)
    {
        if(!Hero || !Hero->IsAlive() || !Hero->bTurnActive || Hero->Turn.Movement<=1.f) bWalkingBlind=false;
        else
        {
            const FVector Here = Hero->GetActorLocation();
            const FVector Step = FVector(BlindTarget.X - Here.X, BlindTarget.Y - Here.Y, 0.0);
            if(Step.GetSafeNormal2D().IsNearlyZero() || Step.Size2D() < 70.0) bWalkingBlind=false;
            else Hero->AddMovementInput(Step.GetSafeNormal2D());
        }
    }
    if(bQueuedMove)
    {
        if(!Hero || !Hero->IsAlive() || !Hero->bTurnActive || GetWorld()->GetTimeSeconds()>QueuedMoveExpires) bQueuedMove=false;
        else if(Hero->CanAct() && GetWorld()->GetTimeSeconds()>=NextQueuedMoveAttempt)
        { NextQueuedMoveAttempt=GetWorld()->GetTimeSeconds()+.2f; RequestMoveToLocation(QueuedMove); }
    }

    // ── Holding the right button ─────────────────────────────────────────
    // A fresh path only when the cursor has gone somewhere else, or a fifth of
    // a second has passed. Exploring only: inside a fight the movement budget
    // is the point, and a held button that quietly spends a whole turn's
    // movement is not a control, it is a trap.
    if(bHoldingMove && !bSheetOpen && !IsPaused() && Hero && Hero->IsAlive() && !Hero->IsBusy())
    {
        const auto* Arena = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
        if(!Arena || !Arena->IsExploring()) bHoldingMove = false;
        else
        {
            const float Now = GetWorld()->GetTimeSeconds();
            FHitResult Ahead;
            FVector RayFrom, RayWay;
            FCollisionQueryParams Held(SCENE_QUERY_STAT(HoldMove), false, Hero);
            const auto* HUD=Cast<AAHCombatHUD>(GetHUD());
            if((!HUD || !HUD->IsPointerOverInterface()) && Now >= NextHoldPath
               && DeprojectMousePositionToWorld(RayFrom, RayWay)
               && GetWorld()->LineTraceSingleByChannel(Ahead, RayFrom,
                       RayFrom + RayWay * 100000.f, ECC_GameTraceChannel1, Held))
            {
                if(!Cast<APawn>(Ahead.GetActor()) && FVector::Dist2D(Ahead.ImpactPoint, LastHoldGoal) > 65.f)
                {
                    NextHoldPath = Now + .2f;
                    bQuietMove   = true;
                    if(RequestMoveToLocation(Ahead.ImpactPoint)) LastHoldGoal = Ahead.ImpactPoint;
                    bQuietMove   = false;
                }
                else NextHoldPath = Now + .1f;
            }
        }
    }
    FHitResult Hover;
    HoveredEnemy = GetHitResultUnderCursor(ECC_Visibility,false,Hover) ? Cast<AAHCharacter>(Hover.GetActor()) : nullptr;
    if(HoveredEnemy && !HoveredEnemy->bEnemy) HoveredEnemy=nullptr;
    if(ChosenTarget && (!IsValid(ChosenTarget) || !ChosenTarget->IsAlive())) ChosenTarget=nullptr;
    if(bClicked) SelectTarget();

    // Arrived at the person we set off to talk to: greet him without a second
    // click. Tried once per arrival, and dropped if he moved off meanwhile.
    if (bTalkOnArrival)
    {
        auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
        auto* Walker = Cast<AAHCharacter>(GetPawn());
        if (!Mode || !Walker) bTalkOnArrival = false;
        else
        {
            bool bStillGoing = false;
            for (const TObjectPtr<AAHVillager>& Who : Mode->Folk)
            {
                if (!IsValid(Who)) continue;
                const float Away = FVector::Dist2D(Walker->GetActorLocation(),
                                                   Who->GetActorLocation());
                if (Away < 340.f) { Mode->TalkToNearest(); bTalkOnArrival = false; break; }
                if (Away < 2600.f) bStillGoing = true;
            }
            if (!bStillGoing) bTalkOnArrival = false;
        }
    }
    if (!Hero || !Hero->bTurnActive || !Hero->IsAlive() || !IsValid(AttackTarget) || !AttackTarget->IsAlive()) { AttackTarget = nullptr; return; }
    if(Hero->IsBusy()) return;
    if (FVector::Dist2D(Hero->GetActorLocation(), AttackTarget->GetActorLocation()) <= 125.f)
    {
        StopMovement();
        if (Hero->TryAttack(AttackTarget)) AttackTarget = nullptr;
    }
}

void AAHPlayerController::AttackNearest()
{
    bQueuedMove=false;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->CanAct() || !Hero->Turn.bAction) return;
    AttackTarget=nullptr;
    // The chosen foe first. Falling back to the nearest is only for when the
    // player has not picked anybody -- picking one and being swung at somebody
    // else is worse than having no choice at all.
    AAHCharacter* Found = IsValid(ChosenTarget) && ChosenTarget->IsAlive() ? ChosenTarget.Get() : nullptr;
    if (!Found)
    {
        float Best = Hero->HasRangedAttack() ? Hero->RangedReach() : 1600.f;
        for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::Dist2D(Hero->GetActorLocation(), It->GetActorLocation());
            if (It->bEnemy && It->IsAlive() && Distance < Best) { Best = Distance; Found = *It; }
        }
    }
    if (!Found) return;
    // Outside a fight, picking a foe starts one rather than swinging. The
    // player joins the turn order and then acts, which is what "combat
    // begins" has to mean in a game where the order decides everything.
    if (auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
        Mode && Mode->IsExploring())
    {
        if (Mode->EngageWith(Found)) { AttackTarget = nullptr; return; }
    }
    if (Hero->HasRangedAttack() && Hero->TryRangedAttack(Found)) return;
    AttackTarget = Found;
    UAIBlueprintHelperLibrary::SimpleMoveToActor(this, AttackTarget);
}

void AAHPlayerController::Heal()
{
    if(bSpellbookOpen || IsReactionPending()) return;
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero) return;
    if(!AHRules::Class(Hero->HeroClass).bCaster || Hero->PreparedSpells.IsEmpty())
    { Hero->UseClassAbility(); return; }

    // The selected spell, at the chosen foe. It used to be "the spell, but only
    // if an enemy happens to be under the mouse right now, otherwise some other
    // ability entirely" -- which is why casting at the foe you meant was
    // impossible from a HUD button, and a coin toss from the keyboard.
    const FAHSpellDefinition& Spell = AHSpells::Get(Hero->SelectedSpell);
    if(!Spell.bHostile) { Hero->CastSpell(Hero->SelectedSpell); return; }
    auto* Victim = CurrentTarget();
    if(!Victim) { Hero->Feedback=TEXT("Nenhum inimigo para conjurar"); return; }

    // Throwing a Fire Bolt at a camp that has not noticed you starts the fight,
    // exactly as clicking one of them does. Without this the buff freedom above
    // turns into free damage on sleeping foes with no initiative ever rolled --
    // and, if you are creeping, this is the ambush opener.
    if (auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
        Mode && Mode->IsExploring())
    {
        if (Mode->EngageWith(Victim)) return;
    }
    Hero->CastSpell(Hero->SelectedSpell, Victim);
}
void AAHPlayerController::ToggleSpellbook()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero || !Hero->bCharacterReady || !AHRules::Class(Hero->HeroClass).bCaster || IsReactionPending()) return;
    bSpellbookOpen=!bSpellbookOpen;

    // Out of a fight, opening the book means RE-PREPARING it. The preparation
    // limit grows every level and the second circle opens at level three, and
    // until now there was no way back into that screen after character creation
    // -- so the capacity a level granted could never actually be spent.
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    Hero->bPreparingSpells = bSpellbookOpen && Mode && Mode->IsExploring();
}
void AAHPlayerController::ToggleSneak()
{
    if(bSpellbookOpen || IsReactionPending()) return;
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero) return;
    // Only out of combat. Inside initiative the rogue's Esconder-se is the
    // stealth that exists, and it costs an action like everything else does.
    if(Mode && !Mode->IsExploring())
    { Hero->Feedback=TEXT("Em combate, use ESCONDER (ladino) em vez do modo furtivo"); return; }
    Hero->ToggleSneak();
}

void AAHPlayerController::BreathAction() { if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->UseRacialAbility(); }
void AAHPlayerController::DisengageAction()
{
    bQueuedMove=false;
    AttackTarget=nullptr;
    StopMovement();
    if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->Disengage();
}
void AAHPlayerController::RotateCameraLeft()  { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->RotateCamera(-45.f); }
void AAHPlayerController::RotateCameraRight() { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->RotateCamera( 45.f); }
void AAHPlayerController::ZoomCameraIn()      { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->ZoomCamera(-1.f); }
void AAHPlayerController::ZoomCameraOut()     { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->ZoomCamera( 1.f); }
void AAHPlayerController::CyclePerformance() { PerformanceProfile=(PerformanceProfile+1)%3; ApplyPerformance(); }
void AAHPlayerController::ApplyPerformance()
{
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {
        Settings->SetOverallScalabilityLevel(PerformanceProfile+1);
        Settings->SetResolutionScaleValueEx(PerformanceProfile==0?85.f:100.f);
        Settings->SetFrameRateLimit(PerformanceProfile==0?120.f:60.f);
        Settings->ApplyNonResolutionSettings();
    }
    // Lumen and virtual shadow maps ride the Quality profile only, so F6 is a
    // real A/B on this machine instead of a guess about what a 6 GB laptop GPU
    // can afford. DefaultEngine.ini still ships with them off.
    const bool bRichLighting = PerformanceProfile >= 2;
    auto SetCVar=[](const TCHAR* Name, int32 Value)
    {
        if(IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
            Variable->Set(Value, ECVF_SetByGameSetting);
    };
    SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), bRichLighting ? 1 : 0);
    SetCVar(TEXT("r.ReflectionMethod"),                bRichLighting ? 1 : 2);
    SetCVar(TEXT("r.Shadow.Virtual.Enable"),           bRichLighting ? 1 : 0);

    const TCHAR* Names[]={TEXT("DESEMPENHO / F6"),TEXT("EQUILIBRADO / F6"),TEXT("QUALIDADE + LUMEN / F6")};
    PerformanceLabel=Names[PerformanceProfile];
}
void AAHPlayerController::Check()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn()); if(!Hero) return;
    AAHCharacter* Target=CurrentTarget();
    Hero->Feedback=Target?FString::Printf(TEXT("ALVO: %d/%d PV | CA %d | Ataque +%d | %.1f m | Análise gratuita"),Target->Health,Target->MaxHealth,Target->ArmorClass,Target->AttackBonus,FVector::Dist2D(Target->GetActorLocation(),Hero->GetActorLocation())/100.f):TEXT("Nenhum inimigo para analisar");
    Hero->AddLog(Hero->Feedback);
}
void AAHPlayerController::Restart() { UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName())); }
void AAHPlayerController::Dash() { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->Dash(); }
void AAHPlayerController::EndTurn()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if(!Hero || !Hero->CanAct()) return;
    bQueuedMove=false;
    if(Mode && Mode->EndTurn(Hero)) { AttackTarget=nullptr; StopMovement(); }
}
void AAHPlayerController::CombatCommand(FName Command)
{
    if(Command==TEXT("Ficha")) { ToggleSheet(); return; }
    if(Command==TEXT("Journal")) { ToggleJournal(); return; }
    if(bSheetOpen)
    {
        auto* Hero=Cast<AAHCharacter>(GetPawn());
        auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>();
        if(!Hero) return;
        const FString Name=Command.ToString();
        if(Command==TEXT("Progression")) { bProgressionSheet=!bProgressionSheet; bQuestJournal=false; }
        else if(Name.StartsWith(TEXT("Feat"))) Hero->ChooseFeat(FCString::Atoi(*Name.Mid(4)));
        else if(Name.StartsWith(TEXT("PackItem"))) Hero->UseBackpackItem(FCString::Atoi(*Name.Mid(8)));
        else if(Name.StartsWith(TEXT("Unequip"))) Hero->Unequip(static_cast<EAHSlot>(FCString::Atoi(*Name.Mid(7))));
        else if(Command==TEXT("PackNext")) ++BackpackPage;
        else if(Command==TEXT("PackPrev")) BackpackPage=FMath::Max(0,BackpackPage-1);
        else if(Command==TEXT("RestShort") && Mode) Mode->RequestRest(false);
        else if(Command==TEXT("RestLong") && Mode) Mode->RequestRest(true);
        return;
    }
    if(Command==TEXT("ReactYes")) { ResolveReaction(true); return; }
    if(Command==TEXT("ReactNo")) { ResolveReaction(false); return; }
    if(IsReactionPending()) return;
    auto* ProgressHero=Cast<AAHCharacter>(GetPawn());
    if(Command==TEXT("Spells")) { ToggleSpellbook(); return; }
    if(ProgressHero && Command==TEXT("ReadySpells"))
    { ProgressHero->bPreparingSpells=false; bSpellbookOpen=false;
      if(!ProgressHero->SelectSpell(ProgressHero->SelectedSpell)) ProgressHero->SelectSpell(ProgressHero->HeroClass==EAHHeroClass::Cleric?EAHSpell::SacredFlame:ProgressHero->HeroClass==EAHHeroClass::Paladin?EAHSpell::CureWounds:ProgressHero->HeroClass==EAHHeroClass::Ranger?EAHSpell::HuntersMark:EAHSpell::FireBolt);
      return; }
    if(ProgressHero && Command.ToString().StartsWith(TEXT("Spell_")))
    {
        const int32 Id=FCString::Atoi(*Command.ToString().Mid(6));
        if(Id>=0 && Id<AHSpells::Count())
        {
            if(static_cast<EAHSpell>(Id)==EAHSpell::Shield)
                ProgressHero->Feedback=TEXT("Escudo arcano e REACAO: sempre disponivel, nao ocupa preparacao. O jogo pergunta quando um golpe acertar.");
            else if(ProgressHero->bPreparingSpells) ProgressHero->TogglePreparedSpell(static_cast<EAHSpell>(Id));
            else if(ProgressHero->SelectSpell(static_cast<EAHSpell>(Id))) bSpellbookOpen=false;
        }
        return;
    }
    if(Command==TEXT("Next")) { if(auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode())) Mode->NextEncounter(); return; }
    if(ProgressHero)
    {
        if(Command==TEXT("Utility")) { ProgressHero->UseClassUtility(); return; }
        if(Command==TEXT("Empower")) { ProgressHero->ToggleEmpower(); return; }
        if(Command==TEXT("Aim")) { ProgressHero->SteadyAim(); return; }
        if(Command==TEXT("Hide")) { ProgressHero->Hide(); return; }
        if(Command==TEXT("Sneak")) { ProgressHero->ToggleSneak(); return; }
        if(Command==TEXT("Sacred")) { ProgressHero->SacredWeapon(); return; }
        if(Command==TEXT("Frenzy")) { ProgressHero->Frenzy(CurrentTarget()); return; }
        if(Command==TEXT("Berry")) { ProgressHero->EatGoodberry(); return; }
        if(Command==TEXT("Feature")) { ProgressHero->UseProgressionAbility(); return; }
        if(Command==TEXT("Breath")) { ProgressHero->UseRacialAbility(); return; }
        if(Command==TEXT("Slot")) { ProgressHero->CycleSpellLevel(); return; }
        if(Command.ToString().StartsWith(TEXT("Feat"))) { ProgressHero->ChooseFeat(FCString::Atoi(*Command.ToString().Right(1))); return; }
    }
    if(Command.ToString().StartsWith(TEXT("Race")))
    {
        const int32 Index=FCString::Atoi(*Command.ToString().Right(1));
        if(Index>=0 && Index<AHRules::AncestryCount()) if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->ChooseAncestry(static_cast<EAHAncestry>(Index));
        return;
    }
    if(Command==TEXT("BackAncestry"))
    {
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady) Hero->bAncestrySelected=false;
        return;
    }
    /**
     * Creation is four steps now, and the class card no longer starts the game.
     *
     * It picks the class and moves to the points; the KIT card is what starts
     * play. Splitting the two is what lets the ability screen show, live, what
     * a point of Constitution is worth to this class -- which is the only
     * reason to have point buy instead of a fixed array.
     */
    if(Command.ToString().StartsWith(TEXT("Class")))
    {
        const int32 Index=FCString::Atoi(*Command.ToString().Right(1));
        if(Index>=0 && Index<AHRules::ClassCount())
            if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->PickClass(static_cast<EAHHeroClass>(Index));
        return;
    }
    if(Command.ToString().StartsWith(TEXT("Abil")) && Command.ToString().Len()==7
        && FChar::IsDigit(Command.ToString()[4]))
    {
        // "Abil3up" / "Abil3dn": the ability's index, then the direction.
        const FString Word=Command.ToString();
        const int32 Which=FCString::Atoi(*Word.Mid(4,1));
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()))
            Hero->BuyAbility(static_cast<EAHAbility>(Which), Word.EndsWith(TEXT("up")) ? 1 : -1);
        return;
    }
    if(Command==TEXT("AbilDone"))
    {
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady && AHSheet::Legal(Hero->Abilities))
            Hero->bPointsDone=true;
        return;
    }
    if(Command==TEXT("AbilAuto"))
    {
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady)
        {
            const int32* Suggested=AHSheet::Recommended(Hero->HeroClass);
            for(int32 A=0;A<static_cast<int32>(EAHAbility::Count);++A)
                Hero->Abilities.Score[A]=Suggested[A];
        }
        return;
    }
    if(Command==TEXT("BackClass"))
    {
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady)
        { Hero->bClassPicked=false; Hero->bPointsDone=false; }
        return;
    }
    if(Command==TEXT("BackAbil"))
    {
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady)
            Hero->bPointsDone=false;
        return;
    }
    if(Command.ToString().StartsWith(TEXT("Kit")))
    {
        const int32 Which=FCString::Atoi(*Command.ToString().Right(1));
        if(auto* Hero=Cast<AAHCharacter>(GetPawn()); Hero && !Hero->bCharacterReady)
        {
            Hero->BeginWith(Which);
            Hero->bPreparingSpells=Hero->MaxSpellSlots(1)>0;
        }
        return;
    }
    if(Command==TEXT("Ficha"))
    {
        ToggleSheet();
        return;
    }
    if(Command==TEXT("Attack")) AttackNearest();
    else if(Command==TEXT("Dodge")) Stop();
    else if(Command==TEXT("Heal")) Heal();
    else if(Command==TEXT("Inspect")) Check();
    else if(Command==TEXT("Dash")) Dash();
    else if(Command==TEXT("Disengage")) DisengageAction();
    else if(Command==TEXT("EndTurn")) EndTurn();
    else if(Command==TEXT("Restart")) Restart();
}

bool AAHPlayerController::OfferReaction(AAHCharacter* Mover, EAHReaction Kind)
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(IsReactionPending() || !Hero || !Hero->IsAlive() || !Hero->Turn.bReaction) return false;
    if(!IsValid(Mover) || !Mover->IsAlive() || !Mover->bEnemy) return false;
    if(!SetPause(true)) return false;
    ReactionKind=Kind;
    ReactionTarget=Mover;
    return true;
}

void AAHPlayerController::ResolveReaction(bool bAccept)
{
    if(!IsReactionPending()) return;
    auto* Other=ReactionTarget.Get(); ReactionTarget.Reset();
    const EAHReaction Kind=ReactionKind;
    SetPause(false);
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero) return;

    if(Kind==EAHReaction::Shield)
    {
        // Other is the attacker, holding a swing in the air until it hears back.
        if(IsValid(Other)) Other->ResumeShieldedImpact(bAccept);
        return;
    }
    if(bAccept) Hero->TryOpportunityAttack(Other,true);
    else Hero->Feedback=TEXT("Oportunidade recusada: reacao preservada");
}
