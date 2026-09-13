# Architecture and technical direction

**Historical Phase 1 design:** see [FIFTH_EDITION.md](FIFTH_EDITION.md) for the implemented character creator, level-one content, visible d20, action economy, rest resources and character-choice persistence added afterward. The original prototype path remains available for regression tests.

## Stack decision

Godot 4.6.1 Standard, typed GDScript, JSON definitions, built-in 3D physics and a Compatibility renderer. This keeps the first project small, portable and directly editable by a beginner. Pinning the engine avoids changing behavior mid-milestone. Compatibility is sufficient for the placeholder slice; evaluate Forward+ when actual volumetric lighting and higher-fidelity materials enter production.

## Folder structure and system ownership

```text
project.godot              Engine configuration and main scene
scenes/main.tscn           Minimal composition-root scene
scripts/actors/
  combatant.gd            Shared health, conditions, movement, procedural model
  player.gd               Player pursuit, basic attacks, tonic, step
  enemy.gd                Idle/chase/windup/return/death melee state machine
scripts/combat/
  rules.gd                Pure deterministic-with-seed dice calculations
  abilities.gd            Cooldown/resource validation and effect dispatch
  projectile.gd           Swept ray collision, bounded projectile lifetime
scripts/world/
  game.gd                 Composition root, input routing, attack reporting, pickup
  navigation.gd           AStarGrid2D projected onto XZ
  courtyard.gd            Repeatable courtyard geometry and obstacle footprints
  geometry.gd             Shared metre-scale primitives/material helpers
  camera.gd               Perspective framing, cursor ray, ground projection
  effects.gd              Transient VFX, floating text, spatial sound
scripts/ui/hud.gd         Scaled HUD, ability metadata, log, guide, pause menu
data/                     Character, class, weapon/item and ability definitions
tests/test_runner.gd      Rules and integrated scene regression checks
docs/                     Architecture and milestone planning
Play.cmd / Edit.cmd       Local beginner launchers
```

No global autoload managers. The composition root creates and supplies dependencies. Shared actors emit `hurt` and `died`; enemy death triggers the pickup. The prototype actor holds a reference to the root for access to presentation and navigation. This is intentional small-slice coupling, not a final 100-spell service locator. Before adding another playable class, replace that root access with narrow combat/navigation interfaces and move the procedural model behind an actor-view component.

## Communication flow

1. Input becomes a movement, target, aim, cast, or interaction command.
2. Navigation produces waypoints. The actor accelerates toward them through CharacterBody3D physics.
3. Player pursuit checks weapon range. Enemy AI checks detection range, telegraphs a fixed impact location, then resolves after windup.
4. AbilityController validates health, conditions, focus and cooldown before dispatching an effect handler selected from data.
5. A projectile checks swept collision; an area effect finds actors within its radius. CombatRules resolves the resulting attack roll or saving throw.
6. The receiving actor consumes temporary HP, subtracts HP and emits lifecycle signals.
7. Presentation shows damage, impact, sound and log entries. HUD reads current state; UI does not own the rules.

Trees and ruins register the same conceptual footprints in navigation. Waypoints avoid the footprint expanded for the actor radius. Stone walls also have physics colliders. The 37×37 grid and fixed-height ray projection are explicitly replaceable, not suitable for the full multilevel region.

## Real-time tabletop adaptation

The d20 layer has no time or scene dependencies. It accepts an RNG and numbers, returning an explicit outcome. A normal melee attack uses d20 + STR modifier + proficiency against AC, then 1d8 + STR on a hit. A natural 20 doubles dice only; a natural 1 misses. Advantage/disadvantage helpers are present and tested. E/R use saving throws, half damage on success, and conditions only on failure. Only the conditions used by Phase 1 are implemented.

The prototype prioritizes a 0.78-second basic attack rhythm, independent ability cooldowns, and regenerating focus. **It does not impose a six-second shared action lock**, which would make this first control test sluggish. For Phase 3, formalize separate action, bonus-action and reaction channels with a six-second resource-equivalent budget, then balance damage and resource use against that budget. Class identity should come from available effects and recovery/resource patterns, not arbitrary copies of the same QWER loadout.

