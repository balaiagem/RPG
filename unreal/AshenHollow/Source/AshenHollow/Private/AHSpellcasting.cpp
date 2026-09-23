#include "AHCharacter.h"
#include "AHGameMode.h"
#include "AHCombatBurst.h"
#include "AHMagicVisual.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

void AAHCharacter::InitializeSpellbook()
{
    PreparedSpells.Reset();
    if(!AHRules::Class(HeroClass).bCaster) return;
    for(int32 I=0;I<AHSpells::Count();++I)
    {
        const auto& S=AHSpells::Get(static_cast<EAHSpell>(I));
        if(AHSpells::ForClass(S.Id,HeroClass) && IsSpellAvailable(S.Id) && S.Rank==1 && PreparedSpells.Num()<PreparedLimit()) PreparedSpells.Add(S.Id);
    }
    SelectedSpell=(HeroClass==EAHHeroClass::Cleric || HeroClass==EAHHeroClass::Paladin)?EAHSpell::CureWounds:HeroClass==EAHHeroClass::Ranger?EAHSpell::HuntersMark:EAHSpell::MagicMissile;
}
bool AAHCharacter::IsSpellAvailable(EAHSpell Id) const
{
    if(static_cast<int32>(Id)>=AHSpells::Count()) return false;
    const auto& S=AHSpells::Get(Id);
    return AHSpells::ForClass(Id,HeroClass) && (S.Rank==0 || MaxSpellSlots(S.Rank)>0);
}
bool AAHCharacter::TogglePreparedSpell(EAHSpell Id)
{
    if(!bPreparingSpells || !IsSpellAvailable(Id) || AHSpells::Get(Id).Rank==0) return false;
    if(PreparedSpells.Contains(Id)) { PreparedSpells.Remove(Id); return true; }
    if(PreparedSpells.Num()>=PreparedLimit()) { Feedback=TEXT("Limite de preparacao: remova uma magia primeiro"); return false; }
    PreparedSpells.Add(Id); return true;
}
bool AAHCharacter::SelectSpell(EAHSpell Id)
{
    if(!IsSpellAvailable(Id) || (AHSpells::Get(Id).Rank>0 && !PreparedSpells.Contains(Id))) return false;
    SelectedSpell=Id;
    if(AHSpells::Get(Id).Rank==2) SelectedSpellLevel=2;
    return true;
}
bool AAHCharacter::HasGuidingMark() const
{
    return GuidingSource.IsValid() && GuidingSource->TurnsStarted<=GuidingExpiresTurn;
}
bool AAHCharacter::CastSpell(EAHSpell Id, AAHCharacter* Target)
{
    if(!CanAct() || bPreparingSpells || !IsSpellAvailable(Id)) return false;
    const auto& S=AHSpells::Get(Id);
    if(S.Rank>0 && (!PreparedSpells.Contains(Id) || SelectedSpellLevel<S.Rank || !HasSpellSlot()))
    { Feedback=TEXT("Magia nao preparada ou espaco indisponivel no circulo escolhido"); return false; }
    if(S.bBonus ? (!Turn.bBonus || bLeveledActionSpellCast) : (!Turn.bAction || (S.Rank>0 && bBonusSpellCast)))
    { Feedback=TEXT("Acao indisponivel ou restricao de magia de bonus neste turno"); return false; }
    if(S.bHostile)
    {
        if(!Target) { Feedback=TEXT("Escolha um inimigo visivel para conjurar"); return false; }
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellCast),false,this); Query.AddIgnoredActor(Target);
        if(!IsValid(Target) || !Target->IsAlive() || Target->bEnemy==bEnemy || FVector::Dist2D(GetActorLocation(),Target->GetActorLocation())>S.Range ||
           GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Query))
        { Feedback=TEXT("Alvo invalido, obstruido ou fora do alcance"); return false; }
    }
    else
    {
        Target=this;
        if((Id==EAHSpell::CureWounds || Id==EAHSpell::HealingWord) && Health>=MaxHealth) { Feedback=TEXT("Vida cheia"); return false; }
        if((Id==EAHSpell::Aid && AidBonus>0) || (Id==EAHSpell::MageArmor && MageArmorBonus>0) || (Id==EAHSpell::ShieldOfFaith && GuardTurns>0) || (Id==EAHSpell::Bless && BlessTurns>0))
        { Feedback=TEXT("Efeito ja ativo"); return false; }
    }
    const bool Empower=bEmpowerNext && S.bHostile && Id!=EAHSpell::HuntersMark;
    if(Empower && SorceryPoints<1) { Feedback=TEXT("Sem pontos de feiticaria"); return false; }
    PendingEmpowerRerolls=Empower?3:0;
    if(Empower) { --SorceryPoints; bEmpowerNext=false; }
    if(S.bBonus) { Turn.SpendBonus(); bBonusSpellCast=true; }
    else { Turn.SpendAction(); if(S.Rank>0) bLeveledActionSpellCast=true; }
    if(S.Rank>0) SpendSpellSlot();
    if(GetController()) GetController()->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    Feedback=FString(S.Name)+TEXT("..."); AddLog(Feedback);
    if(Id==EAHSpell::HuntersMark)
    {
        if(GuardTurns>0) ArmorClass-=2;
        GuardTurns=BlessTurns=0; MarkedTarget=Target; MarkTurns=600;
        PlayGesture(CastAnimation); return true;
    }
    if(S.bHostile)
    {
        SetActorRotation(FRotator(0,(Target->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0));
        PendingTarget=Target; PendingSpellId=static_cast<int32>(Id); PendingSpellRank=S.Rank>0?SelectedSpellLevel:0;
        PendingRange=S.Range; PlayAttack(); MotionLabel=S.Name;
        return true;
    }
    const int32 Rank=SelectedSpellLevel;
    if(Id==EAHSpell::CureWounds || Id==EAHSpell::HealingWord)
    {
        int32 Amount=3; for(int32 I=0;I<Rank;++I) Amount+=Dice.RandRange(1,Id==EAHSpell::CureWounds?8:4);
        Amount=ApplyHealing(Amount);
        ImpactText=FString::Printf(TEXT("+%d PV"),Amount); ImpactTextTime=GetWorld()->GetTimeSeconds(); bImpactHealing=true;
    }
    else if(Id==EAHSpell::ShieldOfFaith) { MarkTurns=0; MarkedTarget.Reset(); BlessTurns=0; GuardTurns=100; ArmorClass+=2; }
    else if(Id==EAHSpell::Bless) { MarkTurns=0; MarkedTarget.Reset(); if(GuardTurns>0) ArmorClass-=2; GuardTurns=0; BlessTurns=10; }
    else if(Id==EAHSpell::Goodberry) Goodberries+=10;
    else if(Id==EAHSpell::Aid) { AidBonus=5; MaxHealth+=5; Health+=5; }
    else if(Id==EAHSpell::FalseLife) TempHP=FMath::Max(TempHP,Dice.RandRange(1,4)+4+5*(Rank-1));
    else if(Id==EAHSpell::MageArmor) { MageArmorBonus=3; ArmorClass+=MageArmorBonus; }
    PlayGesture((Id==EAHSpell::CureWounds || Id==EAHSpell::HealingWord)?HealAnimation.Get():CastAnimation.Get());
    AAHCombatBurst::Emit(GetWorld(),GetActorLocation(),EAHBurst::Heal);
    return true;
}
void AAHCharacter::ResolveSpellImpact()
{
    const auto Id=static_cast<EAHSpell>(PendingSpellId); const int32 Rank=PendingSpellRank;
    int32 Rerolls=PendingEmpowerRerolls; PendingEmpowerRerolls=0;
    PendingSpellId=-1; PendingRange=0;
    auto* Target=PendingTarget.Get(); PendingTarget=nullptr;
    if(!IsAlive() || !IsValid(Target) || !Target->IsAlive()) return;
    const auto& S=AHSpells::Get(Id);
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SpellImpact),false,this); Query.AddIgnoredActor(Target);
    if(FVector::Dist2D(GetActorLocation(),Target->GetActorLocation())>S.Range || GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Query))
    { AddLog(TEXT("Magia interrompida: alvo fora de alcance ou obstruido")); return; }
    int32 TotalDamage=0;
    if(Id==EAHSpell::MagicMissile)
    {
        TotalDamage=(2+Rank)*(SpellDamageDie(4,Rerolls)+1);
        Target->ReceiveHit(TotalDamage,EAHDamageType::Force); LastRollTime=-100.f;
    }
    else if(Id==EAHSpell::BurningHands || Id==EAHSpell::Thunderwave)
    {
        int32 Damage=0; const int32 Count=Id==EAHSpell::BurningHands?2+Rank:1+Rank;
        for(int32 I=0;I<Count;++I) Damage+=SpellDamageDie(Id==EAHSpell::BurningHands?6:8,Rerolls);
        const FVector Forward=GetActorForwardVector(),Side=GetActorRightVector();
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
        {
            auto* Victim=*It; if(Victim==this || !Victim->IsAlive()) continue;
            const FVector Delta=Victim->GetActorLocation()-GetActorLocation();
            const float Along=FVector::DotProduct(Delta,Forward),Across=FMath::Abs(FVector::DotProduct(Delta,Side));
            if(Along<0 || Along>450 || FMath::Abs(Delta.Z)>225 || Across>(Id==EAHSpell::BurningHands?Along*.5f:225.f)) continue;
            FHitResult Block; FCollisionQueryParams Q(SCENE_QUERY_STAT(SpellArea),false,this); Q.AddIgnoredActor(Victim);
            if(GetWorld()->LineTraceSingleByChannel(Block,GetActorLocation(),Victim->GetActorLocation(),ECC_Visibility,Q)) continue;
            const int32 Modifier=Id==EAHSpell::BurningHands?AHRules::Class(Victim->HeroClass).InitiativeBonus:2;
            auto Save=UAHDiceRules::RollCheck(Dice,Modifier+(Victim->BlessTurns>0?Dice.RandRange(1,4):0),SpellSaveDC(),Id==EAHSpell::BurningHands && Victim->bDodging?1:0,Victim->IsLucky());
            Victim->ReceiveHit(Save.bSuccess?Damage/2:Damage,Id==EAHSpell::BurningHands?EAHDamageType::Fire:EAHDamageType::Thunder);
            TotalDamage+=Victim->LastDamage;
            if(Id==EAHSpell::Thunderwave && !Save.bSuccess && Victim->IsAlive())
            {
                // Swept displacement cannot pass through walls and is not voluntary movement.
                const FVector Away=Delta.GetSafeNormal2D();
                Victim->SetActorLocation(Victim->GetActorLocation()+Away*300.f,true);
                Victim->PreviousLocation=Victim->GetActorLocation();
            }
            LastRoll=Save; LastRoll.Damage=Victim->LastDamage; LastRollLabel=TEXT("ALVO / SALVAGUARDA"); LastRollTime=GetWorld()->GetTimeSeconds();
        }
    }
    else if(Id==EAHSpell::SacredFlame)
    {
        // Dexterity modifier is currently the archetype's initiative modifier,
        // excluding ancestry and feat bonuses.
        auto Save=UAHDiceRules::RollCheck(Dice,AHRules::Class(Target->HeroClass).InitiativeBonus+(Target->BlessTurns>0?Dice.RandRange(1,4):0),SpellSaveDC(),Target->bDodging?1:0,Target->IsLucky());
        if(!Save.bSuccess) { TotalDamage=SpellDamageDie(8,Rerolls); Target->ReceiveHit(TotalDamage,EAHDamageType::Radiant); }
        LastRoll=Save; LastRoll.Damage=TotalDamage; LastRollLabel=TEXT("ALVO / SALVAGUARDA DES"); LastRollTime=GetWorld()->GetTimeSeconds();
    }
    else
    {
        bool bThreatened=false;
        for(TActorIterator<AAHCharacter> It(GetWorld());It;++It)
            if(It->bEnemy!=bEnemy && It->IsAlive() && FVector::Dist2D(GetActorLocation(),It->GetActorLocation())<150.f) bThreatened=true;
        const int32 Rays=Id==EAHSpell::ScorchingRay?3:1;
        for(int32 I=0;I<Rays && Target->IsAlive();++I)
        {
            const bool Advantage=Target->bReckless || Target->HasGuidingMark();
            const bool Disadvantage=Target->bDodging || (Id!=EAHSpell::InflictWounds && bThreatened);
            const int32 Count=Id==EAHSpell::InflictWounds?2+Rank:Id==EAHSpell::GuidingBolt?3+Rank:Id==EAHSpell::ScorchingRay?2:1;
            const int32 Sides=(Id==EAHSpell::FireBolt || Id==EAHSpell::InflictWounds)?10:Id==EAHSpell::RayOfFrost?8:6;
            auto Roll=UAHDiceRules::RollAttack(Dice,SpellAttackBonus()+(BlessTurns>0?Dice.RandRange(1,4):0),Target->ArmorClass,0,Sides,0,(Advantage?1:0)-(Disadvantage?1:0),IsLucky());
            if(Roll.bSuccess) for(int32 D=0;D<Count*(Roll.bCritical?2:1);++D) Roll.Damage+=SpellDamageDie(Sides,Rerolls);
            Target->GuidingSource.Reset();
            if(Roll.bSuccess)
            {
                Target->ReceiveHit(Roll.Damage,Id==EAHSpell::InflictWounds?EAHDamageType::Necrotic:Id==EAHSpell::GuidingBolt?EAHDamageType::Radiant:Id==EAHSpell::RayOfFrost?EAHDamageType::Cold:EAHDamageType::Fire);
                TotalDamage+=Target->LastDamage;
                if(Id==EAHSpell::GuidingBolt) { Target->GuidingSource=this; Target->GuidingExpiresTurn=TurnsStarted+1; }
                if(Id==EAHSpell::RayOfFrost) { Target->FrostTurns=1; Target->Turn.Movement=FMath::Max(0.f,Target->Turn.Movement-300.f); }
            }
            LastRoll=Roll; LastRollLabel=S.Name; LastRollTime=GetWorld()->GetTimeSeconds();
            AddLog(FString::Printf(TEXT("%s: d20 %d +5 / %d dano"),S.Name,Roll.NaturalRoll,Roll.Damage));
        }
    }
    Target->ImpactText=TotalDamage>0?FString::Printf(TEXT("-%d"),TotalDamage):TEXT("RESISTIU");
    Target->ImpactTextTime=GetWorld()->GetTimeSeconds(); Target->bImpactHealing=false;
    Feedback=FString::Printf(TEXT("%s: %d dano"),S.Name,TotalDamage); AddLog(Feedback);
    AAHCombatBurst::Emit(GetWorld(),Target->GetActorLocation(),EAHBurst::Force);
}
