#include "AHCharacter.h"
#include "AHCombatBurst.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

void AAHCharacter::UseClassUtility()
{
    if(!CanAct()) return;
    if(HeroClass==EAHHeroClass::Paladin)
    {
        if(!Turn.bAction || LayOnHands<=0 || Health>=MaxHealth) return;
        const int32 Heal=FMath::Min(LayOnHands,MaxHealth-Health);
        Turn.SpendAction(); LayOnHands-=Heal; Health+=Heal;
        PlayGesture(HealAnimation); AAHCombatBurst::Emit(GetWorld(),GetActorLocation(),EAHBurst::Heal);
        Feedback=FString::Printf(TEXT("Imposicao das maos: +%d PV / reserva %d"),Heal,LayOnHands);
    }
    else if(HeroClass==EAHHeroClass::Rogue && Level>=2)
    {
        if(!Turn.SpendBonus()) return;
        bDisengaging=true; PlayGesture(EvadeAnimation); Feedback=TEXT("Acao astuta: desengajar com bonus");
    }
    AddLog(Feedback);
}
void AAHCharacter::ToggleEmpower()
{
    if(HeroClass!=EAHHeroClass::Sorcerer || Level<3 || !CanAct()) return;
    bEmpowerNext=!bEmpowerNext;
    Feedback=bEmpowerNext?TEXT("Potencializar: 1 ponto na proxima magia de dano; rerrola ate 3 dados baixos"):TEXT("Potencializar desativado");
}
void AAHCharacter::SteadyAim()
{
    if(HeroClass!=EAHHeroClass::Rogue || Level<3 || !CanAct() || bSteadyAim || MovementSpentThisTurn>1.f || !Turn.SpendBonus()) return;
    bSteadyAim=true; bAimMovementLocked=true; Turn.Movement=0; if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately(); Feedback=TEXT("Mira firme: vantagem no proximo ataque; sem movimento neste turno");
}
void AAHCharacter::EatGoodberry()
{
    if(!CanAct() || Goodberries<=0 || Health>=MaxHealth || !Turn.SpendAction()) return;
    --Goodberries; ++Health; PlayGesture(HealAnimation); Feedback=TEXT("Bom fruto: +1 PV");
}
int32 AAHCharacter::SpellDamageDie(int32 Sides, int32& Rerolls)
{
    int32 Value=Dice.RandRange(1,Sides);
    if(Rerolls>0 && Value<=Sides/2) { --Rerolls; Value=Dice.RandRange(1,Sides); }
    return Value;
}
int32 AAHCharacter::AddWeaponRiders(AAHCharacter* Target, FAHDiceOutcome& Roll, bool bRanged)
{
    if(!Target || !Roll.bSuccess) return 0;
    int32 Radiant=0;
    if(HeroClass==EAHHeroClass::Rogue && !bSneakUsed && Roll.Advantage>=0)
    {
        bool Eligible=Roll.Advantage>0;
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
            if(*It!=this && It->bEnemy==bEnemy && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(),Target->GetActorLocation())<=150.f) Eligible=true;
        if(Eligible)
        {
            bSneakUsed=true;
            const int32 DiceCount=(Level>=3?2:1)*(Roll.bCritical?2:1);
            for(int32 I=0;I<DiceCount;++I) Roll.Damage+=Dice.RandRange(1,6);
            AddLog(TEXT("Ataque furtivo!"));
        }
    }
    if(MarkTurns>0 && MarkedTarget.Get()==Target)
        for(int32 I=0;I<(Roll.bCritical?2:1);++I) Roll.Damage+=Dice.RandRange(1,6);
    if(HeroClass==EAHHeroClass::Paladin && Level>=2 && !bRanged && bSmiteArmed && ClassCharges>0)
    {
        --ClassCharges; bSmiteArmed=false;
        for(int32 I=0;I<(Roll.bCritical?4:2);++I) Radiant+=Dice.RandRange(1,8);
        AddLog(TEXT("Punicao divina: espaco I gasto no acerto"));
        AAHCombatBurst::Emit(GetWorld(),Target->GetActorLocation(),EAHBurst::Force);
    }
    return Radiant;
}
