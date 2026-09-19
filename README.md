# STONEVEIL

STONEVEIL is a systems-first C++ dungeon crawler prototype inspired by the feel of early-1990s first-person party RPGs, with original code, layout, names, and placeholder visuals.

## v0.4 playable slice and story-editor foundation

- Grid-based first-person exploration
- 90-degree turning plus strafing
- Raycast dungeon rendering
- Collision
- Authored Gatehouse level with a locked door, key, branching route, and hidden niche
- New Game selection for any 1–3 of the three starter characters
- Stable character IDs, data-driven character definitions, an uncapped reserve roster, and a progression-ready active party
- Maximum active-party size is 3; additional recruited characters live in Reserve
- Dynamic 1–3 character gameplay HUD
- Project Character Creator with stable IDs, starter/recruit flags, stats, traits, and recruitment text
- Recruit objects and generic Party Management points backed by the shared event runtime
- Permanent character death state and a save-backed active/reserve roster
- Real-time cooldown melee combat
- Enemy pursuit and attacks
- Healing draught pickups/consumption
- XP counter
- Level exit/victory condition
- Versioned save/load for roster, party, deaths, opened doors, secrets, pickups, and enemies
- Windows CI build, core tests, and downloadable playtest artifact
- Versioned external level files shared by the game and editor
- Integrated dungeon editor for structure, wall, floor, ceiling, and light painting
- Object layer for enemies, pickups, doors/locks, gates, props, shrines, notes, corpses, NPCs,
  recruit interactions, and party-management points
- Story rooms with names, purpose, mood, lore notes, and intended feeling
- Event triggers for room/cell entry, doors, kills, pickups, and object interaction
- In-editor text tools for notes, inscriptions, dialogue, room lore, and discovery messages
- Undo/redo, New Level, Save As, and unsaved-draft warnings
- Versioned campaign registry with a title-screen level selector
- Drag-and-drop audio import into the editor's music library
- Clipboard blueprint import for Codex-assisted level generation
- Resizable maps from 4×4 through 64×64 with automatic canvas fitting
- In-editor playtesting of the current unsaved draft
- Categorized material catalog with per-cell overrides and open-sky ceiling mode
- Editor-placeable torch entities that cast warm, distance-attenuated light and do not bleed through walls
- Directional wall shading, darker ambient, and per-surface floor/ceiling light sampling for a moodier dungeon view
- Authored water cells with depth, flow, and volume IDs for future movement, audio, light, AI, and rendering rules
- Wall, door, floor, and ceiling texture support with placeholder swatch fallback for unfinished materials
- Generated placeholder audio cues for menus, movement, doors, pickups, healing, combat, save/error, victory, and defeat
- Save-time checks for boundaries, spawn placement, materials, and exit reachability

## Documentation

- `docs/ARCHITECTURE.md` — codebase orientation: module map, invariants, format versioning, where to add things
- `docs/CODEX_HANDOFF.md` — current milestone state and non-regression requirements
- `docs/EXPEDITION_DIRECTION.md` — proposed expedition north star and distance-from-safety tiebreaker
- `docs/COMBAT_DIRECTION.md` — combat design options and open questions (exploratory, not approved)
- `docs/PROGRESSION_DIRECTION.md` — classes, skill trees, and spell model (exploratory, not approved)

## Tech

- C++17
- raylib 5.5 (fetched automatically by CMake)
- CMake 3.24+

## Build on Windows

Requirements: Git, CMake, and Visual Studio 2022/2026 with the Desktop development with C++ workload.

```powershell
git clone https://github.com/zdfr9pz2df-pixel/STONEVEIL.git
cd STONEVEIL
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\bin\Release\stoneveil.exe
```

The first configure downloads raylib automatically.

## Dungeon editor

Run `stoneveil.exe` and click **Dungeon Editor** on the main menu (or press `E`). The editor opens the Gatehouse level used by the game; it is part of the same application and package.

| Input | Action |
| --- | --- |
| Layer buttons | Edit map, surfaces, objects, story rooms, events, lights, or audio |
| Left click/drag on map | Paint the selected brush, material, or light |
| W- / W+ / H- / H+ | Resize the map one cell at a time, from 4×4 to 64×64 |
| Random | Randomize surface defaults and per-cell wall/floor/ceiling variation |
| Make Selection Level Default | Set the base finish for that surface |
| P or Play | Play the current draft without saving; Esc returns to editing |
| Ctrl+S | Validate and save |
| Ctrl+Shift+S | Save As using a safe unique level filename |
| Ctrl+Z / Ctrl+Y | Undo / redo up to 64 editor states |
| Ctrl+N | Create a new 16×16 story level template |
| Ctrl+R | Discard edits and reload from disk |
| V | Run validation without saving |
| Import | Open the blueprint import panel |
| Menu or Esc | Return to the title screen |

