#include "AHCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
int32 AAHCharacter::MaxSpellSlots(int32 Rank) const
{
    if(!AHRules::Class(HeroClass).bCaster) return 0;
    if(HeroClass==EAHHeroClass::Paladin || HeroClass==EAHHeroClass::Ranger) return Rank==1 && Level>=2?(Level==2?2:3):0;
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
    const int32 Thresholds[]={0,300,900,2700};
    while(Level<4 && Experience>=Thresholds[Level])
    {
        const int32 Old1=MaxSpellSlots(1),Old2=MaxSpellSlots(2);
        ++Level;
        SorceryPoints=HeroClass==EAHHeroClass::Sorcerer?Level:0;
        LayOnHands+=5;
        const int32 HP=AHRules::Class(HeroClass).HitPointGrowth
                      +AHRules::Ancestry(Ancestry).HealthPerLevel;
        MaxHealth+=HP; if(IsAlive()) Health+=HP;
        if(MaxSpellSlots(1)>0) ClassCharges+=MaxSpellSlots(1)-Old1;
        SpellSlots2+=MaxSpellSlots(2)-Old2;
        if(Old1==0 && MaxSpellSlots(1)>0) InitializeSpellbook();
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
    return AHRules::Class(HeroClass).ProgressionName;
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
    else if(HeroClass==EAHHeroClass::Sorcerer)
    {
        if(!Turn.bBonus || SorceryPoints<2 || ClassCharges>=MaxSpellSlots(1)) { Feedback=TEXT("Converter: 2 pontos + bonus, espaco I abaixo do maximo"); return; }
        Turn.SpendBonus(); SorceryPoints-=2; ++ClassCharges; Feedback=TEXT("Espaco I recuperado");
    }
    else if(HeroClass==EAHHeroClass::Rogue)
    {
        if(bAimMovementLocked || !Turn.SpendBonus()) return;
        Turn.Movement+=FMath::Max(0.f,BaseMovement-(FrostTurns>0?300.f:0.f)); bDashing=true; Feedback=TEXT("Acao astuta: corrida com bonus");
    }
    else if(HeroClass==EAHHeroClass::Paladin) { bSmiteArmed=!bSmiteArmed; Feedback=bSmiteArmed?TEXT("Punicao armada: gasta espaco no proximo acerto corpo a corpo"):TEXT("Punicao desativada"); }
    else if(HeroClass==EAHHeroClass::Ranger) { SelectedSpell=EAHSpell::HuntersMark; UseClassAbility(); return; }
    else { CastSpell(HeroClass==EAHHeroClass::Cleric?EAHSpell::ShieldOfFaith:EAHSpell::FalseLife); return; }
    AddLog(Feedback);
}
void AAHCharacter::Rest()
{
    FinishTurn(); GetWorldTimerManager().ClearTimer(DeathSaveTimer);
    if(GuardTurns>0) ArmorClass-=2;
    MaxHealth-=AidBonus; AidBonus=0; ArmorClass-=MageArmorBonus; MageArmorBonus=0;
    FrostTurns=BlessTurns=0; GuidingSource.Reset(); PendingSpellId=-1; bBonusSpellCast=bLeveledActionSpellCast=false;
    GuardTurns=0; bReckless=false; bRaging=false; RageTurns=0; TempHP=0;
    bDowned=false; bStabilized=false; DeathSuccesses=DeathFailures=0;
    bSecondWindUsed=bActionSurgeUsed=false; bHasRetreated=false;
    bRelentlessUsed=false; bBreathUsed=false;
    SorceryPoints=HeroClass==EAHHeroClass::Sorcerer && Level>=2?Level:0; LayOnHands=5*Level; Goodberries=0;
    bEmpowerNext=bSmiteArmed=bSneakUsed=bSteadyAim=bAimMovementLocked=false; MovementSpentThisTurn=0; MarkedTarget.Reset(); MarkTurns=0;
    Health=MaxHealth; ClassCharges=AHRules::Class(HeroClass).bCaster?MaxSpellSlots(1):AHRules::Class(HeroClass).ClassCharges;
    SpellSlots2=MaxSpellSlots(2); SelectedSpellLevel=1;
    AnimationEnds=ReactionEnds=0; bIsAttacking=false; PendingTarget=nullptr;
    InReachOf.Reset(); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    if(LocomotionClass) GetMesh()->SetAnimInstanceClass(LocomotionClass);
}
