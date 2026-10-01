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
        ++Level;
        HitDice=FMath::Min(Level,HitDice+1);
        const int32 HP=AHRules::Class(HeroClass).HitPointGrowth
                      +AHRules::Ancestry(Ancestry).HealthPerLevel
                      // Draconic Resilience: one more hit point every level.
                      +(HeroClass==EAHHeroClass::Sorcerer?1:0);
        MaxHealth+=HP; if(IsAlive()) Health+=HP;

        // A level is a FULL reset, not a top-up of the slots the level added.
        //
        // In the tabletop game that would be a long rest's job, but this game has
        // no long rest between camps -- so the old behaviour, handing over one
        // extra slot while the rest stayed spent, meant the valley got harder
        // exactly as the character was supposed to be getting stronger. Lucas hit
        // that and called it, correctly: "nao reseta seus slots entao o game fica
        // mt dificil".
        const bool bCaster=AHRules::Class(HeroClass).bCaster;
        ClassCharges=bCaster?MaxSpellSlots(1):AHRules::Class(HeroClass).ClassCharges;
        SpellSlots2=MaxSpellSlots(2);
        SelectedSpellLevel=1;
        SorceryPoints=HeroClass==EAHHeroClass::Sorcerer?Level:0;
        LayOnHands=5*Level;
        bSecondWindUsed=bActionSurgeUsed=false;
        bRelentlessUsed=bBreathUsed=false;
        bHasRetreated=false;
        bChannelUsed=false;

        /**
         * And the sheet is worked out again from the hit die.
         *
         * The lines above still add the class table's flat growth, because
         * that is what a character with no ability scores gets and what every
         * foe uses. RecomputeSheet then replaces the maximum with hit die
         * plus Constitution per level, which is the 5e answer -- and it is a
         * no-op for anybody the ability system does not cover.
         */
        RecomputeSheet();

        // The Duelling and Archery styles are read at the roll, but Defense is
        // armour, and the paladin's and ranger's styles arrive at level 2 -- so
        // the one that is a stored number has to be added the moment it arrives.
        const FAHFightingStyle& Style=AHRules::Style(HeroClass);
        if(bEnemy && Style.Level==Level && Style.ArmorBonus>0) ArmorClass+=Style.ArmorBonus;

        // And say so when the domain, origin or archetype arrives -- a level
        // that silently changes how you fight is a level nobody notices.
        const FAHSubclass& Path=AHRules::Subclass(HeroClass);
        if(Path.Level==Level)
            AddLog(FString::Printf(TEXT("%s: %s"),Path.Name,Path.Detail));

        // The preparation limit grows with the level and the second circle opens
        // at level three, and until now nothing ever offered the book again --
        // so the new capacity existed on paper and nowhere else.
        if(PreparedSpells.IsEmpty() && MaxSpellSlots(1)>0) InitializeSpellbook();
        AddLog(bCaster
            ? FString::Printf(TEXT("Nivel %d! Espacos restaurados. Aperte K fora de combate para repreparar magias (%d de %d)."),
                              Level,PreparedSpells.Num(),PreparedLimit())
            : FString::Printf(TEXT("Nivel %d! Habilidades restauradas."),Level));
    }
}
bool AAHCharacter::ChooseFeat(int32 Choice)
{
    if(Level!=4 || Feat!=0 || Choice<1 || Choice>9 || bEnemy || !IsAlive()) return false;
    if(Choice>=4)
    {
        const int32 Ability=Choice-4;
        const int32 Total=AHSheet::Total(Abilities,Ancestry).Score[Ability];
        if(Total>=20) return false;
        Abilities.Score[Ability]+=FMath::Min(2,20-Total);
    }
    Feat=Choice;
    if(Choice==1) { MaxHealth+=2*Level; if(IsAlive()) Health+=2*Level; }
    if(Choice==2) InitiativeBonus+=5;
    if(Choice==3) BaseMovement+=300.f;
    RecomputeSheet();
    AddLog(TEXT("Evolucao escolhida. A ficha e os atributos foram atualizados."));
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
    else if(HeroClass==EAHHeroClass::Cleric)
    {
        // Channel Divinity: Preserve Life. Once per short rest, and it is the
        // cleric's answer to a fight going badly -- Shield of Faith was already
        // one button away in the spellbook, so that shortcut bought nothing.
        if(bChannelUsed)      { Feedback=TEXT("Canalizar Divindade: uma vez por descanso"); return; }
        if(!Turn.bAction)     { Feedback=TEXT("Canalizar Divindade gasta sua acao"); return; }
        if(Health>=MaxHealth) { Feedback=TEXT("Vida cheia: preservar a vida nao e necessario"); return; }
        bChannelUsed=true; Turn.SpendAction();
        const int32 Restored=ApplyHealing(5*Level);
        PlayGesture(HealAnimation);
        Feedback=FString::Printf(TEXT("Canalizar Divindade - Preservar a Vida: +%d PV"),Restored);
    }
    else { CastSpell(EAHSpell::FalseLife); return; }
    AddLog(Feedback);
}
void AAHCharacter::ShortRest()
{
    bSecondWindUsed = bActionSurgeUsed = false;
    bHasRetreated   = false;

    int32 Healed=0;
    if(IsAlive() && Health<MaxHealth && HitDice>0)
    {
        --HitDice;
        const int32 Con=AHSheet::Total(Abilities,Ancestry).Mod(EAHAbility::Constituicao);
        Healed=ApplyHealing(FMath::Max(0,Dice.RandRange(1,AHSheet::HitDie(HeroClass))+Con));
    }
    // Arcane Recovery is bounded by the long-rest cycle; resting repeatedly
    // cannot refill every caster's spellbook for free.
    if(HeroClass==EAHHeroClass::Wizard && !bArcaneRecoveryUsed && ClassCharges<MaxSpellSlots(1))
    {
        ClassCharges=FMath::Min(MaxSpellSlots(1),ClassCharges+(Level+1)/2);
        bArcaneRecoveryUsed=true;
    }
    bChannelUsed=false;
    AddLog(FString::Printf(TEXT("Descanso curto: 1 hora, +%d PV. Dados de vida: %d/%d."),Healed,HitDice,Level));
}
void AAHCharacter::Rest()
{
    HitDice=Level; bArcaneRecoveryUsed=false;
    FinishTurn(); GetWorldTimerManager().ClearTimer(DeathSaveTimer);
    if(GuardTurns>0) ArmorClass-=2;
    MaxHealth-=AidBonus; AidBonus=0; ArmorClass-=MageArmorBonus; MageArmorBonus=0;
    FrostTurns=BlessTurns=0; GuidingSource.Reset(); PendingSpellId=-1; bBonusSpellCast=bLeveledActionSpellCast=false;
    GuardTurns=0; bReckless=false; bRaging=false; RageTurns=0; TempHP=0;
    bDowned=false; bStabilized=false; DeathSuccesses=DeathFailures=0;
    bSecondWindUsed=bActionSurgeUsed=false; bHasRetreated=false;
    bRelentlessUsed=false; bBreathUsed=false;
    if(ShieldTurns>0) ArmorClass-=5;
    ShieldTurns=0; SacredTurns=0; bHidden=false; bChannelUsed=false; bColossusUsed=false;
    SorceryPoints=HeroClass==EAHHeroClass::Sorcerer && Level>=2?Level:0; LayOnHands=5*Level; Goodberries=0;
    bEmpowerNext=bSmiteArmed=bSneakUsed=bSteadyAim=bAimMovementLocked=false; MovementSpentThisTurn=0; MarkedTarget.Reset(); MarkTurns=0;
    Health=MaxHealth; ClassCharges=AHRules::Class(HeroClass).bCaster?MaxSpellSlots(1):AHRules::Class(HeroClass).ClassCharges;
    SpellSlots2=MaxSpellSlots(2); SelectedSpellLevel=1;
    AnimationEnds=ReactionEnds=0; bIsAttacking=false; PendingTarget=nullptr;
    InReachOf.Reset(); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    if(LocomotionClass) GetMesh()->SetAnimInstanceClass(LocomotionClass);
}
