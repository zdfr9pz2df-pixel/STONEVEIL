# STONEVEIL — Codex Handoff

## Repository

`zdfr9pz2df-pixel/STONEVEIL`

## Companion documents

- `docs/ARCHITECTURE.md` — how the code is put together, the coordinate and format invariants, the
  catalog pattern, and where new work belongs. Read it before adding a system.
- `docs/EXPEDITION_DIRECTION.md` — proposed north star and tiebreaker distilled from Jules's design
  direction: make distance-from-safety legible and consequential. Proposed, not a substitute for
  explicit sign-off.
- `docs/COMBAT_DIRECTION.md` — options and open questions for making combat interesting, dangerous
  and non-repetitive. **Not an approved spec.** Do not implement from it without Jules choosing an
  option first.
- `docs/PROGRESSION_DIRECTION.md` — classes, skill trees, the form × modifier spell model, and the
  three progression tracks. **Not an approved spec**, and it assumes answers to two open questions in
  the combat document. Same rule: do not implement without Jules choosing the shape first.
- `docs/PORTRAIT_STATES.md` — the portrait reaction-state contract: stable state IDs, the priority
  resolver, tiering and fallbacks, and the frame/audio path conventions. The **system** is built and
  tested (`Portrait.hpp` / `Portrait.cpp`); the **art and voice lines are not started**.

## Current milestone

2026-09-18 integration authority: `integration/creator-alpha`. Read
`CREATOR_ALPHA_AUDIT.md`, `SHIP_READINESS.md`, and `SAVE_MIGRATIONS.md` first for
the current gated plan, verification evidence, and compatibility limitations.
PR #1 event compilers/runtime were reconciled with this editor, not substituted
for it. Existing triggers and doors now share the event runtime; story messages
queue in order, and editor-only room mood/purpose are not emitted as dialogue.
`LevelDocument` owns editor draft/path/history: New and blueprint imports are
untitled, Save As creates a new identity, and undo/redo restores document identity
and saved-state. Window close uses the unsaved-work confirmation. Windows
integration CI for the preceding published slice is green; the current local suite has nine
CTest cases. The project/export slice adds ProjectDocument, native file dialogs,
a Project panel, level registry/start selection, and player-only Windows export.
See PROJECT_WORKFLOW.md for use and limitations. The bundled project opens
automatically. Explicit project roots isolate textures, music and saves; Release
uses the static Microsoft runtime. AtomicFile now protects individual level,
campaign, manifest and save writes. Project-wide transactions remain unfinished.

The object-inspector slice adds raylib-free LevelEditing and a right-click
inspector for spawn, world objects, enemies, pickups, doors/gates and lights.
Right-click repeatedly to cycle stacked entities. Move preserves stable IDs and
retargets attached event coordinates (including legacy coordinate-only bindings);
EnterCell events remain at their authored locations. Delete refuses attached
events, including through the older Object Erase and door-repainting paths.
Undo/redo restores complete draft transactions. Inspect also exposes spawn
facing, door lock, pickup type, enemy archetype/health reset, light type, object
movement blocking, and existing object name/text editing. Other object-facing
fields do not yet exist; no dummy facing values were added.

Room/trigger transforms and the first project Character Creator are now local.
Rooms can be selected, moved and resized; subject-bound EnterRoom events follow
and remain inside the region. Cell events can move independently, while events
bound to a stable object refuse independent movement. Project > Characters uses
progressive Identity, Role, Recruitment and Advanced tabs. It persists versioned
`content/characters/characters.svc` definitions with stable numeric IDs,
reference keys, stats, traits, starter/recruitability flags, recruitment text,
basic attack/ability choices, equipment tags and portrait path. Project catalogs
drive New Game, roster, combat, HUD, save validation and export. Recruit and
Party Management Point object brushes compile into the shared event runtime;
recruits enter Reserve once, management supports active/reserve swaps with a
three-character active cap, and save/load preserves the full roster. Gate 3 is
still incomplete because Downed, equipment runtime, portrait import and richer
ability catalogs remain.

STONEVEIL v0.4 is a playable Windows C++17/raylib dungeon-crawler slice with an integrated, story-first dungeon editor.

The current tree includes:

- grid movement, collision, centered-eye raycasting, doors, keys, secrets, enemies, cooldown combat, healing, XP, victory, and defeat
- stable `CharacterId` values, `CharacterDefinition`, `Roster`, and `Party`
- New Game selection for any 1–3 starter characters
- permanent-death state and recruitment-ready reserve records
- a maximum active-party capacity of 3; additional recruits stay in Reserve
- a dynamic gameplay HUD that renders 1–3 active members
- version 6 saves that persist roster state, death, active members, inventory/XP, the current registered level, inactive visited-level snapshots, stable entity identities, and event fired-counts
- backward loading for the earlier save layouts
- a versioned external Gatehouse level in `content/levels/gatehouse.svl`
- categorized stable material IDs, per-cell surface overrides, and open-sky ceiling mode
- a stable light-type catalog and editor-placeable torch entities that light the raycast view by distance,
  with grid line-of-sight so a torch does not shine through solid stone
