#pragma once

#include "CoreMinimal.h"
#include "AHClassData.h"

/**
 * Everything a character can carry, wear or swing.
 *
 * WHY A TABLE AND NOT A CLASS PER ITEM. The whole project's answer to "a
 * number that lives in two places is already wrong" is a declared table with a
 * static_assert on its length, and an item is nothing but numbers: a die size,
 * an armour base, a Dexterity cap. `AHKitTable.h` did it for meshes and it is
 * the reason the world stopped putting houses in the road; this is the same
 * idea for the sheet.
 *
 * WHAT AN ITEM IS ALLOWED TO DO. It changes exactly the things
 * `AAHCharacter::RecomputeSheet` reads: armour class, the attack ability, the
 * damage die, reach, movement. Nothing here has a script, a trigger or an
 * event -- an item is data, and the character is what applies it. That is what
 * keeps the equipment screen from becoming a second combat system.
 */

/** Where a thing is worn. `Nenhum` is a thing that only sits in the pack. */
enum class EAHSlot : uint8
{
    Nenhum, MaoPrincipal, MaoSecundaria, Armadura, Elmo, Botas, Anel, Count
};

enum class EAHItemKind : uint8
{
    Arma, Armadura, Escudo, Vestimenta, Joia, Consumivel, Miudeza, Count
};

struct FAHItemData
{
    const TCHAR* Id;        // "espada_longa": what is stored and looked up
    const TCHAR* Nome;      // "Espada longa": what the screen shows
    const TCHAR* Linha;     // one line, shown under the name

    EAHItemKind  Kind;
    EAHSlot      Slot;

    // ── Weapon ───────────────────────────────────────────────────────────
    EAHWeaponKind Arte;     // which model and which swing animation
    int32 Dano;             // die sides; 0 for anything that is not a weapon
    int32 Alcance;          // centimetres; 0 means melee
    bool  bSutil;           // finesse: may attack with Dexterity instead
    bool  bDuasMaos;        // two-handed: blocks the off hand

    // ── Armour and trinkets ──────────────────────────────────────────────
    int32 Base;             // armour class BEFORE Dexterity; 0 = not armour
    int32 TetoDestreza;     // most Dexterity it will let you add; -1 = no cap
    int32 Bonus;            // flat AC on top: shields, helmets, rings
    int32 ForcaMinima;      // heavy armour you cannot move properly in without

    // ── The rest ─────────────────────────────────────────────────────────
    int32 Movimento;        // centimetres added to the walk, or taken away
    int32 DanoBonus;        // flat damage, for the one ring that gives it
    int32 Cura;             // a potion's healing, as a fixed amount
    int32 Preco;            // gold pieces, for the day there is a shop
    int32 DiceCount = 1;
};

namespace AHItems
{
    /** How many rows the catalogue has. */
    int32 Count();
    /** One row by index, for walking the whole catalogue. */
    const FAHItemData& At(int32 Index);
    /** One row by id, or nullptr. Ids are what a character stores. */
    const FAHItemData* Find(const FString& Id);
    /** The empty row: everything zero. Returned rather than a null check. */
    const FAHItemData& Nothing();

    /**
     * The starting kits, two per class.
     *
     * The SRD hands out equipment as "(a) chain mail or (b) leather armour, a
     * longbow and twenty arrows", and that choice is most of what makes two
     * fighters different on the first morning. Two per class rather than the
     * SRD's four-way trees: the point is a real decision, not a form.
     */
    struct FAHKit
    {
        const TCHAR* Nome;      // "SOLDADO DE LINHA"
        const TCHAR* Linha;     // what it plays like, in one line
        const TCHAR* Itens[8];  // ids; a nullptr ends the list early
    };
    /** Kit Which (0 or 1) for that class. */
    const FAHKit& Kit(EAHHeroClass Class, int32 Which);
    static constexpr int32 KitsPerClass = 2;
}