On the title screen, use the `<` / `>` buttons or `[` / `]` keys to choose any level registered in
`content/campaigns/stoneveil.campaign` before starting a new game or opening the editor.

On the Object layer, choose a brush and click the map. Notes, inscriptions, shrines, corpses, props,
NPCs, recruits, and party-management points can be selected and given interaction text. On Story, click two corners to draw a room,
then edit its story purpose and intended feeling. On Event, choose the event type, click its authored
subject or cell, and edit the message that appears during playtesting.

To import music or ambience, drag `.wav`, `.ogg`, `.mp3`, or `.flac` files onto the editor window,
then select the imported track on the Audio layer. Texture/sprite/portrait import will extend this same pipeline later.

On the Lights layer, pick a torch type and click a cell to place it, click again to swap the type, or select Erase Light to remove one. A light sitting in a wall cell reads as a sconce and lights both faces of that wall; lights carry stable IDs and live in the level file, not in the save.
Placed lights show their approximate radius and half-radius coverage on the editor map, and the selected light previews its radius under the mouse before placement.
On the Lights layer, warm cells show light that actually reaches through the runtime visibility trace; red-crossed cells are inside a light's radius but blocked by occlusion.

Blueprint import is designed for copy/paste level generation. Ask Codex for a `STONEVEIL_BLUEPRINT 1` block, copy it, click **Import**, then **Paste + Import**. The imported level becomes the editable draft and must pass the same validation as a saved `.svl` file.

Small example:

```text
STONEVEIL_BLUEPRINT 1
NAME "THE LOW MEAD HALL"
SIZE 26 18
DEFAULTS WALL timber.hewn FLOOR timber.planks CEILING timber.rafters
FILL wall
ROOM 2 2 22 12 floor
ROOM 10 14 6 2 floor
DOOR 12 13
SPAWN 13 15 NORTH
EXIT 13 2
LIGHT brazier 7 7
LIGHT brazier 13 7
LIGHT candle 22 4
PICKUP key 5 11
PICKUP potion 20 11
ENEMY 8 6 18
ENEMY 18 6 18
SURFACE floor 5 5 16 1 mud
WATER 5 8 3 1 2 EAST 4
END
```

The editor still uses simple 2D swatches for authoring, while the raycaster uses available wall, door, floor, and ceiling textures with swatch fallback for unfinished materials. Water is already authored/queryable gameplay state, but does not yet slow movement, extinguish lights, alter sound, or render as animated water. Stable material IDs and categories are in place now; object/sprite and final audio assets can be attached later without changing authored maps.

## Controls

| Input | Action |
| --- | --- |
| Enter on title | Open New Game party selection |
| E on title | Open the integrated dungeon editor |
| 1 / 2 / 3 or click | Select/deselect a starter |
| Left / Right, then Space | Focus and toggle a starter |
| Enter on New Game | Begin with the selected 1–3 characters |
| W / S or Up / Down | Move forward/back |
| A / D | Strafe left/right |
| Q / E or Left / Right | Turn 90 degrees |
| Space | Party melee attack |
| F | Interact/open locked door |
| H | Use healing draught |
| M | Mute/unmute audio |
| F5 | Save |
| F9 | Load |
| Esc | Return to title |

## Architecture

Player/view state, raycasting, dynamically sized level data and serialization, material definitions, editor state, character definitions, roster, party, combat/enemy simulation, and persistence now live outside `Game.cpp`. `Game` coordinates the title, gameplay, and integrated editor modes. Editor playtesting constructs the real runtime dungeon directly from the unsaved `LevelDefinition`. Reserve-roster size is independent of the active party, whose maximum capacity is three. See `docs/CREATOR_ALPHA_AUDIT.md` and `docs/SHIP_READINESS.md` for the current engineering plan and verified scope.

Narrative and final art remain intentionally deferred. The current goal is a stable core loop and a robust base for recruitment, deeper combat, inventory, and additional authored levels.
