# STONEVEIL — Architecture

Orientation for whoever opens this repo next, human or agent, after a gap. `README.md` says what the
game does and how to build it. `CODEX_HANDOFF.md` says where the milestone sits. This file says how
the code is put together and where new work belongs.

## The shape of the program

One executable, three entry modes, selected in `src/main.cpp`:

| Invocation | What runs |
| --- | --- |
| `stoneveil.exe` | `Game::run()` — title, party select, gameplay, editor |
| `stoneveil.exe --validate <level.svl>` | Headless level validation. No window, no raylib init. Used by CTest. |
| `stoneveil.exe --capture-ui <dir>` | Hidden-window screenshots of title, editor/object/story/light views, Character Creator, gameplay, and Party Management |

The dungeon editor is a **mode of the game**, not a second program (`Game::Mode::Editor`). It shares
the level file, the material and light catalogs, and the runtime `Dungeon`. Keep it that way — the
Windows playtest artifact is one exe plus one `content/` folder.

## The invariant that breaks everything if you touch it

The logical player occupies an **integer cell** `(px, py)`. The rendered eye is **always** the center
of that cell, `(px + 0.5, py + 0.5)` — `PlayerState::eyeX()` / `eyeY()`. DDA setup in `Raycaster`
starts from the centered eye. Collision, interaction, enemy positions, pickups, lights and authored
map coordinates all use the integer cell.

This was a shipped bug once: the camera sat at the tile corner and the viewport clipped into nearby
walls. Do not regress it.

Directions are indices 0–3, from `src/Player.cpp`:

| Index | Facing | dx | dy |
| --- | --- | --- | --- |
| 0 | North | 0 | −1 |
| 1 | East | +1 | 0 |
| 2 | South | 0 | +1 |
| 3 | West | −1 | 0 |

`turn(+1)` rotates clockwise. `movementTarget(forward, strafe)` treats `strafe = +1` as the player's
right. Gatehouse spawns facing 1 (east).

## Module map

Nothing below depends on `Game`. `Game` depends on all of it.

**Core simulation** — no raylib, no I/O beyond files:

- `Character` — `CharacterId` (stable `uint32_t`, 1001–1003 for starters), `CharacterDefinition`
  (the catalog: name, role, maxHp, power), `CharacterRecord` (the mutable per-run state: hp, xp,
  status). Built-in definitions are fallback data; a project may provide the
  versioned `content/characters/characters.svc` catalog. Records remain save
  data. Keep that definition/state split.
- `Roster` — every character that exists, with status `Unrecruited → Reserve → Active`, plus `Dead`,
  which is terminal. Owns damage/heal and permanent death. Independent of party capacity.
- `Party` — who is currently in the field. Capacity starts at 3, caps at 3 (`MaximumCapacity`).
  Membership is an ordered `vector<CharacterId>`; nothing currently reads that order as meaning.
- `Player` — grid cell, facing, and the centered-eye accessors. Deliberately tiny.
- `Dungeon` — runtime level: tiles, pickups, enemies, lights, water state, surface overrides, spawn.
  Built from a `LevelDefinition`. Mutable during play (doors open, secrets reveal, pickups taken,
  enemies die).
- `CombatSystem` — party attack cooldown, enemy pursuit and attacks, RNG. Holds no level state; it
  operates on the `Dungeon` and `Roster` handed to it.
- `Material` / `Lighting` / `Portrait` — data catalogs (see below). `Portrait` also owns `PortraitTrack`,
  the pure priority resolver that turns character condition and events into a displayed state.
- `LevelIO` — the versioned `.svl` text format: load, save, validate.
- `LevelDocument` — editor draft, file identity, save points and bounded undo/redo
  history. It is raylib-free and tested separately. New/imported drafts have no
  file path; Save As creates a distinct level ID and never overwrites a sibling.
- `LevelEditing` — raylib-free selection lookup, transactional entity movement
  and guarded deletion. Stable entity IDs survive moves; v6 light identity is
  still its unique cell plus light type. Attached event coordinates follow the
  entity; independent EnterCell events remain in place. UI commits successful
  candidates through LevelDocument history and never records failed moves.
- `LevelBlueprint` — a copy/paste import language for Codex-assisted level generation. It parses
  `STONEVEIL_BLUEPRINT 1` text into ordinary `LevelDefinition` data, resolves friendly material/light
  aliases to stable IDs, accepts water-volume commands, and validates through `LevelIO` before the
  editor accepts it.
