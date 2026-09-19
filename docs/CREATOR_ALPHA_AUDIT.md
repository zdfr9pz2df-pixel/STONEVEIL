# Creator Alpha integration audit — 2026-09-18

## Authority and repository state

The user's Deep Work Engineering Brief is the current specification. Its maximum
of three active characters supersedes the earlier six-character rule. Reserve is
independent of active capacity. Exploration retains one cell, facing, and camera.

Audit baseline: local main and origin/main point to e509320. Eleven tracked
files are modified; most of the working v0.4 engine, content, tests, and documents
are untracked. This work must be checkpointed before integration, not reset.
Smoke captures are local evidence, not source assets.

Authoritative integration branch: integration/creator-alpha (created from local
main with the entire existing working tree retained).

GitHub branches inspected: main and feature/event-system-foundation.
Draft PR #1: https://github.com/zdfr9pz2df-pixel/STONEVEIL/pull/1
Head: c538cfd8459677dd42537e079c052070ec73e9a8.
Its Windows Build run 34857124685 succeeded. This is evidence for that PR only,
not the local editor or the combined integration branch.

Local baseline Windows Release build succeeds; CTest passes 2/2.
The bundled git executable initially cannot locate its HTTPS helper; connector
reads work. Remote publication/CI must be verified separately.

## Current architecture and gate coverage

1. Consolidation: local v0.4 editor plus a separate event-foundation PR. No
   authoritative combined commit existed at audit time.
2. Editor: geometry/surfaces, objects, rooms, message triggers, audio/lights,
   draft playtest, snapshot undo/redo, New Level and automatic Save As exist.
   Project operations, Open Level, safe unsaved-document identity, moving and
   rotating arbitrary objects, and contextual behavior inspectors are incomplete.
3. Characters: static numeric-ID catalog, mutable CharacterRecord, reserve/death
   records, Party, and save roundtrip exist. Character Creator, authored
   definitions, Downed, management points, and recruitment presentation are absent.
4. Objects: existing data covers many kinds, but lacks a shared action runtime
   in this checkout. Props/NPCs are text interactions; exits only mean victory.
5. Self-hosting: Gatehouse, pressure-cooker, and systems-test are data files.
   No authoring-only recruitment/transition/export walkthrough has been proved.
6. Persistence/export: .svl v1–6 and .sav legacy/v2–4 readers exist; campaign v1
   is a level list. CI packages the development executable. This is not a
   creator project format or an in-editor standalone-game exporter.
7. Combat: cooldown party attack, enemy windup/recovery, deterministic formation
   targeting, and ActionGate movement timing exist. Special abilities, ranged
   attacks/projectiles, Downed, and stable combat-save semantics are incomplete.
8. Validation: level validation and core tests exist. No severity-separated,
   project-wide reference/reachability service exists.

Core/rendering boundaries: Dungeon, LevelIO, roster, combat, catalogs and saves
are raylib-free; Game and LevelEditor coordinate/render UI. Player::eyeX/eyeY
preserve the centered camera. Lighting, material textures, audio streaming,
blueprint parsing, water metadata and portrait priority resolver must be retained.

## Conflicts and defects identified before implementation

- Local WorldAuthoring::TriggerSystem supports only messages. PR EventRuntime
  supports conditions/actions/priorities/signals. Consolidate on EventRuntime;
  retain v6 authoring as a compatibility adapter, not a second scripting engine.
- PR Game.cpp is based on the early monolithic prototype. Replacing the local
  Game.cpp would lose roster/editor/audio/rendering work. Port its integration.
- PR Story Tile content is hardcoded in configureEvents; move ordinary content
  to authored data rather than reintroducing that configuration function.
- PR tests use assert, disabled by NDEBUG in Release. Enable effective checks.
- PR dispatch precomputes candidates; recursive firing can cause a later Once
  candidate to run twice. Callback mutation can invalidate vector references.
  Guard dispatch and test nested ordering and bounded recursion.
- Local fired trigger IDs are not saved. Loading can lose one-shot semantics.
- Local Game uses only messages.back(), losing simultaneous story messages.
- Party allows six; Roster::setActiveParty has no cap. Save migration must retain
  displaced living members in Reserve instead of deleting them.
- Save entities are matched by index. Reordering authored objects can assign
  runtime state to the wrong stable ID.
- New Level retains the old path; Save can overwrite the previous level.
  Save As can preserve duplicate level identity. Undo snapshots omit document
  identity and Exit painting snapshots occur after removing the prior exit.
- Legacy level migration indexes rows before full shape validation.
- Editor-only room purpose/mood must not automatically become player dialogue.
- CampaignInventory exists but Game still owns loose key/potion counts.
- Documents claim older module sizes and incomplete features as completed.

## Implementation order and files

First preserve/checkpoint local work and reconcile EventSystem.hpp/.cpp,
Authoring.hpp/.cpp and EventSystemTests.cpp from PR #1. Adapt WorldAuthoring,
Game, Dungeon, LevelIO and SaveSystem to one event path; add focused regression
coverage. CMake and Windows workflow retain CTest and playtest packaging.

Next establish editor document/project ownership and transactions in core
modules, then project/character authoring with stable IDs and roster operations.
Build contextual object interfaces over common actions, author a tiny two-level
test game, then implement project export and fill combat/validation gaps revealed
by that exercise. LevelEditor presentation should delegate testable decisions.

Expected existing files: Party, Roster, Character, SaveSystem, Game,
WorldAuthoring, Dungeon, LevelIO, LevelBlueprint, LevelEditor, Campaign,
CombatSystem/CombatTuning, tests, CMake, workflow, and architecture/handoff docs.
Expected new modules: EventSystem/Authoring (PR), editor document/project IO,
character catalog IO, project validation/export, runtime campaign/session state.

## Compatibility risks

Keep all supported level readers. New authored fields require a version bump.
Player-save event state and entity IDs require a version bump with legacy readers.
Old capacity 4–6 saves need an explicit three-active migration preserving roster
health/death/progression. Never infer IDs from names. Keep stable numeric
character IDs; do not replace them with display strings.

No gate is verified merely by this baseline build. SHIP_READINESS.md tracks
objective evidence and unfulfilled acceptance criteria.
