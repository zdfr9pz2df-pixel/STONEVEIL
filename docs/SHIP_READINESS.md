# STONEVEIL Creator Alpha ship readiness

Specification: Deep Work Engineering Brief supplied 2026-09-18.
Authority: integration/creator-alpha. Status is evidence-based; compilation alone
does not satisfy a gate.

| Gate | Status | Acceptance required | Evidence / remaining gap |
| --- | --- | --- | --- |
| 1 Repository consolidation | IN PROGRESS | Preserve local and PR work; one integration branch; Release, CTest, Windows CI and compatibility checks pass | Baseline Windows Release and 2/2 CTest pass. PR #1 inspected; its own CI is green. Integration pending. |
| 2 Editor shell | IN PROGRESS | New/Open/Save Project and Level; geometry/object editing, selection, movement/facing, inspector, Play, Undo/Redo | Existing integrated painting and playtest; project operations and document safety incomplete. |
| 3 Character Creator + Roster | IN PROGRESS | Author character, recruit once, active/reserve management at generic point, max 3, permanent death and save/load consistency | Static catalog and roster exist. Authored catalog/creator and full lifecycle absent. |
| 4 Core authoring objects | IN PROGRESS | All twelve object families with IDs, location, properties, common triggers/actions and validation | Many primitives exist; transitions, recruitment, management and shared events not integrated. |
| 5 Self-hosted test game | NOT STARTED | Editor-created two-level game proves speech/combat/items/doors/recruitment/management/death/saves/transition without engine content changes | Existing sample levels do not prove this acceptance. |
| 6 Project/save/export | IN PROGRESS | Versioned project and save data, migrations, references and standalone Windows export from editor | Level/save readers and CI archive exist; creator project/export workflow absent. |
| 7 Stable combat contract | IN PROGRESS | Document and prove 1–3 control, abilities, cooldowns, ranged/projectiles, interception, movement, downed/death/failure and save behavior | Melee/formation/action gates exist; remaining mechanics and contract incomplete. |
| 8 Validation/safety | IN PROGRESS | Creator-readable blocking errors and warnings catch invalid references and progression traps before export | Level validation exists; project-wide severity/graph validation absent. |

## Verification log

- 2026-09-18 baseline: Windows MSVC Release build succeeds, CTest 2/2.
- PR #1 c538cfd: Windows Build run 34857124685 success. This does not establish
  integration CI success.

## Known open defects

See CREATOR_ALPHA_AUDIT.md for the audit findings and migration risks. No gate
has yet met all its acceptance criteria.
