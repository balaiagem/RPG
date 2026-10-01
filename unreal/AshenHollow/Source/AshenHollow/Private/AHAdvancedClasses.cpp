#include "AHCharacter.h"
#include "AHCombatBurst.h"
#include "AHGameMode.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionQueryParams.h"

void AAHCharacter::UseClassUtility()
{
    if(!CanAct()) return;
    if(HeroClass==EAHHeroClass::Paladin)
    {
        if(!Turn.bAction || LayOnHands<=0 || Health>=MaxHealth) return;
        Turn.SpendAction();
        const int32 Heal=ApplyHealing(FMath::Min(LayOnHands,MaxHealth-Health));
        LayOnHands-=Heal;
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
// ── Subclass plumbing ────────────────────────────────────────────────────────
int32 AAHCharacter::CombatRound() const
{
    const auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    return (Mode && Mode->bStarted) ? Mode->Round : 0;
}

bool AAHCharacter::IsSurpriseRound() const
{
    const auto* Mode = Cast<AAHGameMode>(GetWorld()->GetAuthGameMode());
    return Mode && Mode->bStarted && Mode->bSurprise && Mode->Round == 1;
}

void AAHCharacter::UpgradeToCritical(FAHDiceOutcome& Roll, int32 Sides, const TCHAR* Why)
{
    if(!Roll.bSuccess || Roll.bCritical) return;
    Roll.bCritical = true;
    // RollAttack already rolled one weapon die; a critical is two, so one more
    // is exactly the difference. Re-rolling the whole attack would also re-roll
    // the d20 the player just watched land, which is the one thing the dice
    // panel exists to prevent.
    int32 Extra=0;
    for(int32 I=0;I<WeaponDice();++I) Extra+=Dice.RandRange(1,FMath::Max(1,Sides));
    Roll.Damage += Extra;
    AddLog(FString::Printf(TEXT("%s: CRITICO (+%d)"), Why, Extra));
    UE_LOG(LogTemp, Display, TEXT("AH_SUBCLASS crit %s +%d"), Why, Extra);
}

void AAHCharacter::ApplySubclassCrit(FAHDiceOutcome& Roll, int32 Sides)
{
    if(!Roll.bSuccess || Roll.bCritical) return;
    if(HeroClass==EAHHeroClass::Fighter && Level>=3 && Roll.NaturalRoll>=19)
        UpgradeToCritical(Roll,Sides,TEXT("Campeao"));
    else if(HeroClass==EAHHeroClass::Rogue && Level>=3 && IsSurpriseRound())
        UpgradeToCritical(Roll,Sides,TEXT("Assassino"));
}

void AAHCharacter::SacredWeapon()
{
    if(HeroClass!=EAHHeroClass::Paladin || Level<3) return;
    if(!CanAct())      { Feedback=TEXT("Aguarde o fim da acao"); return; }
    if(bChannelUsed)   { Feedback=TEXT("Canalizar Divindade: uma vez por descanso"); return; }
    if(SacredTurns>0)  { Feedback=TEXT("A arma sagrada ja esta brilhando"); return; }
    if(!Turn.SpendAction()) { Feedback=TEXT("Sem acao neste turno"); return; }
    bChannelUsed=true;
    SacredTurns=10;
    PlayGesture(GuardAnimation);
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation(),EAHBurst::Heal);
    Feedback=TEXT("Arma sagrada: +2 para acertar por 10 turnos");
    AddLog(Feedback);
    UE_LOG(LogTemp, Display, TEXT("AH_SUBCLASS arma sagrada"));
}

void AAHCharacter::Frenzy(AAHCharacter* Target)
{
    if(HeroClass!=EAHHeroClass::Barbarian || Level<3) return;
    if(!bRaging)    { Feedback=TEXT("Frenesi so em furia"); return; }
    if(!Turn.bBonus){ Feedback=TEXT("Sem acao bonus neste turno"); return; }
    if(!IsValid(Target) || !Target->IsAlive())
    { Feedback=TEXT("Escolha um alvo com TAB antes do frenesi"); return; }
    if(TryAttack(Target,/*bBonusAction*/true))
    {
        Feedback=TEXT("Frenesi: golpe extra com a acao bonus");
        AddLog(Feedback);
        UE_LOG(LogTemp, Display, TEXT("AH_SUBCLASS frenesi"));
    }
}

// ── Fighting Style ───────────────────────────────────────────────────────────
// Read at the roll rather than baked into the sheet, because that is the only
// place Archery and Duelling mean anything, and a number baked at level 1 would
// be wrong for the two classes that get their style at level 2.
int32 AAHCharacter::RangedAttackBonus() const
{
    const FAHFightingStyle& Style=AHRules::Style(HeroClass);
    return AttackBonus + ((Style.Level>0 && Level>=Style.Level)?Style.RangedAttack:0);
}
int32 AAHCharacter::MeleeDamageBonus() const
{
    const FAHFightingStyle& Style=AHRules::Style(HeroClass);
    const auto* Weapon=bEnemy?nullptr:AHItems::Find(Equipped[static_cast<int32>(EAHSlot::MaoPrincipal)]);
    const bool bDueling=bEnemy || (Weapon && !Weapon->bDuasMaos && Weapon->Alcance==0);
    return DamageModifier + (bRaging?2:0) + ((bDueling && Style.Level>0 && Level>=Style.Level)?Style.MeleeDamage:0);
}

// ── Hide ─────────────────────────────────────────────────────────────────────
// Not a free button. You hide BEHIND something, so the refusal names what is
// missing -- which also makes the cover the map generator lays down matter for
// something other than armour class.
FString AAHCharacter::HideRefusal() const
{
    if(HeroClass!=EAHHeroClass::Rogue) return TEXT("Somente o ladino se esconde em combate");
    if(bHidden)   return TEXT("Voce ja esta escondido");
    if(!CanAct()) return TEXT("Aguarde o fim da acao");
    if(Level>=2 ? !Turn.bBonus : !Turn.bAction)
        return Level>=2?TEXT("Sem acao bonus neste turno"):TEXT("Sem acao neste turno");

    for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
    {
        const AAHCharacter* Watcher=*It;
        if(!IsValid(Watcher) || Watcher==this || Watcher->bEnemy==bEnemy) continue;
        if(!Watcher->IsAlive() || Watcher->bDowned) continue;

        FHitResult Blocked;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HideSight),false,this);
        Query.AddIgnoredActor(Watcher);
        // A wall between the two of you is as good as a crate in front of you.
        if(GetWorld()->LineTraceSingleByChannel(Blocked,GetActorLocation(),
               Watcher->GetActorLocation(),ECC_Visibility,Query)) continue;
        if(CoverFrom(Watcher)!=EAHCover::None) continue;
        return TEXT("Sem onde se esconder: fique atras de cobertura ou fora de vista");
    }
    return FString();
}

