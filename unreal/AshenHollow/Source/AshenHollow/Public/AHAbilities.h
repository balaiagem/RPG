#pragma once

#include "CoreMinimal.h"
#include "AHClassData.h"
#include "AHItems.h"

/**
 * The six ability scores, and everything a character sheet derives from them.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * Until now every number that decides a fight was a constant typed into
 * `FAHClassSheet`: the fighter had AC 16, attack +5, damage +3 and 12 hit
 * points because those are the numbers I chose, and no choice the player made
 * at creation could move any of them. That is a game with D&D's vocabulary and
 * none of its arithmetic -- and Lucas asked for the arithmetic: "quero fazermos
 * a criacao de personagem mais interessante assim como dnd 5e, com ficha de
 * personagem".
 *
 * So the flat numbers become DERIVED numbers. Armour class comes from what you
 * are wearing plus your Dexterity; the attack bonus is proficiency plus the
 * ability the weapon uses; hit points are the hit die plus Constitution. The
 * class table keeps its constants as the fallback for anybody who has no
 * abilities rolled -- every foe in the world, for now -- so nothing that works
 * today stops working.
 *
 * EVERYTHING HERE IS PURE ARITHMETIC ON PURPOSE. No actors, no engine types
 * beyond int32 -- which means `Scripts/preview/ficha.cpp` compiles it with g++
 * and prints the whole derived sheet for every class and every ancestry
 * without opening Unreal. A rules change that shifts the fighter's AC by two
 * is a thing I can see in forty seconds instead of after a build.
 */

enum class EAHAbility : uint8
{
    Forca, Destreza, Constituicao, Inteligencia, Sabedoria, Carisma, Count
};

/**
 * One character's six scores, before or after the ancestry's bonus.
 *
 * Stored as the raw 3-to-20 score rather than as the modifier, because the
 * sheet shows both and the player buys the score. The modifier is the score
 * minus ten, halved, rounded DOWN -- and rounding down is why this is a
 * function and not a subtraction: for a score of 9 the answer is -1, and
 * integer division in C++ rounds towards zero, which would say 0.
 */
struct FAHAbilities
{
    int32 Score[static_cast<int32>(EAHAbility::Count)] = { 10, 10, 10, 10, 10, 10 };

    int32 Raw(EAHAbility Which) const
    {
        const int32 Slot = static_cast<int32>(Which);
        return (Slot >= 0 && Slot < static_cast<int32>(EAHAbility::Count)) ? Score[Slot] : 10;
    }
    void Set(EAHAbility Which, int32 Value)
    {
        const int32 Slot = static_cast<int32>(Which);
        if (Slot >= 0 && Slot < static_cast<int32>(EAHAbility::Count)) Score[Slot] = Value;
    }
    /** The modifier: (score - 10) / 2, rounded DOWN, so 9 gives -1 and not 0. */
    int32 Mod(EAHAbility Which) const
    {
        const int32 Value = Raw(Which) - 10;
        return (Value >= 0) ? Value / 2 : -(( -Value + 1) / 2);
    }
    void Add(const int32* Six)
    {
        if (!Six) return;
        for (int32 I = 0; I < static_cast<int32>(EAHAbility::Count); ++I) Score[I] += Six[I];
    }
};

namespace AHSheet
{
    // ── Point buy ────────────────────────────────────────────────────────
    /**
     * Twenty-seven points, and the SRD's own price list.
     *
     * The prices are not linear and that is the whole design of point buy: the
     * step from 13 to 14 costs two points and the step from 14 to 15 costs two
     * more, so a 15 costs nine of your twenty-seven. It is what stops everybody
     * building the same character with three 15s.
     */
    static constexpr int32 BuyBudget = 27;
    static constexpr int32 BuyFloor  = 8;
    static constexpr int32 BuyCeil   = 15;

    inline int32 BuyCost(int32 Value)
    {
        static const int32 Price[] = { 0, 1, 2, 3, 4, 5, 7, 9 };   // 8 .. 15
        if (Value <= BuyFloor) return 0;
        if (Value >  BuyCeil)  return 999;                          // not buyable
        return Price[Value - BuyFloor];
    }
    inline int32 Spent(const FAHAbilities& Sheet)
    {
        int32 Total = 0;
        for (int32 I = 0; I < static_cast<int32>(EAHAbility::Count); ++I)
            Total += BuyCost(Sheet.Score[I]);
        return Total;
    }
    inline bool Legal(const FAHAbilities& Sheet)
    {
        for (int32 I = 0; I < static_cast<int32>(EAHAbility::Count); ++I)
            if (Sheet.Score[I] < BuyFloor || Sheet.Score[I] > BuyCeil) return false;
        return Spent(Sheet) <= BuyBudget;
    }

