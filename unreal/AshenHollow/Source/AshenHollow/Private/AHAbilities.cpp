#include "AHAbilities.h"

/**
 * The tables. One row per enum value, and a static_assert on every one of
 * them, for the reason AHClassData.cpp gives: a table indexed by an enum that
 * is one row short does not crash, it returns whatever is in memory after it.
 */
namespace
{
    constexpr int32 GAbilityCount = static_cast<int32>(EAHAbility::Count);
    constexpr int32 GClassCount   = static_cast<int32>(EAHHeroClass::Count);
    constexpr int32 GBloodCount   = static_cast<int32>(EAHAncestry::Count);

    const TCHAR* const GShort[GAbilityCount] =
    { TEXT("FOR"), TEXT("DES"), TEXT("CON"), TEXT("INT"), TEXT("SAB"), TEXT("CAR") };

    const TCHAR* const GLong[GAbilityCount] =
    { TEXT("Forca"), TEXT("Destreza"), TEXT("Constituicao"),
      TEXT("Inteligencia"), TEXT("Sabedoria"), TEXT("Carisma") };

    /**
     * What each one buys, in this game, in the player's own terms.
     *
     * Written as what it DOES rather than what it is, because "Destreza:
     * agilidade e reflexos" tells a new player nothing about whether to spend
     * nine points on it. These lines are the whole of the tutorial this screen
     * gets.
     */
    const TCHAR* const GExplains[GAbilityCount] =
    {
        TEXT("Ataque e dano com armas pesadas. Empurrar, arrombar, agarrar."),
        TEXT("Classe de armadura, iniciativa, armas leves e arcos, furtividade."),
        TEXT("Pontos de vida em cada nivel, e resistir a veneno e a exaustao."),
        TEXT("Magia do mago. Saber o que aquela runa quer dizer."),
        TEXT("Magia do clerigo e do patrulheiro. Perceber o que esta errado."),
        TEXT("Magia do feiticeiro e do paladino. Convencer, mentir, liderar."),
    };

    /** SRD 5.1 ability score increases, in EAHAncestry order. */
    const int32 GBlood[GBloodCount][GAbilityCount] =
    {
        { 1, 1, 1, 1, 1, 1 },   // Humano: versatil, +1 em tudo
        { 0, 2, 0, 1, 0, 0 },   // Elfo (alto): DES +2, INT +1
        { 0, 0, 2, 0, 1, 0 },   // Anao (da colina): CON +2, SAB +1
        { 0, 2, 0, 0, 0, 1 },   // Halfling (pes leves): DES +2, CAR +1
        { 2, 0, 1, 0, 0, 0 },   // Meio-orc: FOR +2, CON +1
        { 0, 0, 0, 1, 0, 2 },   // Tiefling: CAR +2, INT +1
        { 2, 0, 0, 0, 0, 1 },   // Draconato: FOR +2, CAR +1
    };
    static_assert(sizeof(GBlood) / sizeof(GBlood[0]) == GBloodCount,
                  "uma linha de bonus por ancestralidade");

    /** Hit dice, in EAHHeroClass order. */
    const int32 GHitDie[GClassCount] =
    {
        10,  // Guerreiro
        12,  // Barbaro
        8,   // Clerigo
        6,   // Mago
        6,   // Feiticeiro
        8,   // Ladino
        10,  // Paladino
        10,  // Patrulheiro
    };
    static_assert(sizeof(GHitDie) / sizeof(GHitDie[0]) == GClassCount,
                  "um dado de vida por classe");

    const EAHAbility GPrimary[GClassCount] =
    {
        EAHAbility::Forca,        // Guerreiro
        EAHAbility::Forca,        // Barbaro
        EAHAbility::Sabedoria,    // Clerigo
        EAHAbility::Inteligencia, // Mago
        EAHAbility::Carisma,      // Feiticeiro
        EAHAbility::Destreza,     // Ladino
        EAHAbility::Forca,        // Paladino
        EAHAbility::Destreza,     // Patrulheiro
    };
    static_assert(sizeof(GPrimary) / sizeof(GPrimary[0]) == GClassCount,
                  "uma habilidade principal por classe");