void AAHCharacter::Hide()
{
    const FString Refusal=HideRefusal();
    if(!Refusal.IsEmpty()) { Feedback=Refusal; return; }
    // Cunning Action makes this a bonus action at level 2. Before that it is
    // your action, exactly as the rules have it for everyone else.
    if(Level>=2 ? !Turn.SpendBonus() : !Turn.SpendAction()) return;
    bHidden=true;
    PlayGesture(EvadeAnimation);
    Feedback=Level>=2
        ? TEXT("Escondido (acao astuta): vantagem no proximo ataque, e o ataque furtivo vale")
        : TEXT("Escondido: vantagem no proximo ataque, e o ataque furtivo vale");
    AddLog(Feedback);
}

// ── Shield ───────────────────────────────────────────────────────────────────
// Worth asking about only when the answer changes something. A natural twenty
// hits whatever the armour class is, and five more points on an attack that was
// already going to miss buys nothing -- in both cases the prompt would be a
// pause that costs the player a slot for no reason, which is worse than not
// having the spell.
// Asked on EVERY hit that lands, not only on the ones the five points would
// turn away.
//
// The narrow version was wrong twice over. It is not the rule -- the Shield
// stands until your next turn, so it is worth raising against the second and
// third attacks of a round even when it arrives too late for the first. And it
// made the spell invisible: with armour class 12 against foes at +5, the band
// where five points flip the answer is a total of 12 to 16, and a whole session
// went by without one landing in it. A reaction nobody is ever offered is a
// reaction that does not exist.
FString AAHCharacter::ShieldRefusal() const
{
    if(bEnemy || !IsAlive() || bDowned)                  return TEXT("voce nao pode reagir agora");
    if(!AHSpells::ForClass(EAHSpell::Shield,HeroClass))  return TEXT("sua classe nao conhece o escudo");
    if(ShieldTurns>0)                                    return TEXT("o escudo ja esta de pe");
    if(ClassCharges<=0)                                  return TEXT("sem espaco de 1o circulo");
    if(!Turn.bReaction)                                  return TEXT("reacao ja gasta nesta rodada");
    return FString();
}

