#include "AHPlayerController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
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
    PathFollowing = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("PathFollowing"));
}

void AAHPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ApplyPerformance();
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
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
        Mapping->MapKey(Action, Key);
        Input->BindAction(Action, ETriggerEvent::Started, this, Handler);
    };
    Bind(EKeys::Q, &AAHPlayerController::AttackNearest);
    Bind(EKeys::E, &AAHPlayerController::Heal);
    Bind(EKeys::C, &AAHPlayerController::Check);
    Bind(EKeys::F5, &AAHPlayerController::Restart);
    Bind(EKeys::Enter, &AAHPlayerController::EndTurn);
    Bind(EKeys::R, &AAHPlayerController::Dash);
    Bind(EKeys::F6, &AAHPlayerController::CyclePerformance);
    if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(Mapping, 0);
    }
}

void AAHPlayerController::MoveToCursor()
{
    FHitResult Hit;
    if (!GetPawn() || !GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->CanAct()) return;
    if (auto* HUD = Cast<AAHCombatHUD>(GetHUD()); HUD && HUD->IsPointerOverInterface()) return;
    if (auto* Enemy = Cast<AAHCharacter>(Hit.GetActor()); Enemy && Enemy->bEnemy && Enemy->IsAlive())
    {
        if (!Hero->Turn.bAction) { Hero->Feedback=TEXT("Action already spent"); return; }
        AttackTarget = Enemy;
        UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Enemy);
        return;
    }
    AttackTarget = nullptr;
    if (Hero->Turn.Movement<=1.f) { Hero->Feedback=TEXT("No movement remaining"); return; }
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Destination;
    if (!Nav || !Nav->ProjectPointToNavigation(Hit.ImpactPoint, Destination, FVector(80, 80, 150))) return;
    UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Destination.Location);
    DrawDebugCircle(GetWorld(), Destination.Location + FVector(0, 0, 5), 35, 32,
        FColor(220, 176, 90), false, 0.7f, 0, 2, FVector::ForwardVector, FVector::RightVector, false);
}

void AAHPlayerController::Stop()
{
    AttackTarget = nullptr;
    StopMovement();
    if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->Dodge();
}

void AAHPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    auto* Hero = Cast<AAHCharacter>(GetPawn());
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
    auto* Hero = Cast<AAHCharacter>(GetPawn());
    if (!Hero || !Hero->CanAct() || !Hero->Turn.bAction) return;
    AttackTarget=nullptr;
    float Best = 1600.f;
    for (TActorIterator<AAHCharacter> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist2D(Hero->GetActorLocation(), It->GetActorLocation());
        if (It->bEnemy && It->IsAlive() && Distance < Best) { Best = Distance; AttackTarget = *It; }
    }
    if (AttackTarget) UAIBlueprintHelperLibrary::SimpleMoveToActor(this, AttackTarget);
}

void AAHPlayerController::Heal() { if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->UseClassAbility(); }
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
void AAHPlayerController::Check() { if (auto* Hero = Cast<AAHCharacter>(GetPawn())) Hero->CheckArcana(); }
void AAHPlayerController::Restart() { UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName())); }
void AAHPlayerController::Dash() { if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->Dash(); }
void AAHPlayerController::EndTurn()
{
    auto* Hero=Cast<AAHCharacter>(GetPawn());
    auto* Mode=Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    if(Mode && Mode->EndTurn(Hero)) { AttackTarget=nullptr; StopMovement(); }
}
void AAHPlayerController::CombatCommand(FName Command)
{
    if(Command.ToString().StartsWith(TEXT("Class")))
    {
        const int32 Index=FCString::Atoi(*Command.ToString().Right(1));
        if(Index>=0 && Index<4) if(auto* Hero=Cast<AAHCharacter>(GetPawn())) Hero->ChooseClass(static_cast<EAHHeroClass>(Index));
        return;
    }
    if(Command==TEXT("Attack")) AttackNearest();
    else if(Command==TEXT("Dodge")) Stop();
    else if(Command==TEXT("Heal")) Heal();
    else if(Command==TEXT("Arcana")) Check();
    else if(Command==TEXT("Dash")) Dash();
    else if(Command==TEXT("EndTurn")) EndTurn();
    else if(Command==TEXT("Restart")) Restart();
}