    /**
     * The casting ability. A class with no spells still has a row, because a
     * table with holes in it is a table somebody will index into by accident.
     */
    const EAHAbility GCasting[GClassCount] =
    {
        EAHAbility::Forca,        // Guerreiro: nenhuma
        EAHAbility::Forca,        // Barbaro: nenhuma
        EAHAbility::Sabedoria,    // Clerigo
        EAHAbility::Inteligencia, // Mago
        EAHAbility::Carisma,      // Feiticeiro
        EAHAbility::Destreza,     // Ladino: nenhuma
        EAHAbility::Carisma,      // Paladino
        EAHAbility::Sabedoria,    // Patrulheiro
    };
    static_assert(sizeof(GCasting) / sizeof(GCasting[0]) == GClassCount,
                  "uma habilidade de conjuracao por classe");

    /** The two saving throws each class is proficient in (SRD 5.1). */
    const EAHAbility GSaves[GClassCount][2] =
    {
        { EAHAbility::Forca,        EAHAbility::Constituicao },  // Guerreiro
        { EAHAbility::Forca,        EAHAbility::Constituicao },  // Barbaro
        { EAHAbility::Sabedoria,    EAHAbility::Carisma      },  // Clerigo
        { EAHAbility::Inteligencia, EAHAbility::Sabedoria    },  // Mago
        { EAHAbility::Constituicao, EAHAbility::Carisma      },  // Feiticeiro
        { EAHAbility::Destreza,     EAHAbility::Inteligencia },  // Ladino
        { EAHAbility::Sabedoria,    EAHAbility::Carisma      },  // Paladino
        { EAHAbility::Forca,        EAHAbility::Destreza     },  // Patrulheiro
    };
    static_assert(sizeof(GSaves) / sizeof(GSaves[0]) == GClassCount,
                  "duas salvaguardas por classe");

    /**
     * A legal 27-point spread per class.
     *
     * Every one of these was checked by Scripts/preview/ficha.cpp against the
     * price list rather than counted by hand -- the first draft had two of the
     * eight over budget, and an over-budget default is a creation screen that
     * opens illegal.
     */
    const int32 GRecommended[GClassCount][GAbilityCount] =
    {
        { 15, 13, 14, 8,  10, 12 },   // Guerreiro
        { 15, 13, 14, 8,  12, 10 },   // Barbaro
        { 12, 10, 14, 8,  15, 13 },   // Clerigo
        { 8,  14, 14, 15, 10, 10 },   // Mago
        { 8,  14, 14, 10, 10, 15 },   // Feiticeiro
        { 10, 15, 14, 13, 12, 8  },   // Ladino
        { 15, 8,  14, 10, 12, 13 },   // Paladino
        { 10, 15, 14, 8,  13, 12 },   // Patrulheiro
    };
    static_assert(sizeof(GRecommended) / sizeof(GRecommended[0]) == GClassCount,
                  "uma distribuicao sugerida por classe");

    int32 Safe(int32 Value, int32 Limit) { return (Value < 0 || Value >= Limit) ? 0 : Value; }
}

const TCHAR* AHSheet::Short(EAHAbility Which)
{
    return GShort[Safe(static_cast<int32>(Which), GAbilityCount)];
}
const TCHAR* AHSheet::Long(EAHAbility Which)
{
    return GLong[Safe(static_cast<int32>(Which), GAbilityCount)];
}
const TCHAR* AHSheet::Explains(EAHAbility Which)
{
    return GExplains[Safe(static_cast<int32>(Which), GAbilityCount)];
}
const int32* AHSheet::AncestryBonus(EAHAncestry Which)
{
    return GBlood[Safe(static_cast<int32>(Which), GBloodCount)];
}
int32 AHSheet::HitDie(EAHHeroClass Which)
{
    return GHitDie[Safe(static_cast<int32>(Which), GClassCount)];
}
EAHAbility AHSheet::Primary(EAHHeroClass Which)
{
    return GPrimary[Safe(static_cast<int32>(Which), GClassCount)];
}
EAHAbility AHSheet::Casting(EAHHeroClass Which)
{
    return GCasting[Safe(static_cast<int32>(Which), GClassCount)];
}
bool AHSheet::SavesWith(EAHHeroClass Which, EAHAbility Ability)
{
    const int32 Row = Safe(static_cast<int32>(Which), GClassCount);
    return GSaves[Row][0] == Ability || GSaves[Row][1] == Ability;
}
const int32* AHSheet::Recommended(EAHHeroClass Which)
{
    return GRecommended[Safe(static_cast<int32>(Which), GClassCount)];
}