void AAHCharacter::RaiseShield(FAHDiceOutcome& Roll)
{
    if(!Turn.SpendReaction()) return;
    --ClassCharges;
    ArmorClass  += 5;
    ShieldTurns  = 1;                // stands until the start of your next turn
    Roll.Target += 5;

    // It may well arrive too late for this particular swing, and that is a real
    // outcome rather than a bug: the wall is up for the rest of the round either
    // way. Saying which of the two happened is the whole point.
    const bool bTurned = Roll.Total < Roll.Target && !Roll.bCritical;
    if(bTurned) { Roll.bSuccess=false; Roll.Damage=0; }

    ImpactText = bTurned ? TEXT("ESCUDO!") : TEXT("ESCUDO TARDE");
    ImpactTextTime=GetWorld()->GetTimeSeconds();
    bImpactHealing=false; bLastImpactCritical=false;
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation()+FVector(0,0,40),EAHBurst::Guard);
    Feedback = bTurned
        ? FString::Printf(TEXT("Escudo arcano: CA %d, o golpe erra. Espacos I: %d"),ArmorClass,ClassCharges)
        : FString::Printf(TEXT("Escudo arcano tarde demais para este golpe, mas CA %d ate o seu turno. Espacos I: %d"),ArmorClass,ClassCharges);
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
    if(HeroClass!=EAHHeroClass::Rogue || Level<2 || !CanAct() || bSteadyAim || MovementSpentThisTurn>1.f || !Turn.SpendBonus()) return;
    bSteadyAim=true; bAimMovementLocked=true; Turn.Movement=0; if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately(); Feedback=TEXT("Mira firme: vantagem no proximo ataque; sem movimento neste turno");
}
void AAHCharacter::EatGoodberry()
{
    if(!CanAct() || Goodberries<=0 || Health>=MaxHealth || !Turn.SpendAction()) return;
    --Goodberries; ApplyHealing(1); PlayGesture(HealAnimation); Feedback=TEXT("Bom fruto: +1 PV");
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
        // The assassin opens before anyone has found their feet.
        bool Eligible=Roll.Advantage>0 || (Level>=3 && CombatRound()==1);
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
            if(*It!=this && It->bEnemy==bEnemy && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(),Target->GetActorLocation())<=150.f) Eligible=true;
        if(Eligible)
        {
            bSneakUsed=true;
            const int32 Scale=(Level>=3?2:1);
            const int32 DiceCount=Scale*(Roll.bCritical?2:1);
            int32 Extra=0;
            for(int32 I=0;I<DiceCount;++I) Extra+=Dice.RandRange(1,6);
            Roll.Damage+=Extra;
            // Said out loud, on the damage number as well as in the log. A
            // feature that fires silently is a feature the player reports as
            // broken -- which is exactly what happened to this one.
            Target->ImpactText=FString::Printf(TEXT("FURTIVO +%d"),Extra);
            Target->ImpactTextTime=GetWorld()->GetTimeSeconds();
            Feedback=FString::Printf(TEXT("Ataque furtivo: +%dd6 = %d"),DiceCount,Extra);
            AddLog(FString::Printf(TEXT("Ataque furtivo! +%dd6 = %d de dano"),DiceCount,Extra));
        }
    }
    // Hunter: Colossus Slayer. Once per turn, against something already bleeding.
    if(HeroClass==EAHHeroClass::Ranger && Level>=3 && !bColossusUsed
       && Target->Health<Target->MaxHealth)
    {
        bColossusUsed=true;
        const int32 Extra=Dice.RandRange(1,8);
        Roll.Damage+=Extra;
        AddLog(FString::Printf(TEXT("Matador de colossos: +1d8 = %d"),Extra));
        UE_LOG(LogTemp, Display, TEXT("AH_SUBCLASS colossos +%d"), Extra);
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
