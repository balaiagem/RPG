#include "AHGameMode.h"
#include "AHCharacter.h"
#include "AHPlayerController.h"
#include "AHCombatHUD.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
AAHGameMode::AAHGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AAHCharacter::StaticClass();
    PlayerControllerClass = AAHPlayerController::StaticClass();
    HUDClass = AAHCombatHUD::StaticClass();
}
void AAHGameMode::BeginPlay()
{
    Super::BeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (auto* Enemy = GetWorld()->SpawnActor<AAHCharacter>(AAHCharacter::StaticClass(), FVector(0,100,110), FRotator(0,-90,0), Params))
    {
        Enemy->BecomeEnemy();
        Order.Add(Enemy);
    }
}
AAHCharacter* AAHGameMode::ActiveCharacter() const { return Order.IsValidIndex(ActiveIndex) ? Order[ActiveIndex].Get() : nullptr; }
void AAHGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Hero = Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!bStarted && Hero && Hero->bCharacterReady && Order.Num()==1)
    {
        Order.Add(Hero);
        FRandomStream Dice; Dice.GenerateNewSeed();
        for (const auto& Character : Order) Character->Initiative = Dice.RandRange(1,20) + Character->InitiativeBonus;
        Order.StableSort([](const AAHCharacter& A,const AAHCharacter& B) { return A.Initiative>B.Initiative || (A.Initiative==B.Initiative && !A.bEnemy && B.bEnemy); });
        bStarted=true;
        TurnStarted=GetWorld()->GetTimeSeconds();
        ActiveCharacter()->StartTurn();
        Hero->AddLog(FString::Printf(TEXT("Initiative: you %d / foe %d"),Hero->Initiative,Order[0]==Hero?Order[1]->Initiative:Order[0]->Initiative));
    }
    if (!bStarted || bFinished) return;
    bool HeroAlive=false, EnemyAlive=false;
    for(const auto& Character:Order) { if(Character->bEnemy) EnemyAlive|=Character->IsAlive(); else HeroAlive|=Character->IsAlive(); }
    if(!HeroAlive || !EnemyAlive)
    {
        bFinished=true;
        for(const auto& Character:Order) Character->FinishTurn();
        return;
    }
    auto* Active=ActiveCharacter();
    if(Active && Active->bEnemy && !Active->IsBusy())
    {
        const float Elapsed=GetWorld()->GetTimeSeconds()-TurnStarted;
        const bool OutOfReach=Hero && FVector::Dist2D(Hero->GetActorLocation(),Active->GetActorLocation())>190.f;
        if((!Active->Turn.bAction && Elapsed>1.2f) || (Active->Turn.Movement<=1.f && OutOfReach && Elapsed>2.f) || Elapsed>10.f) EndTurn(Active);
    }
}
bool AAHGameMode::EndTurn(AAHCharacter* Requester)
{
    if(!bStarted || bFinished || Requester!=ActiveCharacter() || Requester->IsBusy()) return false;
    Requester->FinishTurn();
    ActiveIndex=(ActiveIndex+1)%Order.Num();
    if(ActiveIndex==0) ++Round;
    ActiveCharacter()->StartTurn();
    TurnStarted=GetWorld()->GetTimeSeconds();
    return true;
}
