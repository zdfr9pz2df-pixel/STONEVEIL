# STONEVEIL Creator Alpha ship readiness

Specification: Deep Work Engineering Brief supplied 2026-09-18.
Authority: integration/creator-alpha. Status is evidence-based; compilation alone
does not satisfy a gate.

| Gate | Status | Acceptance required | Evidence / remaining gap |
| --- | --- | --- | --- |
| 1 Repository consolidation | VERIFIED | Preserve local and PR work; one integration branch; Release, CTest, Windows CI and compatibility checks pass | 8f24aca: local Release/CTest 4/4, Windows CI run 35409657662 success, editor/game captures, package validation; PR #2 includes PR #1 parent and preserved working editor. |
| 2 Editor shell | IN PROGRESS | New/Open/Save Project and Level; geometry/object editing, selection, movement/facing, inspector, Play, Undo/Redo | Project/document operations plus contextual object, room and trigger selection/movement/inspection exist. Rich behavior inspectors and authored non-spawn facing remain. |
| 3 Character Creator + Roster | IN PROGRESS | Author character, recruit once, active/reserve management at generic point, max 3, permanent death and save/load consistency | Project-authored v1 character catalog drives roster/combat/HUD/save/export. Stable-ID recruit objects add Reserve members exactly once; generic management points provide scrolling active/reserve swaps with max 3, and save/load preservation is tested. Downed and runtime equipment/ability catalogs remain. |
| 4 Core authoring objects | IN PROGRESS | All twelve object families with IDs, location, properties, common triggers/actions and validation | Story/door/recruit/management/arrival/transition authoring uses common EventRuntime. Richer contextual behaviors still remain. |
| 5 Self-hosted test game | IN PROGRESS | Editor-created two-level game proves speech/combat/items/doors/recruitment/management/death/saves/transition without engine content changes | Gatehouse and Underkeep now prove the complete loop locally; Windows CI/artifact confirmation is the remaining acceptance evidence. |
| 6 Project/save/export | IN PROGRESS | Versioned project and save data, migrations, references and standalone Windows export from editor | Save v6 stores current/inactive level state plus campaign-wide roster, death, inventory, XP and event state. Legacy readers, project registry, atomic writes and editor Windows export remain tested. Multi-file recovery remains. |
| 7 Stable combat contract | IN PROGRESS | Document and prove 1–3 control, abilities, cooldowns, ranged/projectiles, interception, movement, downed/death/failure and save behavior | Melee/formation/action gates exist; remaining mechanics and contract incomplete. |
| 8 Validation/safety | IN PROGRESS | Creator-readable blocking errors and warnings catch invalid references and progression traps before export | Project validation now catches missing transition destinations, missing/blocked arrivals, unreachable levels and reachable dead ends. Rich behavior-reference validation remains. |

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

## Project/export slice verification

- Windows MSVC Release and CTest 6/6 pass, including ProjectTests.
- New/Open/Save Project, registered-level browser, native level Open/Save As,
  automatic registration and starting-level selection are implemented.
- Windows export copies runtime, registry, levels and referenced assets; marker
  is written last. A retained real export launched and captured successfully.
- Project panel visually inspected in build/project-shell-final-smoke.
- dumpbin confirms Release depends only on Windows system DLLs.
- Gate 2 remains IN PROGRESS: contextual selection/movement/facing/inspector
  still missing. Gate 6 remains IN PROGRESS: no complete campaign progression,
  multi-file transaction, or installed/user-profile save workflow yet.
- Gate 8 now distinguishes project blocking errors and missing-asset warnings;
  progression-graph validation remains absent.

## Known open defects

- Object inspector slice: local Release and CTest 7/7 pass; selection/movement,
  occupied/boundary rejection, door-marker movement, stable/legacy event
  following, protected deletion and undo/redo are tested. Spawn inspector
  capture inspected at build/object-inspector-smoke/object-inspector.png.
  Gate 2 remains IN PROGRESS: room/trigger spatial transforms now exist; rich
  behavior inspection and authored non-spawn facing are not implemented.
- Character-catalog slice: local Release and CTest 8/8 pass, including catalog
  validation/roundtrip and project export/reopen. Character Creator capture is
  under build/character-creator-smoke.
- Recruitment slice: local MSVC Release and CTest 9/9 pass. Version 7 level
  migration, stable recruit references, duplicate/non-healing recruit behavior,
  generic management dispatch, project reference validation, scrolling roster,
  and authored-character save/load are covered. UI captures include the Object
  layer and Party Management screen. Downed and equipment/ability runtime
  behavior remain outside this checkpoint.

- Destination transitions and persistent multi-level campaign state are implemented.
- Richer Story Tile/door compiler fields still need format/editor/runtime
  adapters. Current authored events are messages and existing door behaviors.
- Saves do not preserve full active-combat cooldown/RNG state. Legacy saves
  cannot recover event history or stable entity identity they never stored.
- Individual writes now use flushed atomic replacement on Windows. Multi-file
  recovery and project-wide reference migrations remain Gate 6 work.
- Long story messages need wrapped/paged presentation. Object art is placeholder;
  portrait reaction logic is preserved but not shown in the HUD.
- Save As uses a native filename dialog and generates a distinct safe level ID;
  saving inside the project registers automatically. A no-op text-edit session
  can conservatively mark the draft dirty.
- See CREATOR_ALPHA_AUDIT.md for original findings and SAVE_MIGRATIONS.md for
  current persistence limitations. Only Gate 1 is verified; no full Creator Alpha
  claim is made.

## Next highest-leverage task

Run the connected campaign as a human playtest from Gatehouse through recruitment,
party management, return travel, save/reload, then close remaining usability defects
found in that session. Do not treat the current creator fields as a complete
equipment or ability system.
