#pragma once
#include "CoreMinimal.h"
#include "AHClassData.h"
enum class EAHSpell : uint8 { SacredFlame, CureWounds, HealingWord, GuidingBolt, ShieldOfFaith, Aid, FireBolt, RayOfFrost, MagicMissile, FalseLife, MageArmor, ScorchingRay, InflictWounds, Bless, BurningHands, Thunderwave, HuntersMark, Goodberry, Count };
struct FAHSpellDefinition
{
    EAHSpell Id;
    EAHHeroClass Class;
    const TCHAR* Name;
    const TCHAR* Description;
    int32 Rank;
    bool bBonus;
    bool bHostile;
    float Range;
};
namespace AHSpells { const FAHSpellDefinition& Get(EAHSpell Id); int32 Count(); bool ForClass(EAHSpell Id, EAHHeroClass Class); }
