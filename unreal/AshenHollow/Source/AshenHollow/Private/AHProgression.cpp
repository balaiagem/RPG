#include "AHCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
int32 AAHCharacter::MaxSpellSlots(int32 Rank) const
{
    if(HeroClass!=EAHHeroClass::Cleric && HeroClass!=EAHHeroClass::Wizard) return 0;
    const int32 First[]={0,2,3,4,4},Second[]={0,0,0,2,3};
    return Rank==1?First[FMath::Clamp(Level,1,4)]:Rank==2?Second[FMath::Clamp(Level,1,4)]:0;
}
bool AAHCharacter::HasSpellSlot() const { return SelectedSpellLevel==2?SpellSlots2>0:ClassCharges>0; }
void AAHCharacter::SpendSpellSlot() { if(SelectedSpellLevel==2) --SpellSlots2; else --ClassCharges; }
void AAHCharacter::CycleSpellLevel() { if(MaxSpellSlots(2)>0) SelectedSpellLevel=3-SelectedSpellLevel; }
void AAHCharacter::GainExperience(int32 Amount)
{
    if(bEnemy || !bCharacterReady || Amount<=0) return;
    Experience=FMath::Min(2700,Experience+Amount);
    const int32 Thresholds[]={0,300,900,2700},Growth[]={8,9,7,6};
    while(Level<4 && Experience>=Thresholds[Level])
    {
        const int32 Old1=MaxSpellSlots(1),Old2=MaxSpellSlots(2);
        ++Level;
        const int32 HP=Growth[static_cast<int32>(HeroClass)]+(Ancestry==EAHAncestry::Dwarf?1:0);
        MaxHealth+=HP; if(IsAlive()) Health+=HP;
        if(MaxSpellSlots(1)>0) ClassCharges+=MaxSpellSlots(1)-Old1;
        SpellSlots2+=MaxSpellSlots(2)-Old2;
        AddLog(FString::Printf(TEXT("Nivel %d!"),Level));
    }
}
bool AAHCharacter::ChooseFeat(int32 Choice)
{
    if(Level!=4 || Feat!=0 || Choice<1 || Choice>3 || bEnemy) return false;
    Feat=Choice;
    if(Choice==1) { MaxHealth+=2*Level; if(IsAlive()) Health+=2*Level; }
    if(Choice==2) InitiativeBonus+=5;
    if(Choice==3) BaseMovement+=300.f;
    return true;
}
FString AAHCharacter::ProgressionAbilityName() const
{
    const TCHAR* Names[]={TEXT("SURTO"),TEXT("TEMERARIO"),TEXT("ESCUDO"),TEXT("VIDA FALSA")};
    return Names[static_cast<int32>(HeroClass)];
}
void AAHCharacter::UseProgressionAbility()
{
    if(Level<2 || !CanAct()) return;
    if(HeroClass==EAHHeroClass::Fighter)
    {
        if(Turn.bAction || bActionSurgeUsed) return;
        bActionSurgeUsed=true; Turn.bAction=true; Feedback=TEXT("Surto: acao adicional");
    }
    else if(HeroClass==EAHHeroClass::Barbarian)
    {
        if(!Turn.bAction || bReckless) return;
        bReckless=true; Feedback=TEXT("Ataque temerario: vantagem ao atacar e ser atacado ate seu proximo turno");
    }
    else if(HeroClass==EAHHeroClass::Cleric)
    {
        if(!Turn.bBonus || !HasSpellSlot() || GuardTurns>0) return;
        Turn.SpendBonus(); SpendSpellSlot(); GuardTurns=10; ArmorClass+=2;
        PlayGesture(GuardAnimation); Feedback=TEXT("Escudo da fe: +2 CA / concentracao");
    }
    else
    {
        if(!Turn.bAction || !HasSpellSlot()) return;
        Turn.SpendAction(); SpendSpellSlot();
        TempHP=FMath::Max(TempHP,Dice.RandRange(1,4)+4+5*(SelectedSpellLevel-1));
        PlayGesture(CastAnimation); Feedback=TEXT("Vida falsa: PV temporarios nao acumulam");
    }
    AddLog(Feedback);
}
void AAHCharacter::Rest()
{
    FinishTurn(); GetWorldTimerManager().ClearTimer(DeathSaveTimer);
    if(GuardTurns>0) ArmorClass-=2;
    GuardTurns=0; bReckless=false; bRaging=false; RageTurns=0; TempHP=0;
    bDowned=false; bStabilized=false; DeathSuccesses=DeathFailures=0;
    bSecondWindUsed=bActionSurgeUsed=false; bHasRetreated=false;
    Health=MaxHealth; ClassCharges=MaxSpellSlots(1)>0?MaxSpellSlots(1):HeroClass==EAHHeroClass::Fighter?1:2;
    SpellSlots2=MaxSpellSlots(2); SelectedSpellLevel=1;
    AnimationEnds=ReactionEnds=0; bIsAttacking=false; PendingTarget=nullptr;
    InReachOf.Reset(); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    if(LocomotionClass) GetMesh()->SetAnimInstanceClass(LocomotionClass);
}
