# STONEVEIL — Codex Instructions

## Project identity
STONEVEIL is a native Windows C++17 first-person party dungeon crawler inspired by the feel of early-1990s grid-based RPGs such as Lands of Lore, but it must use original code, names, maps, UI, and art direction.

## Current stack
- C++17
- raylib
- CMake
- Windows-first
- GitHub Actions CI

## Build and validation
Configure and build with:

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
```

The expected executable for Visual Studio and other multi-configuration generators is:

```text
build/bin/Release/stoneveil.exe
```

For single-configuration Release generators such as Ninja, it is:

```text
build/bin/stoneveil.exe
```

Before finishing any code task:
1. Build the Release target.
2. Fix all compile errors and warnings that indicate correctness problems.
3. Keep the GitHub Actions Windows build green.
4. Do not remove the playtest artifact packaging step.

## Architecture rules
- Keep dungeon simulation, rendering, party/roster, combat, inventory, save/load, and UI logically separated as the codebase grows.
- Render 1–3 active characters dynamically. Maximum active party size is 3; additional recruits remain in Reserve. The 2026-09-18 Creator Alpha brief supersedes the earlier six-slot direction.
- Prefer data-driven definitions for maps, enemies, items, recruits, and character stats.
- Keep the dungeon editor integrated into the main executable; do not ship it as a second program.
- Level width and height are data-driven within the supported 4–64 cell range. Do not restore fixed 16×16 runtime storage.
- Keep gameplay systems independent of final art assets.
- Preserve deterministic grid coordinates: the logical player tile is integer `(px, py)` while the raycast eye position is the center of that tile, `(px + 0.5, py + 0.5)`.
- Camera, collision, interaction, enemy positions, and map tiles must use one consistent coordinate convention.

## Current gameplay direction
- A new game begins with three available starter characters.
- The player may choose 1, 2, or all 3 starters.
- Maximum active party capacity is 3. Recruitment expands the reserve roster, not active capacity.
- Character death is intended to be permanent.
- More recruitable characters are found during the game and enter the reserve roster before party assignment.
- Keep recruitment and roster state persistent in saves.
- Combat should remain real-time/cooldown-driven rather than fully turn-based unless explicitly changed by the user.
- Prioritize the story-first creator pipeline and self-hosted test game; defer final artwork and content quantity.

## Visual direction
- First-person dungeon viewport with a classic 1990s PC dungeon-crawler feel.
- Pixel-art presentation, not glossy painterly concept-art rendering.
- Dark stone, aged metal, muted brass, deep crimson, slate/teal accents.
- Lands-of-Lore-like information density is acceptable as inspiration, but do not copy its exact UI, assets, map layouts, characters, or art.
- The large "STONEVEIL" logo should not sit over the gameplay HUD.

## Known critical rendering bug already fixed
The first playtest rendered the camera from the corner of the player's tile. This caused one side of the viewport to intersect nearby walls. The renderer was corrected so the DDA raycaster uses the center of the player's cell (`px + 0.5`, `py + 0.5`). Do not regress this.

## Current priorities
Read `docs/CODEX_HANDOFF.md` before making major changes. Preserve the existing playable prototype while evolving it toward the real game architecture.
