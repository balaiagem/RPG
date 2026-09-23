# Classes, spells and progression (levels 1-4)

## Play

Choose ancestry and class. Full casters then choose a repertoire before initiative starts. Click a spell row to prepare/remove it, then **PRONTO**. During combat, **K / MAGIAS** opens the repertoire; select a prepared spell and use **E / CONJURAR**. Offensive spells use the enemy under the cursor, otherwise the nearest visible enemy in range. The **CIRCULO** button selects first- or second-circle slots. Cantrips spend an action but no slot.

Victories award 300 XP. Level thresholds are 300, 900 and 2700 XP. Choose a permanent feat at level 4, then **SEGUIR** rests and starts another encounter. This preserves the current character and XP in this run; **F5** or closing the game starts a new run.

Opportunity attacks ask **Y / ATACAR** or **N / PASSAR** and pause the encounter. Declining preserves the reaction. Accepting spends it once, even on a miss.

## Eight playable classes

| Class | First milestone features |
|---|---|
| Fighter / Guerreiro | Second Wind; Action Surge at level 2 |
| Barbarian / Barbaro | Rage; Reckless Attack at level 2 |
| Cleric / Clerigo | Divine spell preparation, healing, buffs and attack spells |
| Wizard / Mago | Arcane repertoire, cantrips and second-circle spells at level 3 |
| Sorcerer / Feiticeiro | Limited repertoire; sorcery points at level 2; CONVERTER trades 2 points and a bonus action for a first-circle slot; POTENCIA at level 3 arms an empowered damage spell |
| Rogue / Ladino | Sneak Attack once per turn, 1d6 then 2d6 at level 3; ESCAPAR and CORRER spend the bonus action from level 2; MIRA at level 3 grants advantage on the next attack and prevents movement that turn |
| Paladin / Paladino | CURAR spends the Lay on Hands pool (5 per level); first-circle spells from level 2; PUNIR toggles Divine Smite, consuming one slot only on a successful melee hit |
| Ranger / Patrulheiro | Longbow attack; first-circle spells from level 2; Hunter's Mark adds 1d6 to weapon hits against the marked foe; Goodberry creates ten consumable healing berries |

Full-caster slots I/II: level 1 = 2/0, level 2 = 3/0, level 3 = 4/2, level 4 = 4/3. Paladin and Ranger: 0/0, 2/0, 3/0, 3/0. Rest restores resources and removes encounter buffs.

## Spell catalog

- Cleric: Sacred Flame, Cure Wounds, Healing Word, Guiding Bolt, Shield of Faith, Aid, Inflict Wounds, Bless.
- Wizard and Sorcerer: Fire Bolt, Ray of Frost, Magic Missile, False Life, Mage Armor, Scorching Ray, Burning Hands, Thunderwave.
- Paladin (available by level 4): Cure Wounds, Bless, Shield of Faith.
- Ranger: Cure Wounds, Hunter's Mark, Goodberry.

Damage spells resolve at the animation impact rather than at button press. Spell attacks use a real d20; saving throws are labeled as the target's save; Magic Missile does not invent an attack roll. Upcasting consumes the selected slot rank. Shield of Faith, Bless and Hunter's Mark require concentration and replace each other. Damage can break concentration. Burning Hands and Thunderwave affect a frontal area, including other creatures caught in it; Thunderwave's push sweeps against collision.

## Current boundaries

This is a combat prototype using a curated subset of 2014 fifth-edition mechanics, not the complete tabletop classes. Subclasses, the complete spell lists, spellbook research, material components, and party-targeted healing are not implemented. Healing and support spells currently target the caster. Repertoire changes at camp are allowed for all casters, including Sorcerer and Ranger. Empowered Spell automatically rerolls up to three low damage dice after being armed; it costs one sorcery point at casting. Only conversion from points to a first-circle slot is currently exposed.

Class attributes are fixed archetype sheets. Feats and ancestry traits remain the prototype's defined bonuses. Models, some weapons and spell visuals are shared; adding classes does not provide new authored character/weapon assets. Persistent saves across launches are not implemented.

Rules references: [2014 Basic Rules spells](https://www.dndbeyond.com/sources/dnd/basic-rules-2014/spells), [2014 Basic Rules classes](https://www.dndbeyond.com/sources/dnd/basic-rules-2014/classes). Spell descriptions in the UI are concise implementation summaries.