- a second-pass lighting model with darker ambient, warm torch contribution, softer falloff, directional
  wall-face shading, distance/corner dimming, and per-surface floor/ceiling light sampling
- level format version 8, which adds stable arrival markers and level-transition destinations while
  retaining recruit references and loading versions 1–7
- queryable authored water cells with depth, flow direction, and volume IDs, ready for later movement,
  audio, light-extinguishing, rendering, and AI rules
- a title-menu dungeon editor for structure, wall, floor, ceiling, object, story, event, light, and audio layers
- Object brushes for enemies, pickups, locked/unlocked doors, gates, props, shrines, notes, corpses,
  NPCs, recruits, and party-management points; text-bearing objects can be named and written directly
- Story-room rectangle authoring with room name, purpose, mood, lore note, and intended feeling
- Event triggers for entering a cell/room, opening a door, killing an enemy, collecting an item, and
  interacting with an object; playtests display authored discovery text through the real game HUD
- editor undo/redo, New Level, Save As, and dirty-draft confirmation before reload/new/menu exit
- a versioned campaign registry and title-screen level selector; Gatehouse is no longer a compiled filename
- a playable two-level Gatehouse/Underkeep loop with lore, recruitment, party management, return travel,
  cross-level persistence, save/reload, and project-wide route validation
- drag-and-drop audio import into `content/audio/music/`, retaining the existing safe path and streaming contract
- dynamically sized 4×4 through 64×64 maps with fitted editor rendering and dynamic runtime storage
- in-editor playtesting that runs the current unsaved `LevelDefinition` through the real gameplay systems
- clipboard import for `STONEVEIL_BLUEPRINT 1` blocks, allowing Codex-assisted copy/paste level
  generation that becomes ordinary editable/validated level data
- an `AudioSystem` presentation module with generated placeholder cues for menu, movement, doors,
  pickups, healing, combat, save/error, victory, and defeat events; it also streams and loops the
  level's selected music track until the mode/event changes it; press `M` to mute/unmute
- 256×256 wall, door, floor, and ceiling textures referenced from `MaterialDefinition::texturePath`;
  wall and door faces are sampled per raycast column, floor/ceiling surfaces are projected in small
  blocks, and placeholder swatches remain the fallback when a texture is absent or fails to load
- a portrait reaction-state catalog and a pure priority resolver (`PortraitTrack`) covering moods and
  event reflexes, with tiering and fallback chains so portrait art can land incrementally; no frames
  or voice lines exist yet and nothing in the HUD draws it
- Release CI, core tests, editor/level validation, and a single-program Windows playtest artifact

## Build and verification

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Expected multi-configuration Windows outputs:

```text
build/bin/Release/stoneveil.exe
build/bin/Release/content/levels/gatehouse.svl
```

CI must keep uploading the artifact named `STONEVEIL-Windows-Playtest`.

## Architecture now in place

- `Game` coordinates screens and input; player state, raycasting, combat, roster, party, level data, material data, and persistence are separate modules.
- `LevelDefinition` is the shared authoring model used by the external level loader, validator, editor, and `Dungeon` runtime construction.
- `LevelIO` owns the versioned `.svl` format and rejects malformed maps, invalid stable material references, unsafe boundaries, bad object placement, duplicate surface overrides, and an unreachable or missing exit.
- `MaterialDefinition` provides a stable ID, surface kind, category, display name, and placeholder swatch. Texture references can be added later without changing authored level IDs.
- The editor and game are modes of `stoneveil.exe` and read the same level file. Press `E` on the title screen to edit.
- Play creates a temporary runtime dungeon directly from the editor draft. It does not require saving and Escape returns to the intact draft.
- `Party` owns active capacity. `Roster` is independent of that capacity and retains recruited reserves and permanent deaths.

## Critical coordinate invariant

The logical player occupies integer cell `(px, py)`. The rendered eye is always centered at `(px + 0.5, py + 0.5)`. DDA setup must continue from that centered eye position. Collision, interaction, enemies, objects, and authored map coordinates use the integer cell convention.

## Dungeon editor v0.4 behavior

- Structure brushes: open floor, solid wall, door, secret door, exit, and player spawn
- Surface layers: categorized wall, floor, and ceiling materials
- Ceiling option: open sky
- Per-cell painting by click or drag
- map resizing from 4×4 through 64×64; shrinking refuses to discard the spawn, exit, enemies, or pickups
- Selected finish can become the level default
- Boundary locking and object/spawn collision protection
- validation before save, reload from disk, integrated playtesting, and headless `--validate <level.svl>` support
- pickups, enemies, doors/locks/gates, props, shrines, notes, corpses, and NPCs are editable on the Object layer
- the Lights layer shows each placed light's approximate radius/half-radius coverage, previews the
  selected light radius under the mouse before placement, and includes a visibility diagnostic overlay:
  warm cells are reached by runtime light sampling while red-crossed cells are in range but blocked by
  grid occlusion