- `LevelOne` — the built-in fallback `LevelDefinition`, used when the external level file will not
  load. Keep it in sync with `content/levels/gatehouse.svl`.
- `SaveSystem` — the versioned `.sav` format.
- `WorldAuthoring` — stable door/object/room/trigger data plus the runtime `TriggerSystem`. It is the
  story-facing contract: authored meaning lives here, while presentation stays in `Game`/`LevelEditor`.
- `Campaign` — the versioned campaign registry. It links stable level IDs to safe
  `content/levels/*.svl` paths so the executable is no longer tied to Gatehouse.

**Presentation** — these include `raylib.h`:

- `Raycaster` — draws the first-person viewport. DDA per screen column, wall/door textures from the
  ray-hit cell's material, projected floor/ceiling texture blocks from sampled world cells, and
  swatch fallback when a texture cannot be loaded. Lighting is still intentionally stylized rather
  than physically correct: warm authored lights are line-of-sight checked, walls receive directional
  face shading, and floor/ceiling blocks sample light at their projected surface point.
- `AudioSystem` — owns raylib audio initialization, shutdown, generated placeholder cues, streamed
  looping level music, mute state, and playback for UI/gameplay events. Keep raw `PlaySound` /
  `PlayMusicStream` calls here rather than scattering them through gameplay code; later asset-backed
  `.wav`/`.ogg` cues should replace the generated tones behind the same `AudioCue` IDs.
- `LevelEditor` — editor state and its own immediate-mode UI. Owns a draft `LevelDefinition`.
  Object, Story, and Event layers place gameplay objects, draw named room regions, edit lore text,
  and connect enter/open/kill/pickup/interact events to discovery messages. The Audio layer accepts
  supported files dropped onto the editor window and copies them into `content/audio/music/`.
  The Lights layer includes a diagnostic overlay: warm cells are actually reached by runtime light
  sampling, red-crossed cells are inside a light radius but blocked by grid occlusion, and rings show
  range only. The Audio layer scans `content/audio/music/` for `.wav`, `.ogg`, `.mp3`, and `.flac`
  tracks and stores a safe `content/audio/...` relative path on the level.
- `Game` — mode machine, input, HUD, title/party/end screens. Coordinates; does not simulate.

## The testable/presentation boundary is enforced by CMake, not by folders

`stoneveil_core` compiles **only the raylib-free sources** once; the game and three core test executables link this library. It does
not link raylib. `Raycaster.cpp`, `LevelEditor.cpp`, `Game.cpp` and `main.cpp` are excluded.

The practical rule: **logic you want covered by tests must live in a source file that the test target
can compile.** If you put a rule inside `Game.cpp` or `LevelEditor.cpp`, it is permanently untestable
without restructuring. When adding a system, put the decisions in a core module and leave only
drawing and input in the presentation files.

`LevelEditor.hpp` is raylib-free (it includes `Dungeon.hpp`), so core code may include it. `Game.hpp`
includes `raylib.h` and may not be included from a core module.

## The catalog pattern

Four catalogs now follow the same shape, and a fifth should too:

```
struct XDefinition { std::string id; /* stable, authored into files */ ... };
const std::vector<XDefinition>& xCatalog();     // static, compile-time
const XDefinition* findX(const std::string& id); // nullptr when unknown
```

- `materialCatalog()` — `material.<surface>.<category>.<name>`, plus a placeholder swatch and optional
  texture path under `content/textures/`
- `lightCatalog()` — `light.torch.<name>`, plus tint, intensity, radius
- `characterDefinitions()` — numeric `CharacterId` rather than a string, for historical reasons
- `portraitStateCatalog()` — `portrait.<state>`, plus category, tier, priority, duration and fallback

**Authored files store only the stable ID.** Swatches, tints, texture paths, and stats can change freely;
authored levels never need rewriting. `LevelIO::validate` rejects unknown
IDs, which is what keeps a renamed catalog entry from silently becoming an invisible wall.

An `EnemyDefinition` catalog is the obvious next one — see `COMBAT_DIRECTION.md`.

## Data flow

**Level authoring and play**

