# Playable milestones

## Current priority — Unreal combat first

The user's current direction is to polish the turn-based encounter in Unreal 5.8.2 before expanding maps. The older phases below describe the preserved Godot prototype and the broader backlog; they do not override this priority.

The courtyard is the combat test space. Progress is judged by readable attack preparation/contact/recovery, reliable targeting and resource costs, distinct hit/miss/critical/healing feedback, and meaningful offensive/defensive decisions. Passing rules tests alone does not establish that combat is engaging; repeatable play sessions must also evaluate pacing and clarity.

Before expanding environments, complete these combat checks:

- Verify mouse and keyboard actions, turn transitions, obstruction and exhausted movement in the running game.
- Author attack contact notifies and pair weapon animations with matching equipment; verify contact and reactions frame by frame.
- Add appropriate combat poses, transitions, audio and effects using original or licensed assets.
- Improve tactical choices and enemy responses within the existing encounter, with clear cost and outcome previews.
- Evaluate repeated play and frame times on the target notebook. Art, animation and performance need explicit validation before claiming AAA quality.

Each phase preserves the previous playable loop and runs the regression suite before being considered complete. Make a version-control checkpoint before major changes. Add one tested system at a time.

| Phase | Deliverable | Exit condition |
|---|---|---|
| 1 — Combat prototype | Current Warden courtyard | Move, fight, cast all four abilities, die/restart, collect one item |
| 2 — Character creation | Data-backed ancestry/class/standard array/skills/spell choices | A valid created character enters the same courtyard and survives a save/load cycle; add classes incrementally |
| 3 — RPG rules | Typed stats, damage affinities, condition rules, concentration, spell slots, rest pools and cadence | Deterministic rules tests plus class-specific encounter balance |
| 4 — First environment | Handcrafted forest route, settlement, river/bridge, watchtower and cave entrance | Navigation meshes, occlusion handling and exploration remain responsive |
| 5 — Enemies | Melee, ranged and caster behavior components with perception/leashing | Distinct encounters with no pathing deadlocks or unavoidable attacks |
| 6 — Spells/abilities | Expand targeting, drag-to-slot loadouts and class-specific effects | Each supported targeting/effect combination has clear previews and resource rules |
| 7 — Inventory/loot | Grid/list inventory, equip/compare/stack/sort and loot tables | Equipment changes combat/model state; all items survive save/load |
| 8 — Dialogue/quests | Branching conversation, contextual checks, quest states and rest | A short objective supports alternative choices and resumes correctly from saves |
| 9 — Vertical slice | Complete introductory forest adventure and dungeon boss | First-time players finish a coherent 30–45 minute quest without developer assistance |
| 10 — Polish/performance | Authored animation/audio/art, accessibility, rebinding, LOD/batching | Profiled 1080p/60 target on the selected PC, readable combat and regression-free saves |

## First full vertical-slice milestone: A Fire in the Roots

The Warden reaches the settlement at Broken Watch. Missing travelers left behind warm ash instead of footprints. A frightened keeper gives a short lead. The player follows abandoned packs through the forest, examines an altered shrine, and enters the root-choked crypt beneath the watchtower.

The eventual boss, the **Root Lantern**, anchors itself to three old beacon stones. The first phase teaches a directional sweep; the second lights dangerous ground and calls smaller creatures; the final phase exposes a short interrupt window when it drains a beacon. Movement, resource management and environmental choices solve the fight. This encounter is planned, not present in Phase 1.

The slice must introduce movement, melee, a spell, telegraph avoidance, one branching conversation, a skill check, loot/equipment, rest, dungeon traversal and the boss. Ship that complete loop before growing the world or filling out hundreds of spells.