// ── The derived sheet ────────────────────────────────────────────────────────

AHSheet::FAHDerived AHSheet::Derive(EAHHeroClass Class, EAHAncestry Blood, int32 Level,
                                    const FAHAbilities& Scores, const FString* Equipped)
{
    const FAHClassSheet&    Sheet  = AHRules::Class(Class);
    const FAHAncestrySheet& Family = AHRules::Ancestry(Blood);
    const FAHFightingStyle& Style  = AHRules::Style(Class);
    const int32 Prof = Proficiency(Level);
    const bool  bStyled = Style.Level > 0 && Level >= Style.Level;

    FAHDerived Out;

    auto Worn = [Equipped](EAHSlot Slot) -> const FAHItemData&
    {
        if (!Equipped) return AHItems::Nothing();
        const FString& Id = Equipped[static_cast<int32>(Slot)];
        const FAHItemData* Found = AHItems::Find(Id);
        return Found ? *Found : AHItems::Nothing();
    };

    const FAHItemData& Hand   = Worn(EAHSlot::MaoPrincipal);
    const FAHItemData& Off    = Worn(EAHSlot::MaoSecundaria);
    const FAHItemData& Armour = Worn(EAHSlot::Armadura);
    const FAHItemData& Head   = Worn(EAHSlot::Elmo);
    const FAHItemData& Feet   = Worn(EAHSlot::Botas);
    const FAHItemData& Ring   = Worn(EAHSlot::Anel);

    /**
     * The ancestry's bonus is added HERE, not stored on the character.
     *
     * The character keeps what the player BOUGHT -- eight to fifteen, the
     * numbers the point-buy screen is about. If the bonus were folded into
     * the stored scores, changing ancestry halfway through creation would
     * either double it or need somebody to remember to take the old one off,
     * and "remember to take the old one off" is how a +2 becomes a +6 over
     * three clicks. Derived every time, from the two things that are true.
     */
    FAHAbilities Total = Scores;
    Total.Add(AncestryBonus(Blood));

    const int32 Str = Total.Mod(EAHAbility::Forca);
    const int32 Dex = Total.Mod(EAHAbility::Destreza);
    const int32 Con = Total.Mod(EAHAbility::Constituicao);

    // ── Hit points ───────────────────────────────────────────────────────
    /**
     * The die in full at first level, then its average rounded up, and the
     * Constitution modifier EVERY level -- which is the rule people forget
     * and the reason a +2 Constitution is worth more than it looks.
     *
     * Floored at one per level, because a wizard with Constitution 8 at
     * fourth level would otherwise crawl towards zero.
     */
    const int32 Die = HitDie(Class);
    Out.MaxHealth = Die + Con;
    for (int32 Up = 2; Up <= FMath::Max(1, Level); ++Up)
        Out.MaxHealth += FMath::Max(1, Die / 2 + 1 + Con);
    Out.MaxHealth += Family.HealthPerLevel * FMath::Max(1, Level);
    if(Class==EAHHeroClass::Sorcerer) Out.MaxHealth+=FMath::Max(1,Level);
    Out.MaxHealth = FMath::Max(1, Out.MaxHealth);

    // ── Armour class ─────────────────────────────────────────────────────
    /**
     * Base plus as much Dexterity as the armour will let through.
     *
     * TetoDestreza is the whole of the armour design: leather takes all of it
     * and so rewards a Dexterity build, a breastplate takes two, and plate
     * takes none at all and simply hands you eighteen. A player who bought
     * Dexterity 15 and then puts on chain mail has wasted nine points, and
     * the sheet should let him see that before the first fight rather than
     * after it.
     */
    if (Armour.Base > 0)
    {
        const int32 Allowed = Armour.TetoDestreza==0 ? 0 : (Armour.TetoDestreza < 0) ? Dex
                            : FMath::Min(Dex, Armour.TetoDestreza);
        Out.ArmorClass = Armour.Base + Allowed;
        // Heavy armour you are not strong enough for: three metres slower, as
        // the SRD has it. Not forbidden -- just paid for.
        if (Armour.ForcaMinima > 0 && Total.Raw(EAHAbility::Forca) < Armour.ForcaMinima)
            Out.bOverloaded = true;
    }
    else if (Equipped)
    {
        Out.ArmorClass = 10 + Dex;                 // unarmoured, but kitted
    }
    else
    {
        Out.ArmorClass = Sheet.ArmorClass;         // a foe: the old constant
    }
    Out.ArmorClass += Off.Bonus + Head.Bonus + Ring.Bonus + Family.ArmorBonus;
    // Clothing is not armour: unarmoured class features still apply.
    if(Armour.Base<=10 && Class==EAHHeroClass::Barbarian) Out.ArmorClass+=Con;
    if(Armour.Base<=10 && Class==EAHHeroClass::Sorcerer) Out.ArmorClass+=3;
    if (bStyled && Armour.Base > 0) Out.ArmorClass += Style.ArmorBonus;

    // ── The weapon in hand ───────────────────────────────────────────────
    const bool bArmed = Hand.Dano > 0;
    const bool bBow   = bArmed && Hand.Alcance > 0;
    /**
     * Which ability swings it.
     *
     * A bow is always Dexterity; a finesse weapon is whichever of the two is
     * better, which is the rule as written and also the only version a player
     * will not feel cheated by. Everything else is Strength.
     */
    Out.AttackWith = bBow ? EAHAbility::Destreza
                   : (bArmed && Hand.bSutil && Dex > Str) ? EAHAbility::Destreza
                   : bArmed ? EAHAbility::Forca
                   : Primary(Class);
    const int32 Swing = Total.Mod(Out.AttackWith);

    if (bArmed)
    {
        Out.DamageSides    = Hand.Dano;
        Out.DamageDice     = Hand.DiceCount;
        Out.AttackBonus    = Prof + Swing;
        Out.DamageModifier = Swing + Ring.DanoBonus + Hand.DanoBonus + Family.DamageBonus;
        Out.Art            = Hand.Arte;
        if (bBow)
        {
            Out.RangedRange = Hand.Alcance;
            Out.RangedSides = Hand.Dano;
        }
        /**
         * Archery and Duelling are NOT added here, and that is deliberate.
         *
         * `AAHCharacter::RangedAttackBonus()` and `MeleeDamageBonus()` already
         * add them on top of AttackBonus and DamageModifier at the moment of
         * the roll -- which is the right place, because Duelling depends on
         * what is in the other hand at that instant. Adding them here as well
         * would give a ranger +4 to hit with a bow instead of +2, and nothing
         * in a play session would ever show which of the two was wrong.
         *
         * Defense IS added, just below the armour, because that one lands in
         * ArmorClass and this function is the only thing that writes it.
         */
    }
    else if(!Equipped)
    {
        // No kit at all: the class's own numbers, exactly as before.
        Out.DamageSides    = Sheet.DamageSides;
        Out.AttackBonus    = Sheet.AttackBonus;
        Out.DamageModifier = Sheet.DamageModifier;
        Out.Art            = Sheet.Weapon;
        Out.RangedRange    = Sheet.RangedRange;
        Out.RangedSides    = Sheet.RangedSides;
    }
    else
    {
        Out.AttackBonus=Prof+Str;
        Out.DamageSides=1;
        Out.DamageModifier=Str;
        Out.Art=EAHWeaponKind::Sword;
    }

    // ── The rest of the sheet ────────────────────────────────────────────
    Out.InitiativeBonus = Dex + Family.InitiativeBonus;
    Out.Movement        = Family.Movement + static_cast<float>(Feet.Movimento)
                        - (Out.bOverloaded ? 300.f : 0.f);

    const int32 CastMod = Total.Mod(Casting(Class));
    Out.SpellAttack = Prof + CastMod;
    Out.SpellDC     = 8 + Prof + CastMod;
    Out.ConSave     = Con + (SavesWith(Class, EAHAbility::Constituicao) ? Prof : 0);

    return Out;
}
