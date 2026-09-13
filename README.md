# STONEVEIL

STONEVEIL is a systems-first C++ dungeon crawler prototype inspired by the feel of early-1990s first-person party RPGs, with original code, layout, names, and placeholder visuals.

## v0.1 playable slice

- Grid-based first-person exploration
- 90-degree turning plus strafing
- Raycast dungeon rendering
- Collision
- Locked door + key interaction
- Three-character party HUD
- Real-time cooldown melee combat
- Enemy pursuit and attacks
- Healing draught pickups/consumption
- XP counter
- Prototype exit/victory condition
- Save/load foundation
- Windows CI build

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
.\build\bin\Release\stoneveil.exe
```

The first configure downloads raylib automatically.

## Controls

| Input | Action |
| --- | --- |
| W / S or Up / Down | Move forward/back |
| A / D | Strafe left/right |
| Q / E or Left / Right | Turn 90 degrees |
| Space | Party melee attack |
| F | Interact/open locked door |
| H | Use healing draught |
| F5 | Save |
| F9 | Load |
| Esc | Return to title |

## Current scope

Narrative and final art are intentionally deferred. The immediate goal is to prove the exploration/combat/inventory loop first, then separate additional systems into dedicated modules as the prototype grows.