    /** The short name the sheet prints: FOR, DES, CON, INT, SAB, CAR. */
    const TCHAR* Short(EAHAbility Which);
    /** The full name, for the tooltip. */
    const TCHAR* Long(EAHAbility Which);
    /** What this ability actually does in THIS game, in one line. */
    const TCHAR* Explains(EAHAbility Which);

    // ── Ancestry ─────────────────────────────────────────────────────────
    /**
     * The SRD's racial bonuses, six numbers per ancestry.
     *
     * Applied AFTER point buy, which is why the buy limit is 15 and a starting
     * score can still be 17: the human gets +1 to everything, the elf +2
     * Dexterity and +1 Intelligence, and so on.
     */
    const int32* AncestryBonus(EAHAncestry Which);

    // ── Class ────────────────────────────────────────────────────────────
    /** Hit die: 12 for the barbarian, 6 for the wizard. */
    int32 HitDie(EAHHeroClass Which);
    /** The ability this class attacks and saves with first. */
    EAHAbility Primary(EAHHeroClass Which);
    /** The ability its spells use. Meaningless for a class that has none. */
    EAHAbility Casting(EAHHeroClass Which);
    /** True when this class is proficient in saving throws with that ability. */
    bool SavesWith(EAHHeroClass Which, EAHAbility Ability);
    /**
     * A legal 27-point spread for this class, before the ancestry's bonus.
     *
     * Not a shortcut for lazy players: it is what the creation screen starts
     * from, so somebody who does not want to think about point buy still gets
     * a character that works, and somebody who does can see what a sensible
     * spread looks like before he moves it.
     */
    const int32* Recommended(EAHHeroClass Which);

    /** Proficiency bonus. +2 for levels 1 to 4, which is all this game has. */
    inline int32 Proficiency(int32 Level) { return 2 + FMath::Max(0, (Level - 1) / 4); }

    // ── The derived sheet ────────────────────────────────────────────────
    /**
     * Everything the fight reads, worked out from the abilities and the kit.
     *
     * A plain struct returned by a free function, deliberately: it means the
     * whole of this game's character arithmetic can be compiled by g++ and
     * printed as a table by `Scripts/preview/ficha.cpp`, and a change that
     * quietly gives the barbarian AC 20 is something I can see in forty
     * seconds. `AAHCharacter::RecomputeSheet` does nothing but call this and
     * copy the answers into the fields the rest of the game already reads --
     * which is what keeps a hundred call sites from having to learn about
     * ability scores.
     */
    struct FAHDerived
    {
        int32 ArmorClass      = 10;
        int32 AttackBonus     = 0;
        int32 DamageSides     = 4;
        int32 DamageDice      = 1;
        int32 DamageModifier  = 0;
        int32 InitiativeBonus = 0;
        int32 MaxHealth       = 1;
        /** Zero for a melee weapon; centimetres for a bow. */
        int32 RangedRange     = 0;
        int32 RangedSides     = 0;
        float Movement        = 900.f;
        int32 SpellDC         = 10;
        int32 SpellAttack     = 0;
        int32 ConSave         = 0;
        /** Which ability the weapon in hand actually attacks with. */
        EAHAbility AttackWith = EAHAbility::Forca;
        EAHWeaponKind Art     = EAHWeaponKind::Sword;
        /** True when the armour worn is heavier than this Strength can carry. */
        bool  bOverloaded     = false;
    };

    /**
     * Works the sheet out. Equipped is EAHSlot::Count ids, empty for nothing.
     *
     * Falls back to the class table's hand-tuned constants wherever the
     * character has no equipment at all, which is every enemy in the world
     * today: a foe with no kit fights exactly as it did before this file
     * existed, so nothing that works stops working on the day this lands.
     */
    FAHDerived Derive(EAHHeroClass Class, EAHAncestry Blood, int32 Level,
                      const FAHAbilities& Bought, const FString* Equipped);

    /** What the sheet shows: bought plus the ancestry's bonus. */
    inline FAHAbilities Total(const FAHAbilities& Bought, EAHAncestry Blood)
    {
        FAHAbilities Sum = Bought;
        Sum.Add(AncestryBonus(Blood));
        return Sum;
    }
}
