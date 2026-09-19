# STONEVEIL — Expedition Direction

Status: proposed direction distilled from `STONEVEIL-design-direction.md`. This does not override Jules's
future decisions; it records the useful design discoveries so engineering choices keep pointing at the
same game.

## North star

STONEVEIL should feel like a dangerous expedition. The protected emotion is the weight of deciding
whether to go one room farther when the way back is already long.

Tiebreaker:

> When two good mechanics conflict, favor the one that makes distance-from-safety more legible and
> more consequential.

## Decisions to protect

- Combat should be measured: telegraph, commit, recover.
- The three-character control scheme should not become fully manual real-time micromanagement without
  a deliberate sign-off.
- Positioning and party abilities should both matter; grid movement remains a real resource.
- Permanent death should leave recoverable corpses/equipment in the world, with decay pressure but no
  wipe-lock state.
- Rest should cost supplies/light/safety; wounds clear fully only at the surface.
- Floors should be interconnected, with shortcuts that visibly shorten the route back to safety.
- Interactivity should be a small verb set applied broadly: pry, break, burn, flood/drain, block,
  illuminate.
- Enemy rules should be readable before they are clever.
- Darkness should be mechanical: torches/fuel, stealth, detection, and sound.
- Water must be gameplay state, not only rendering state: depth per cell, flow direction, volume id,
  and connectivity hooks for later pathing/audio/light systems.
- Recruit differences should be capabilities, not just stat numbers.
- Audio is a systems feature, not polish: use it to communicate condition, danger, distance, and
  missing visual information.

## Engineering choices already made from this direction

- `AudioSystem` exists as a systems-facing presentation module.
- `LevelDefinition` and `Dungeon` now carry queryable `WaterCell` state: depth, flow, and volume id.
- `LevelIO` saves/loads water as authored data, while older levels load as dry.
- `LevelBlueprint` can import `WATER x y width height depth [flow] [volume]` commands for
  Codex-assisted level generation.

## Next safe implementation slices

1. Decide the control scheme before deep combat/UI tuning.
2. Add an Object/Entity layer with persistent state that pathfinding and AI can query.
3. Add authored shortcut/return-route metadata and tests for "route home got shorter."
4. Build a wipe-lock regression scenario: one death at depth, low supplies, weak light, still
   recoverable.