- the Audio layer lists supported tracks under `content/audio/music/`; selecting one writes its
  safe relative path into the level and gameplay loops it until returning to the title/editor or
  reaching victory/defeat

**Audio/music contract:** authored level music is stored as a relative path under `content/audio/`,
currently selected through the editor's Audio layer from `content/audio/music/`. Supported extensions
are `.wav`, `.ogg`, `.mp3`, and `.flac`. The engine streams music with raylib `Music`, so longer
loops should be music/ambience assets rather than `Sound` cues. Missing tracks fail silently apart
from raylib/STONEVEIL warnings; the level remains playable.

Editor swatches remain the functional fallback and the editor still paints with them. The material buttons
show a `TEX` / `NO TEX` chip so missing texture-backed materials are visible while authoring. The raycaster
prefers a material's `texturePath` when one is present and loads, and falls back to the swatch when it is
not — so an untextured material is still fully playable.

**Texture contract, as actually shipped:** 256×256 PNG, one image per material, sampled per screen
column or projected surface block by `Raycaster`, point-filtered, under
`content/textures/<surface>/<category>/<name>.png`. Wall, door, floor, and non-sky ceiling materials can
all be texture-backed. This contract supersedes the earlier instruction to defer texture production — that
work has landed. Changing the 256×256 size or the path convention now means reauthoring every texture, so
treat both as settled. Texture lookup first tries the executable directory's `content/`, then the current
working directory, then the source-tree `content/` in local developer builds; failures log every searched
path before falling back to swatches.

## Party and recruitment direction

- A new game still presents exactly the three starters and allows selecting 1–3.
- Maximum active capacity is 3. The 2026-09-18 Creator Alpha brief supersedes the older six-slot rule.
- Recruitment adds a character to the reserve roster first. Joining the active party is a separate decision at a Party Management Point.
- Capacity has been persisted since save version 5; legacy version 4 capacities of 4–6 clamp to 3 on load.
- Death remains permanent unless the user explicitly changes that pillar.
- The management screen scrolls through an uncapped authored roster while the HUD remains limited to the active 1–3.

## Recommended next engineering milestone

Continue the editor incrementally:

1. Add sprite definitions and final first-person art for authored props, shrines, notes, corpses,
   gates, and NPCs. They currently use clear editor symbols and simple gameplay billboards.
2. Replace automatic Save As filenames with a native/in-engine filename browser and add campaign-registry editing.
3. Move the material catalog to an external manifest. **Partly done** — `MaterialDefinition` now carries
   `texturePath` and a core test asserts every referenced file exists, but the catalog itself is still
   compiled in rather than loaded from a manifest.
4. ~~Expand the campaign registry with exits that name their destination level and persistent campaign flags.~~ **Done for destinations and persistent fired-event story state.** A dedicated named fact editor remains future work.
5. ~~Teach the renderer to use actual tiled textures while retaining the placeholder fallback.~~ **Done**
   for walls, doors, floors, and non-sky ceilings. The next rendering slice should be object/sprite
   presentation and water-specific visuals.
6. Use the new water state for movement slowdown, splash/audio propagation, light extinguishing, and
   water rendering; do not infer water mechanically from floor material.
7. Replace generated placeholder audio cues with authored `.wav`/`.ogg` assets behind the existing
   `AudioCue` IDs, then add event-triggered music swaps for combat, sanctuary, boss, and victory
   states using the level-music path as the default exploration loop.
8. ~~Add recruitment triggers and a party-management screen using stable character IDs and saved capacity.~~ **Done.**
9. Extract remaining screen/UI presentation from `Game.cpp` as those screens grow.

The Object/Story/Event milestone follows the same authored-data path first exercised by lights. Two
deliberate lighting simplifications remain open. A light has no facing, so one placed inside a
wall lights both sides of it. The renderer samples lighting per wall column and per projected
floor/ceiling block, not per final pixel; this is a deliberate performance/clarity compromise until the
renderer has a more formal scene-lighting pass. The editor's Lights layer is now the first lighting
debug view and should be preserved/expanded before attempting more realistic shadows.

Keep the current 4–64 dimension safety range until larger-map performance and editor navigation are measured.

## Non-regression requirements

- Keep the Release build and all nine CTest cases green.
- Preserve movement, collision, doors, combat, save/load compatibility, and the centered-eye raycaster.
- Keep loading older level files; saving always upgrades them to the current version.
- Keep the single executable and shared `content` folder in the downloadable Windows playtest.
- Do not fuse editor-only behavior into gameplay systems.
- Do not add narrative before the editor/content contracts are stable. Wall and door art is now in and
  bound by the texture contract above; portrait and voice art is deliberately still deferred, and the
  state contract in `PORTRAIT_STATES.md` exists so it can be produced once rather than twice.
