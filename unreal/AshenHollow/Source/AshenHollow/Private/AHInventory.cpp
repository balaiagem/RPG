#include "AHCharacter.h"
#include "AHGameMode.h"
#include "Engine/World.h"

bool AAHCharacter::DropBackpackOnSlot(int32 Index,EAHSlot Slot)
{
    if(!Backpack.IsValidIndex(Index) || Slot==EAHSlot::Nenhum || Slot>=EAHSlot::Count) return false;
    const auto* Item=AHItems::Find(Backpack[Index].Id);
    if(!Item || Item->Slot!=Slot) { Feedback=TEXT("Esse item nao cabe neste espaco."); return false; }
    return UseBackpackItem(Index);
}

int32 AAHCharacter::WeaponDice() const
{
    const auto* Item=bEnemy?nullptr:AHItems::Find(Equipped[static_cast<int32>(EAHSlot::MaoPrincipal)]);
    return Item?Item->DiceCount:1;
}

int32 AAHCharacter::ConcentrationModifier() const
{
    if(bEnemy) return AHRules::Class(HeroClass).ConSaveModifier;
    return AHSheet::Total(Abilities,Ancestry).Mod(EAHAbility::Constituicao)
        +(AHSheet::SavesWith(HeroClass,EAHAbility::Constituicao)?AHSheet::Proficiency(Level):0);
}

bool AAHCharacter::UseBackpackItem(int32 Index)
{
    if(bEnemy || !bCharacterReady || !IsAlive() || IsBusy() || !Backpack.IsValidIndex(Index)) return false;
    const FString Id=Backpack[Index].Id;
    const FAHItemData* Item=AHItems::Find(Id);
    if(!Item || Backpack[Index].Many<=0) return false;
    const auto* Mode=GetWorld()->GetAuthGameMode<AAHGameMode>();
    if(Item->Cura>0)
    {
        if(Health>=MaxHealth) { Feedback=TEXT("Vida cheia: a pocao continua na mochila."); return false; }
        if(Mode && !Mode->IsExploring() && (!CanAct() || !Turn.SpendAction()))
            { Feedback=TEXT("Beber exige sua acao neste turno."); return false; }
        ApplyHealing(Item->Cura);
        if(--Backpack[Index].Many==0) Backpack.RemoveAt(Index);
        Feedback=FString::Printf(TEXT("%s: +%d PV"),Item->Nome,Item->Cura);
        AddLog(Feedback); return true;
    }
    if(Item->Slot==EAHSlot::Nenhum)
        { Feedback=TEXT("Este item fica guardado na mochila."); return false; }
    if(Mode && !Mode->IsExploring())
        { Feedback=TEXT("Troque equipamentos fora de combate."); return false; }
    if(Equipped[static_cast<int32>(Item->Slot)]==Id) return false;
    if(Item->Slot==EAHSlot::MaoSecundaria)
    {
        const auto* Main=AHItems::Find(Equipped[static_cast<int32>(EAHSlot::MaoPrincipal)]);
        if(Main && Main->bDuasMaos)
            { Feedback=TEXT("Guarde a arma de duas maos antes de equipar o escudo."); return false; }
    }
    // Copy and remove before Equip: putting away an off-hand item can resize
    // the pack, so no array reference may survive the equipment transaction.
    if(--Backpack[Index].Many==0) Backpack.RemoveAt(Index);
    const FString Previous=Equip(Id);
    if(!Previous.IsEmpty()) Carry(Previous);
    Feedback=FString::Printf(TEXT("Equipado: %s"),Item->Nome);
    AddLog(Feedback); return true;
}
