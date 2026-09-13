# STONEVEIL — Codex Handoff

## Repository
`zdfr9pz2df-pixel/STONEVEIL`

## What exists now
The current prototype already has:
- C++17/raylib/CMake project scaffolding
- Windows GitHub Actions build
- packaged Windows playtest artifact
- grid-based first-person exploration
- 90-degree turning and strafing
- raycast wall rendering
- collision
- doors/keys
- three placeholder party members
- real-time melee cooldown combat
- enemy pursuit/attacks
- healing consumables
- XP
- save/load foundation
- victory/defeat states

## Most recent engine correction
The initial renderer treated the integer player cell coordinates as the actual ray origin, which placed the camera on the corner of the occupied grid cell. This caused visible wall intersection/clipping on one side of the viewport.

The renderer now uses:

```cpp
const double posX = static_cast<double>(px_) + 0.5;
const double posY = static_cast<double>(py_) + 0.5;
```

DDA setup must continue from that centered eye position.

## Product direction now locked
### Party / roster
- There are three starter characters at the beginning.
- The player may start with any one starter, any pair, or all three.
- Maximum active party size: 3.
- More characters are recruitable later.
- Characters can die permanently.
- The roster/save system therefore needs stable character IDs and explicit states such as available, active, reserve, downed/dead as design evolves.
- The HUD must dynamically support 1–3 active party members instead of assuming exactly three.

### Permadeath
Permadeath is a core design pillar. Do not implement ordinary RPG resurrection unless the user explicitly changes this. A downed/rescue window is a possible design option but has not been locked as final; do not silently make it canon.

### Level 1
The first proper level should be a small authored dungeon floor suitable for proving the core loop. Current visual concept suggests:
- opening/entrance area
- branching route
- side room
- locked gate/door
- key or lever interaction
- enemies
- consumable pickup(s)
- at least one secret or optional discovery
- staircase/exit

The exact map should remain original and should be implemented as game data rather than being fused into renderer logic.

### UI / presentation
The current desired presentation is a handcrafted pixel-art 1990s PC dungeon-crawler UI:
- large first-person viewport
- compact map/compass area
- action slots
- quick items
- message log
- party portraits/status panels
- dark stone / iron / brass / crimson / slate palette

Do not put a large STONEVEIL logo across the gameplay HUD.

## Recommended next engineering milestone
Treat the next substantial version as a systems refactor rather than adding more placeholder content on top of `Game.cpp`.

Suggested order:
1. Extract player/view state and raycaster/rendering into clearer modules.
2. Add `CharacterDefinition` + stable character IDs.
3. Add `Roster` and `Party` state with maximum active size 3.
4. Add New Game party-selection screen allowing 1–3 starters.
5. Extend save format to persist roster, character life/death state, active party, inventory and progression safely.
6. Implement permadeath semantics deliberately and test save/reload behavior.
7. Make HUD layout dynamic for 1–3 party members.
8. Move dungeon content toward data-driven Level 1 definitions.
9. Implement the first real Level 1.
10. Add automated sanity checks where feasible for map bounds, spawn validity, player/camera coordinate consistency, and save round-tripping.

## Engineering caution
`src/Game.cpp` is currently carrying too many responsibilities. Do not perform a giant rewrite without maintaining a working build. Refactor incrementally, keep CI green, and preserve a runnable playtest after each meaningful step.

## Playtest delivery
`.github/workflows/windows-build.yml` builds the Release configuration, packages the Windows executable, and uploads a `STONEVEIL-Windows-Playtest` artifact. Keep this working so the user can test without maintaining a local development toolchain.

## First Codex task suggestion
Audit the current source first. Then propose and execute the smallest safe refactor that introduces a proper roster/character model and a 1–3-character party selection flow without regressing movement, raycasting, CI, or playtest packaging.
