# STONEVEIL

A systems-first C++ dungeon crawler prototype inspired by the feel of early-1990s first-person party RPGs, with original code, layout, names, and placeholder visuals.

## Prototype goals

- Grid-based first-person exploration
- 90-degree turning
- Doors, keys, pickups, and collision
- Three-character party HUD
- Melee combat and enemy pursuit
- Healing consumables
- Save/load
- Data-oriented dungeon logic
- Minimal placeholder presentation so mechanics can be iterated first

## Tech

- C++17
- raylib 6.0
- CMake 3.24+

## Build on Windows

```powershell
git clone https://github.com/zdfr9pz2df-pixel/STONEVEIL.git
cd STONEVEIL
cmake -S . -B build
cmake --build build --config Release
.\build\Release\stoneveil.exe
```

Controls are listed in-game and in this README once the first playable slice lands.
