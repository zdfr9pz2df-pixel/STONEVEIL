# STONEVEIL Creator Alpha ship readiness

Specification: Deep Work Engineering Brief supplied 2026-09-18.
Authority: integration/creator-alpha. Status is evidence-based; compilation alone
does not satisfy a gate.

| Gate | Status | Acceptance required | Evidence / remaining gap |
| --- | --- | --- | --- |
| 1 Repository consolidation | VERIFIED | Preserve local and PR work; one integration branch; Release, CTest, Windows CI and compatibility checks pass | 8f24aca: local Release/CTest 4/4, Windows CI run 35409657662 success, editor/game captures, package validation; PR #2 includes PR #1 parent and preserved working editor. |
| 2 Editor shell | IN PROGRESS | New/Open/Save Project and Level; geometry/object editing, selection, movement/facing, inspector, Play, Undo/Redo | LevelDocument owns file identity/history, untitled New/import, unique Save As, save-point-aware undo/redo, and failed-load preservation. Close/discard guards and exit-paint undo fixed. Project operations/browser/object movement and inspector remain. |
| 3 Character Creator + Roster | IN PROGRESS | Author character, recruit once, active/reserve management at generic point, max 3, permanent death and save/load consistency | Max 3 enforced; reserve swap, non-healing duplicate recruit and death persistence tests pass. Static catalog only; authored creator/management/downed lifecycle absent. |
| 4 Core authoring objects | IN PROGRESS | All twelve object families with IDs, location, properties, common triggers/actions and validation | Existing story/door authoring uses common EventRuntime. Rich PR compilers retained/tested. Advanced contextual behaviors, transitions, recruitment and management remain. |
| 5 Self-hosted test game | NOT STARTED | Editor-created two-level game proves speech/combat/items/doors/recruitment/management/death/saves/transition without engine content changes | Existing sample levels do not prove this acceptance. |
| 6 Project/save/export | IN PROGRESS | Versioned project and save data, migrations, references and standalone Windows export from editor | Save v5 stores event state and entity IDs; legacy formats tested. CI archive exists; creator project/export workflow absent. |
| 7 Stable combat contract | IN PROGRESS | Document and prove 1–3 control, abilities, cooldowns, ranged/projectiles, interception, movement, downed/death/failure and save behavior | Melee/formation/action gates exist; remaining mechanics and contract incomplete. |
| 8 Validation/safety | IN PROGRESS | Creator-readable blocking errors and warnings catch invalid references and progression traps before export | Compiled-ID collision checks, malformed legacy-map guard and bounded/reentrant event tests added. Project-wide severity/graph validation absent. |

## Verification log

- 2026-09-18 baseline: Windows MSVC Release build succeeds, CTest 2/2.
- PR #1 c538cfd: Windows Build run 34857124685 success. This does not establish
  integration CI success.
- Consolidation 8f24aca: Windows Build run 35409657662 success on PR #2.
  CTest 4/4 and packaging passed. Remote tree ac61e91 is byte-identical to local
  integration checkpoint 74db761. Baseline bd740bb and merge 74db761 remain on
  archive/creator-alpha-local-checkpoint. Existing smoke captures remain on disk.
- Editor-safety slice: local MSVC Release and CTest 5/5 pass, including file
  identity, undo/redo, and failed-save/load preservation tests. Its new CI run is
  linked from PR #2; the gate-1 evidence above refers to the consolidation commit.
- Hidden-window title/editor/story/lights/playtest captures are under
  build/creator-integration-smoke. Gameplay textures load from executable content
  paths. The standalone copied package validates Gatehouse. These smoke checks
  are not proof of the full self-hosted-game or export gates.

## Known open defects

- No project browser/creator, external character catalog, recruitment UI,
  management points, destination transitions, or editor-driven game export yet.
- Richer Story Tile/door compiler fields still need format/editor/runtime
  adapters. Current authored events are messages and existing door behaviors.
- Saves do not preserve full active-combat cooldown/RNG state. Legacy saves
  cannot recover event history or stable entity identity they never stored.
- Save writes are validated but not yet crash-atomic; disk-full/power-loss
  recovery and project-wide reference migrations remain Gate 6 work.
- Long story messages need wrapped/paged presentation. Object art is placeholder;
  portrait reaction logic is preserved but not shown in the HUD.
- Save As uses an automatic unique filename/level ID; campaign registration is
  still manual. A no-op text-edit session can conservatively mark the draft dirty.
- See CREATOR_ALPHA_AUDIT.md for original findings and SAVE_MIGRATIONS.md for
  current persistence limitations. Only Gate 1 is verified; no full Creator Alpha
  claim is made.

## Next highest-leverage task

Finish Gate 2 with a versioned ProjectDocument and New/Open/Save project + level
browser backed by the campaign registry. Add contextual select/move/facing and
inspector operations, then proceed to the authored Character Creator/roster gate.
