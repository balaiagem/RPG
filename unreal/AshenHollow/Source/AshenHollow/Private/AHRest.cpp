#include "AHGameMode.h"
#include "AHCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

bool AAHGameMode::IsInTown(const FVector& Location) const
{
    int32 X=0,Y=0; AHArena::CellOf(Location,X,Y);
    // Include the road crossing a settlement, not only house footprints.
    for(int32 DX=-1;DX<=1;++DX)
        for(int32 DY=-1;DY<=1;++DY)
            if(AHArena::InGrid(X+DX,Y+DY) && (Plan.At(X+DX,Y+DY)==EAHCell::Village || Plan.At(X+DX,Y+DY)==EAHCell::Plaza))
                return true;
    return false;
}

FString AAHGameMode::RestRefusal(bool bLong) const
{
    const auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Hero || !Hero->bCharacterReady || !Hero->IsAlive()) return TEXT("O personagem precisa estar de pe.");
    if(!IsExploring() || bStarted || Hero->IsBusy()) return TEXT("Termine o combate antes de descansar.");
    const bool bTown=IsInTown(Hero->GetActorLocation());
    if(bLong && !bTown) return TEXT("Descanso longo: procure a cidade.");
    if(!bLong && bTown) return TEXT("Na cidade, use o descanso longo.");
    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        if(It->bEnemy && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(),Hero->GetActorLocation())<1800)
            return TEXT("Ha inimigos perto demais. Procure um lugar seguro.");
    return FString();
}

bool AAHGameMode::RequestRest(bool bLong)
{
    auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Hero) return false;
    const FString Refusal=RestRefusal(bLong);
    if(!Refusal.IsEmpty()) { Hero->Feedback=Refusal; return false; }
    if(Hero->GetController()) Hero->GetController()->StopMovement();
    if(bLong)
    {
        const int32 RecoveredDice=FMath::Min(Hero->Level,Hero->HitDice+FMath::Max(1,Hero->Level/2));
        Hero->Rest();
        Hero->HitDice=RecoveredDice;
        WorldMinutes+=480;
        Hero->AddLog(TEXT("Descanso longo: passaram 8 horas. Vida, magias e habilidades restauradas."));
    }
    else
    {
        Hero->ShortRest();
        WorldMinutes+=60;
    }
    Hero->bRoaming=true;
    Hero->StartTurn(); Hero->Turn.Movement=1000000.f;
    NextClockLight=0;
    Hero->Feedback=bLong?TEXT("Uma nova partida da cidade."):TEXT("Descanso curto concluido.");
    TickWorldClock(0);
    return true;
}

FString AAHGameMode::ClockLabel() const
{
    const int32 Minute=static_cast<int32>(WorldMinutes)%1440;
    return FString::Printf(TEXT("DIA %d  %02d:%02d"),static_cast<int32>(WorldMinutes/1440)+1,Minute/60,Minute%60);
}

void AAHGameMode::TickWorldClock(float DeltaSeconds)
{
    const auto* Hero=Cast<AAHCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Hero || !Hero->bCharacterReady) return;
    WorldMinutes+=DeltaSeconds; // 24 real minutes per day; rests advance explicitly.
    if(GetWorld()->GetTimeSeconds()<NextClockLight) return;
    NextClockLight=GetWorld()->GetTimeSeconds()+2.f;
    const float Hour=FMath::Fmod(static_cast<float>(WorldMinutes/60.0),24.f);
    const float Height=FMath::Sin((Hour-6.f)*PI/12.f);
    const float Day=FMath::Clamp((Height+.08f)/.35f,0.f,1.f);
    for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
    {
        auto* Light=It->GetLightComponent();
        if(!Light) continue;
        if(DaylightIntensity<0) DaylightIntensity=Light->Intensity;
        It->SetActorRotation(FRotator(-FMath::Max(8.f,FMath::Abs(Height)*65.f),Plan.SunYaw,0));
        Light->SetIntensity(FMath::Lerp(DaylightIntensity*.035f,DaylightIntensity,Day));
        Light->SetTemperature(FMath::Lerp(10000.f,Height<.35f?3800.f:Plan.SunTemperature,Day));
    }
    for(TActorIterator<ASkyLight> It(GetWorld());It;++It)
        if(auto* Sky=It->GetLightComponent()) Sky->SetIntensity(FMath::Lerp(.35f,Plan.SkyIntensity,Day));
}
