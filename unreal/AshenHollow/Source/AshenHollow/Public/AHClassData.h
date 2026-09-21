#pragma once

#include "CoreMinimal.h"

/**
 * Archetype and ancestry data.
 *
 * These used to live as switch cases and four-element arrays scattered across
 * AHCharacter, AHProgression, AHCombatHUD and AHEquipmentComponent, indexed by
 * the class enum. Adding a fifth class would have read past the end of every one
 * of those arrays -- which does not crash, it returns garbage. One table per
 * concept instead, so a new archetype is a row and the compiler keeps count.
 */

enum class EAHHeroClass : uint8 { Fighter, Barbarian, Cleric, Wizard, Count };
enum class EAHAncestry  : uint8 { Human, Elf, Dwarf, Halfling, HalfOrc, Tiefling, Dragonborn, Count };

/**
 * Damage types. This started life as a bool called bPhysical, which could say
 * "rage halves this" and nothing else. Fire resistance needs a real type.
 */
enum class EAHDamageType : uint8 { Physical, Fire, Force };

/** Which authored attack clip and weapon model an archetype uses. */
enum class EAHWeaponKind : uint8 { Sword, Axe, Mace, Staff, Count };

struct FAHClassSheet
{
    const TCHAR* Name;                 // GUERREIRO
    const TCHAR* FoeName;              // shown when this archetype is rolled for an enemy
    const TCHAR* AbilityName;          // action bar button
    const TCHAR* AbilityDescription;   // action bar tooltip
    const TCHAR* ProgressionName;      // level 2 ability
    const TCHAR* SelectionAbility;     // character creation: ability headline
    const TCHAR* SelectionDetail;      // character creation: cost and effect
    const TCHAR* SelectionPassive;     // character creation: flavour line

    int32 MaxHealth;
    int32 ArmorClass;
    int32 AttackBonus;
    int32 DamageSides;
    int32 DamageModifier;
    int32 InitiativeBonus;
    int32 ClassCharges;                // uses of the level 1 ability per encounter
    int32 HitPointGrowth;              // gained per level after the first
    bool  bCaster;                     // has spell slots

    EAHWeaponKind Weapon;
    const TCHAR* Icon;                 // AHCombatHUD::Icon() key
    FLinearColor Colour;

    // Ranged option. RangedRange of 0 means this archetype only fights in melee.
    int32 RangedRange;                 // centimetres
    int32 RangedSides;                 // damage die
    int32 RangedBonus;
    const TCHAR* RangedName;
};

struct FAHAncestrySheet
{
    const TCHAR* Name;                 // HUMANO
    const TCHAR* Trait;                // full line, used under the class header
    const TCHAR* SelectionPassive;     // short line on the ancestry card

    float Movement;                    // centimetres per turn
    float BodyScale;
    int32 InitiativeBonus;
    int32 ArmorBonus;
    int32 HealthPerLevel;              // dwarven toughness
    int32 DamageBonus;                 // heavier build hits harder
    bool  bLucky;                      // rerolls a natural 1
    bool  bFireResistant;              // halves fire damage
    bool  bRelentless;                 // refuses one drop to 0 HP per rest
    bool  bBreathWeapon;               // gains the draconic breath action
};

namespace AHRules
{
    const FAHClassSheet&    Class(EAHHeroClass Which);
    const FAHAncestrySheet& Ancestry(EAHAncestry Which);
    int32 ClassCount();
    int32 AncestryCount();
}
