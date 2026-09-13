# Character creation and visible dice

## Start

Relaunch **Play.cmd**. The game opens with character creation. Choose origin, class, attributes, skills and two combat spells where applicable, then review and enter the courtyard. A local character-choice save can be reused from the first page on your next launch.

**F5** restarts the encounter with your current build. **F6** returns to a fresh creator. Character choices are saved; encounter progress is not yet persisted.

## Combat changes

- Combat remains real time, with free movement. Actions, bonus actions and reactions have separate six-second recovery channels. Basic attacks and action spells compete for the same action.
- The large, animated faceted d20 reports the actual natural roll, modifier, total, target and success/failure. Presentation uses its own animation clock and cannot alter the rules RNG. It displays attacks, enemy saves, concentration checks, skills and death saves. Rolls queue briefly; overflow remains in the dice history/combat log.
- Level-one HP comes from class hit die + Constitution; armor comes from equipment or class defense formulas. Attack modifiers and spell DCs derive from attributes and proficiency.
- Cantrips are at will. Leveled spells spend first-level slots. A leveled bonus spell restricts leveled action spells for the six-second round.
- Concentration occupies one channel, checks Constitution when damaged, expires by duration, and removes its linked effects when broken.
- **Space** takes the Dodge action. **1** drinks a 2d4+2 healing potion using an action. **C** makes an Arcana check to demonstrate skill proficiency and the d20 presentation.
- **5** takes a short rest, **6** a long rest, only while the enemy is unalert or defeated. Resting advances fictional time immediately in this prototype. Short rests spend the one available Hit Die if injured and recover eligible features; long rests restore HP, slots and feature pools.
- A knockout starts death saves every six seconds. A natural 20 returns the hero to 1 HP; three successes stabilize; three failures end the encounter. Massive damage can kill immediately. F5 always starts another attempt.

Targeted QWER abilities use hold-to-aim and release-to-cast. Self abilities activate on press. Hover each slot for its actual cost and targeting. Right click selects an enemy or moves, as before.

## Class implementation at level one

| Class | Implemented identity |
|---|---|
| Barbarian | Rage twice per long rest, +2 Strength melee damage, physical resistance, Strength-check advantage; Constitution-based Unarmored Defense |
| Bard | Charisma casting, Vicious Mockery and healing choices; Bardic Inspiration gives a nearby allied wayfarer a d6 for an attack, with Charisma-based uses |
| Cleric | Fixed Life domain, heavy armor/shield, Wisdom casting, Disciple of Life healing |
| Druid | Wisdom casting, nature skill options, flame/healing/Entangle; Druidic recorded as a language feature |
| Fighter | Fixed Defense fighting style, armored AC bonus, Second Wind 1d10+1 once per short rest |
| Monk | Dexterity/Wisdom Unarmored Defense, 1d4 Dexterity unarmed attacks, bonus Martial Arts strike after Attack |
| Paladin | Heavy armor/shield, five-point Lay on Hands pool, limited Divine Sense |
| Ranger | Longbow, fixed plant favored enemy and forest terrain; Survival advantage and applicable forest expertise |
| Rogue | Sneak Attack 1d6 with advantage once per round, expertise in first two selected skills, Hide action |
| Sorcerer | Fixed draconic origin, 13+Dexterity AC and +1 HP; Charisma casting |
| Warlock | Fixed Fiend patron, one short-rest Pact Magic slot, Dark One's Blessing; Eldritch Blast, Agathys or Rebuke choices |
| Wizard | Intelligence casting, a one-slot Arcane Recovery on short rest once per long rest |

Ancestry options: Human, High Elf, Hill Dwarf, Lightfoot Halfling, Half-Orc, Tiefling and Dragonborn. Attribute bonuses and visual scale apply. Implemented traits include elf Perception, dwarf toughness/poison resistance, halfling Lucky, half-orc endurance/extra melee critical die, fire resistance and a Dragonborn fire cone. These are **selected traits**, not complete ancestry implementations.

## Scope and deliberate adaptations

This implements a curated level-one combat library, **not the entire Player's Handbook**. Spellcasters currently equip one supported cantrip and one supported first-level spell rather than maintaining their complete learned/prepared spell counts. No levels 2–20, multiclassing, background selection, full subclasses, feat selection, complete racial traits, languages/dialogue mechanics, ritual casting or complete spell lists yet.

Healing spells currently target the player. The Bard's allied wayfarer exists to make Inspiration playable; it follows and attacks, but is not a full party AI or companion-management system. The enemy still focuses the hero. Divine Sense correctly reports the Thornbound as a plant. Ranger tracking and Arcana are demonstrative skill checks; a full investigation/quest framework is still pending.

Distances use metres with slightly generous melee reach for collision geometry. Movement remains faster than tabletop feet per round to preserve the established control feel. Ranged spell attacks resolve at cast time with impact effects rather than waiting for a projectile. Hide uses a six-metre exposure gate and passive Perception 12, not a complete visibility/cover model. Ray of Frost reduces movement by one third for the round. Entangle uses the initial save and concentration-duration restraint; spending an action to escape is not implemented yet. Heroism supplies repeated temporary HP; frightened conditions and their immunity are not implemented. Chill Touch deals its damage; healing prevention/undead disadvantage await enemy healing and creature-type systems. Bardic Inspiration currently supports allied attacks, not allied checks/saves. Rage ends if no attack or incoming damage sustains it for one round. This is an explicit real-time adaptation, not turn-based initiative combat.

Art remains consistent metre-scale placeholder geometry. Creator and gameplay share appearance/weapon styling. The dark teal/gold UI and die design are original. This is not a claim of finished AAA art, animation or performance.

## Architecture additions

- `data/classes_5e.json`, `ancestries.json`, `abilities_5e.json`: content definitions; original phase-one data is retained for regression testing.
- `CharacterBuild`: validation, standard-array derivation, equipment math and schema-versioned character-choice persistence. Saves write a temporary file and rename; unsupported/corrupt records fall back to creation.
- `CharacterCreator`: native Godot controls for the six-page flow and a 3D preview. UI reads the content catalog.
- `RoundResources`: action channels, slots, feature pools and timed states.
- `HeroRules`: attacks, traits, skills, concentration, rest and death saving throws.
- `FifthEditionAbilities`: generic handler dispatch for the supported effect families; class UI does not branch on spell names.
- `DiceOverlay`: receives resolved rolls. It animates a projected 20-faced icosahedron independently of combat.

Full encounter saves and key rebinding remain future work. The original combat path is available with `-- --legacy` for regression testing. A pre-change snapshot is in `.tools/phase1-checkpoint.zip`; Git author identity was unset, so no commit identity was fabricated or configured.

## Verification

The existing 26-check suite and the new 95-check suite run through Test.cmd. New checks cover all 12 builds, every offered spell/class combination, independent action channels, features, resistance, concentration cleanup, rest, death/revival, save replacement/corruption and native creator validation. `tests/capture_creation.gd` renders all creation pages and a real seeded Arcana roll for visual review. Screenshots are under `artifacts/` and excluded from Git.

## Rules source

Uses the 2014-compatible [SRD 5.1](https://media.dndbeyond.com/compendium-images/srd/5.1/SRD_CC_v5.1.pdf), rather than mixing it with the revised 2024 rules. The supplied handbook link returned a loading error. No commercial game assets, UI layouts, or handbook artwork/text have been copied. See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for SRD attribution.