Attack delivery and damage resolution are separate: a projectile can physically hit and still fail its attack roll; MISS explains the distinction. Enemy cleaves require the player to still occupy the marked area, then roll against AC. Dodging works by leaving that area.

## Data contracts

JSON dictionaries are sufficient for the initial slice. Convert them into validated typed Resource definitions when the editor-facing content library grows. Do not add unchecked fields to UI branches.

**Character:** id, name, ancestry id, class id, level, XP, six attributes, HP/focus, AC, speed, equipped weapon id, ordered ability ids. Phase 2 extends the record with appearance, ancestry features, chosen skill proficiencies, learned spells and prepared spells.

**Class:** id, name, hit die, primary attributes, armor/weapon/save proficiencies, starting equipment ids, ability ids, spellcasting progression, named resource definitions and a level-indexed progression map. Only Fighter has a record in Phase 1; no empty pretend implementations of the other eleven classes are exposed.

**Ability/spell:** id, name, description, icon, level, school, castType, targetingType, range, radius, projectileSpeed, cooldown, manaCost, spellSlotCost, damageDice/type, savingThrow, statusEffects, duration, concentration, visualEffect, audioEffect and handler. The UI iterates slot ids and reads labels/costs/cooldowns. The handler registry owns behavior. Current handlers: projectile, barrier, ground and burst. Adding 100 definitions that use those handlers needs no UI branches; new targeting/effect mechanics still need their own tested handler.

Future targeting vocabulary: SELF, TARGET_ENEMY, TARGET_ALLY, GROUND_POINT, CONE, LINE, CIRCLE, PROJECTILE, CHAIN, AURA. Only SELF, GROUND_POINT, CIRCLE and PROJECTILE execute today. Spell slots, concentration and level are schema reservations, not functioning rules.

**Weapon/item:** stable id, category, rarity, slot, attack range/speed/animation, damage dice/type/bonus, attribute scaling and special properties. The one loot item has a description and stack-size field; a full inventory stack model waits for Phase 7.

**Enemy:** stable id, HP/AC, movement and detection/leash ranges, attack bonus/dice/range/cooldown, windup, attributes, behavior id and loot id. Phase 5 will bind behavior ids through an AI factory; the first slice instantiates the single melee type directly.

## Future persistence contract

Only audio preference persistence is implemented now. Add a versioned save service before Phase 2 character creation ships: root `schemaVersion`, character choices/stats/progression, abilities/spells by stable id, equipment/inventory, position and region id, quest flags, NPC states and world entity states. Migrate one version at a time, preserve a backup, write to a temporary file, and atomically replace the previous valid save. Tests must cover migration and corrupted-file fallback. Never serialize live Node references.

## Asset and performance boundaries

One world unit equals one metre. Humanoid height is approximately 1.9 m, feet at local Y=0, forward is -Z, actor radius 0.42 m. Replace the visual subtree without moving the origin or changing gameplay dimensions. Current animation blends speed-driven leg swing, facing, attack pose and death tilt. A later rigged actor view will consume the same state through AnimationTree.

Effects and audio nodes free themselves on completion; projectiles have finite travel distance. There is only one enemy, so pooling is unnecessary at present. Before encounter count grows, introduce pooled VFX/projectiles, shared materials, MultiMesh vegetation, distance LOD, and performance captures on the actual target PC. Navigation searches occur on commands or at bounded pursuit intervals, not every render frame.

## Primary references

- [Godot 4.6.1 engine release](https://github.com/godotengine/godot-builds/releases/tag/4.6.1-stable)
- [Godot navigation agents](https://docs.godotengine.org/en/stable/tutorials/navigation/navigation_using_navigationagents.html)

All game names, geometry, UI, descriptions and synthesized sounds in this prototype were created for this project. There are no copied commercial assets.
