#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AHGameMode.generated.h"

UCLASS()
class ASHENHOLLOW_API AAHGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAHGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY() TArray<TObjectPtr<class AAHCharacter>> Order;
    int32 Round = 1;
    int32 ActiveIndex = 0;
    bool bStarted = false;
    bool bFinished = false;
    float TurnStarted = 0.f;
    AAHCharacter* ActiveCharacter() const;
    bool EndTurn(AAHCharacter* Requester);
};
