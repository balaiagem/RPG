# Phase 1 verification — 2026-09-09

- Engine: Godot 4.6.1 Standard, official Windows x86_64 build.
- Editor import: passed; all game scripts registered without parse errors.
- Headless integrated suite: **26/26 checks passed**, exit code 0.
- Graphics run: OpenGL Compatibility on NVIDIA GeForce RTX 3050 6GB Laptop GPU.
- Inspected the rendered courtyard and HUD, plus a live combat window showing movement into the arena, damage, barrier and ability cooldown state.
- Final lighting preview: `artifacts/preview.png` (generated, excluded from git).

Checks include attribute/proficiency math, natural 1/20, advantage/disadvantage, obstacle routes, blocked-destination fallback, movement, four-ability loading, shield absorption, resource/cooldown rejection, ground damage, projectile collision, burst, pursue-and-attack, melee telegraph, leaving the impact zone, paused cooldowns, enemy/player death, pickup, duplicate-pickup prevention and scene restart.

The first movement assertion pointed at an obstacle-expanded cell rather than its reachable destination. The test was corrected to target open ground; a separate blocked-target check verifies fallback behavior explicitly. A same-frame repeat-pickup check identified the need to clear the loot reference immediately after scheduling deletion; this is fixed and tested.

These checks do not certify 1080p/60 FPS, camera visibility at every angle, long-duration stability, or unimplemented later-phase systems. Full hardware profiling and authored animation/asset review remain later work.