```
content/levels/gatehouse.svl
  → LevelIO::load  (validates; refuses malformed maps, bad IDs, unreachable exits)
  → LevelDefinition          ← the shared authoring model
  → Dungeon{definition}      ← runtime, mutable
```

`LevelEditor` holds its own `LevelDefinition` draft. Pressing `P` builds a temporary `Dungeon` from
that unsaved draft and drops into real gameplay; `Esc` returns to the intact draft. Saves are
disabled during a playtest, deliberately — see `Game::editorPlaytest_`.

**Format versions**

- `.svl` is at **version 7**. Versions 1–6 still load; saving always writes the current version.
  Backward loading is a non-regression requirement. Versions 3–5 added enemy archetypes, water, and
  music. Version 6 adds stable pickup/enemy/door IDs, door lock/gate metadata, world objects, story
  rooms, and triggers. Version 7 adds stable character references for recruit objects. Older levels
  receive deterministic legacy IDs and locked-door metadata in memory.
- `.campaign` is at **version 1** and lists stable level IDs, display names, a starting level, and safe
  relative level paths. The title screen can cycle registered levels without recompilation.
- `.svc` character catalogs are at **version 1**. They are project definitions,
  exported with the game, and are not embedded in player saves.
- `.sav` is at **version 5**, with readers for 2–4 and the pre-versioned layout. Version 5 stores stable enemy/pickup IDs and event fired-counts; see `SAVE_MIGRATIONS.md`. `SaveSystem::load`
  copies the caller's `Dungeon` and overwrites tiles, pickups and enemies — so anything that is
  *authored* level data (materials, lights, dimensions) survives a load without touching the save
  format. That is why lights needed no save-version bump.

When you add mutable runtime state, it needs a save-version bump. When you add authored data, it needs
an `.svl` version bump and a loader that tolerates the older version.

## Where to add things

| You want to add | Put it in |
| --- | --- |
| A new placeable object type | `LevelDefinition` + `Dungeon` + `LevelIO` (bump `.svl`) + an editor layer |
| A new catalog of authored definitions | New core module following the catalog pattern; register in both CMake targets |
| A new blueprint command | `LevelBlueprint` plus core tests; output must remain normal `LevelDefinition` data |
| A water/rendering/audio/AI rule | Read `Dungeon::waterDepthAt`, `waterFlowAt`, and `waterVolumeAt`; do not infer water from floor texture |
| A combat or progression rule | `CombatSystem` / `Roster` / `Party` — never `Game.cpp` |
| A new sound effect or music hook | `AudioSystem` for cue definition/playback; call it from presentation flow only |
| A new screen | `Game` for now; extract once it grows (item 7 in the handoff's next-milestone list) |
| Anything you want a test for | Any source file listed in the `stoneveil_core_tests` target |

New core `.cpp` files belong in the `stoneveil_core` source list in `CMakeLists.txt`. Presentation sources belong only in the game target.

## Build and verification

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

raylib 5.5 is fetched by CMake on first configure. Five CTest cases: `stoneveil_core_tests` (the core
checks), `stoneveil_event_tests`, `stoneveil_compatibility_tests`, `stoneveil_document_tests`, and
`stoneveil_editor_validate` (runs the real exe headlessly against the shipped level).
CI is `.github/workflows/windows-build.yml` and must keep uploading the `STONEVEIL-Windows-Playtest`
artifact.

Warnings are `/W4 /permissive-` on MSVC and `-Wall -Wextra -Wpedantic` elsewhere. `LevelOne.cpp`
emits two known `-Wmissing-field-initializers` warnings on GCC/Clang from its aggregate initializer;
that is pre-existing and harmless.

## Known rough edges

- `Game.cpp` remains a large coordinator (~950 lines) and holds all screen drawing. Extracting screens is a named next step.
- Authored world objects have editor symbols, interaction text, and simple first-person placeholder
  billboards, but no sprite catalog or final first-person art yet.
- `frontEnemyIndex` is called with `maxDistance = 1` for attacks but `6` for drawing the enemy, so the
  sprite appears well before you can hit it.
- `LevelOne.cpp` duplicates the Gatehouse by hand. Any edit to `gatehouse.svl` should be mirrored
  there or the fallback drifts.
- The campaign registry is read-only in the UI; adding/removing registry entries still means editing
  `content/campaigns/stoneveil.campaign` as text.
