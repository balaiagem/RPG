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
#include "AHCombatHUD.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"

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
    Input->BindAction(MoveAction, ETriggerEvent::Started, this, &AAHPlayerController::MoveToCursor);
    Input->BindAction(StopAction, ETriggerEvent::Started, this, &AAHPlayerController::Stop);
    auto Bind = [this, Input](FKey Key, void (AAHPlayerController::*Handler)())
    {
        auto* Action = NewObject<UInputAction>(Mapping);
        Action->ValueType = EInputActionValueType::Boolean;
        Action->bTriggerWhenPaused=(Key==EKeys::Y || Key==EKeys::N);
        Mapping->MapKey(Action, Key);
        Input->BindAction(Action, ETriggerEvent::Started, this, Handler);
    };
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
    Bind(EKeys::F6, &AAHPlayerController::CyclePerformance);
    if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(Mapping, 0);
    }
}

void AAHPlayerController::MoveToCursor()
{
    FHitResult Hit;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->IsAlive() || !Hero->bTurnActive) return;
    if (auto* HUD = Cast<AAHCombatHUD>(GetHUD()); HUD && HUD->IsPointerOverInterface()) return;
    FVector RayStart, RayDirection;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MoveCursor),false,Hero);
    if(!DeprojectMousePositionToWorld(RayStart,RayDirection) || !GetWorld()->LineTraceSingleByChannel(Hit,RayStart,RayStart+RayDirection*100000.f,ECC_Visibility,Query)) { Hero->Feedback=TEXT("Clique em uma superfície do cenário"); return; }
    if (auto* Enemy = Cast<AAHCharacter>(Hit.GetActor()); Enemy && Enemy->bEnemy && Enemy->IsAlive())
    {
        bQueuedMove=false;
        if(Hero->IsBusy()) { Hero->Feedback=TEXT("Aguarde o fim da ação para atacar"); return; }
        if (!Hero->Turn.bAction) { Hero->Feedback=TEXT("Ação já utilizada neste turno"); return; }
        // A shooter fires from where it stands rather than walking into reach.
        if (Hero->HasRangedAttack() && Hero->TryRangedAttack(Enemy)) { AttackTarget=nullptr; return; }
        AttackTarget = Enemy;
        UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Enemy);
        return;
    }
    RequestMoveToLocation(Hit.ImpactPoint);
}

bool AAHPlayerController::RequestMoveToLocation(const FVector& Location)
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(!Hero || !Hero->IsAlive() || !Hero->bTurnActive) { bQueuedMove=false; return false; }
    if(Hero->Turn.Movement<=1.f) { bQueuedMove=false; Hero->Feedback=TEXT("Sem movimento restante"); return false; }
    AttackTarget=nullptr;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Destination;
    if (!Nav || !Nav->ProjectPointToNavigation(Location, Destination, FVector(80, 80, 150)))
    {
        Hero->Feedback=TEXT("Destino fora da área navegável");
        bQueuedMove=false; return false;
    }
    if(Hero->IsBusy())
    {
        bQueuedMove=true; QueuedMove=Destination.Location; QueuedMoveExpires=GetWorld()->GetTimeSeconds()+2.0;
        Hero->Feedback=TEXT("Movimento preparado para o fim da ação"); return true;
    }
    bQueuedMove=false;
    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Hero->GetNavAgentLocation(),Destination.Location,Hero);
    if(!Path || !Path->IsValid() || Path->IsPartial()) { Hero->Feedback=TEXT("Não há caminho livre até esse ponto"); return false; }
    FAIMoveRequest Move(Destination.Location);
    Move.SetAcceptanceRadius(4.f); Move.SetReachTestIncludesAgentRadius(false); Move.SetReachTestIncludesGoalRadius(false);
    PathFollowing->RequestMove(Move,Path->GetPath());
    DrawDebugCircle(GetWorld(), Destination.Location + FVector(0, 0, 5), 35, 32,
        FColor(220, 176, 90), false, 0.7f, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
    return true;
}

void AAHPlayerController::Stop()
{
    bQueuedMove=false;
    AttackTarget = nullptr;
    StopMovement();
    if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->Dodge();
}

void AAHPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if(IsReactionPending()) return;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if(bQueuedMove)
    {
        if(!Hero || !Hero->IsAlive() || !Hero->bTurnActive || GetWorld()->GetTimeSeconds()>QueuedMoveExpires) bQueuedMove=false;
        else if(Hero->CanAct()) RequestMoveToLocation(QueuedMove);
    }
    FHitResult Hover;
    HoveredEnemy = GetHitResultUnderCursor(ECC_Visibility,false,Hover) ? Cast<AAHCharacter>(Hover.GetActor()) : nullptr;
    if(HoveredEnemy && !HoveredEnemy->bEnemy) HoveredEnemy=nullptr;
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
    float Best = Hero->HasRangedAttack() ? Hero->RangedReach() : 1600.f;
    AAHCharacter* Found=nullptr;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist2D(Hero->GetActorLocation(), It->GetActorLocation());
        if (It->bEnemy && It->IsAlive() && Distance < Best) { Best = Distance; Found = *It; }
    }
    if (!Found) return;
    if (Hero->HasRangedAttack() && Hero->TryRangedAttack(Found)) return;
    AttackTarget = Found;
    UAIBlueprintHelperLibrary::SimpleMoveToActor(this, AttackTarget);
}

void AAHPlayerController::Heal()
{
    if(bSpellbookOpen || IsReactionPending()) return;
    if(auto* Hero=Cast<AAHCharacter>(GetPawn()))
    {
        if(Hero->MaxSpellSlots(1)>0 && HoveredEnemy) Hero->CastSpell(Hero->SelectedSpell,HoveredEnemy);
        else Hero->UseClassAbility();
    }
}
void AAHPlayerController::ToggleSpellbook()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(Hero && Hero->bCharacterReady && AHRules::Class(Hero->HeroClass).bCaster && !IsReactionPending()) bSpellbookOpen=!bSpellbookOpen;
}
void AAHPlayerController::BreathAction() { if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->UseRacialAbility(); }
void AAHPlayerController::DisengageAction()
{
    bQueuedMove=false;
    AttackTarget=nullptr;
    StopMovement();
    if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->Disengage();
}
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
    const TCHAR* Names[]={TEXT("DESEMPENHO / F6"),TEXT("EQUILIBRADO / F6"),TEXT("QUALIDADE / F6")};
    PerformanceLabel=Names[PerformanceProfile];
}
void AAHPlayerController::Check()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn()); if(!Hero) return;
    AAHCharacter* Target=HoveredEnemy; float Distance=FLT_MAX;
    if(!Target) for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        if(It->bEnemy && It->IsAlive() && FVector::DistSquared(It->GetActorLocation(),Hero->GetActorLocation())<Distance)
        { Target=*It; Distance=FVector::DistSquared(It->GetActorLocation(),Hero->GetActorLocation()); }
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
            if(ProgressHero->bPreparingSpells) ProgressHero->TogglePreparedSpell(static_cast<EAHSpell>(Id));
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
    if(Command.ToString().StartsWith(TEXT("Class")))
    {
        const int32 Index=FCString::Atoi(*Command.ToString().Right(1));
        if(Index>=0 && Index<AHRules::ClassCount()) if(auto* Hero=Cast<AAHCharacter>(GetPawn())) { Hero->ChooseClass(static_cast<EAHHeroClass>(Index)); Hero->bPreparingSpells=Hero->MaxSpellSlots(1)>0; }
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

bool AAHPlayerController::OfferReaction(AAHCharacter* Mover)
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    if(IsReactionPending() || !Hero || !Hero->IsAlive() || !Hero->Turn.bReaction || !IsValid(Mover) || !Mover->IsAlive() || !Mover->bEnemy) return false;
    if(!SetPause(true)) return false;
    ReactionTarget=Mover;
    return true;
}

void AAHPlayerController::ResolveReaction(bool bAccept)
{
    if(!IsReactionPending()) return;
    auto* Mover=ReactionTarget.Get(); ReactionTarget.Reset();
    SetPause(false);
    if(auto* Hero=Cast<AAHCharacter>(GetPawn()))
    {
        if(bAccept) Hero->TryOpportunityAttack(Mover,true);
        else Hero->Feedback=TEXT("Oportunidade recusada: reacao preservada");
    }
}
