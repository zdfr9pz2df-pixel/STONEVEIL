# STONEVEIL — Portrait Reaction States

> **Status: contract proposed, art not started.** The code in `include/Portrait.hpp` /
> `src/Portrait.cpp` is written and unit-verified but **not yet wired into the build** — see
> "Wiring it in" below. Nothing here authorises drawing frames or recording voice yet; the point of
> the document is to settle the contract so that when art does start, it gets drawn once.

## Why a contract before art

`AGENTS.md` says gameplay systems stay independent of final art, and `CODEX_HANDOFF.md` says not to
begin art before the content contracts are stable. Portrait frames for damage, talking, frustration
and looking, across three characters, is a large amount of drawing. If the reaction *states* are not
pinned down first, the wrong set gets drawn.

So this splits in two:

- **The system** — stable state IDs, a pure resolver, event hooks. Art-independent, testable, done.
- **The art and audio** — frames and voice lines that attach to those IDs by path. Later.

## How a state is chosen

Each character's portrait shows exactly one state, picked every frame by priority rather than by a
transition graph. A graph would be more code and harder to reason about at this scale.

- **Moods** are continuous and derived from condition: idle, peering, alert, wounded, critical, dead.
- **Reflexes** are brief, event-driven and interrupt the mood: hit, kill, ally-down, and so on.

Resolution order: death outranks everything; an unexpired reflex outranks any mood; otherwise the
highest-priority matching mood wins. A reflex cannot be interrupted by a lower-priority reflex, which
is why taking a hit cuts off a spoken line but a spoken line never cuts off a damage reaction.

## The states

| ID | Tier | Pri | Duration | Bark gap | Falls back to | Fires when |
| --- | --- | --- | --- | --- | --- | --- |
| `portrait.dead` | 0 | — | mood | — | — | `CharacterStatus::Dead` |
| `portrait.critical` | 0 | 60 | mood | 12 s | wounded | HP below 25% |
| `portrait.alert` | 0 | 50 | mood | 6 s | idle | An enemy is visible or adjacent |
| `portrait.wounded` | 0 | 40 | mood | — | idle | HP below 60% |
| `portrait.peering` | 2 | 30 | mood | 8 s | idle | Standing in an unlit cell, no enemy in view |
| `portrait.idle` | 0 | 10 | mood | — | — | Default |
| `portrait.ally-down` | 1 | 190 | 2.5 s | — | hit | Another party member died |
| `portrait.hurt-badly` | 2 | 170 | 1.0 s | 6 s | hit | A hit dropped this character below 25% |
| `portrait.spell-fizzle` | 2 | 150 | 1.0 s | 4 s | blocked | A cast was interrupted and the Focus lost |
| `portrait.hit` | 0 | 140 | 0.6 s | 1.5 s | idle | Took damage |
| `portrait.kill` | 1 | 130 | 0.8 s | 4 s | alert | This character's attack killed an enemy |
| `portrait.blocked` | 1 | 120 | 0.5 s | 5 s | alert | Movement blocked, or an attack hit nothing |
| `portrait.heal` | 2 | 115 | 0.8 s | 4 s | idle | Received healing |
| `portrait.pickup` | 2 | 110 | 0.8 s | 3 s | idle | Picked something up |
| `portrait.talking` | 1 | 105 | 1.4 s | — | idle | A dialogue line is playing |

`portrait.blocked` is the "frustration" state. `portrait.alert` and `portrait.peering` are the two
halves of "looking" — one is attention on a threat, the other is straining to see in the dark.

## Tiers, so the art can land incrementally

**Tier 0 is the shippable minimum: six states.** `dead`, `idle`, `alert`, `wounded`, `critical`,
`hit`. Every other state declares a `fallbackId`, and `resolvePortraitFallback` walks that chain until
it reaches a state the character actually has art for.

The consequence, which is the point of the whole design: **you can draw 6 states × 3 characters and
ship.** Tier 1 adds life, tier 2 adds polish, and neither requires a code change or leaves a character
rendering blank. Do not let tier 2 block the game.

## Frame contract

Settle these before the first frame is drawn; changing them later means redrawing everything.

- **32 × 32 pixels per frame.** Drawn at ×2 (64 px) in the standard 92 px party card, ×1 in the 48 px
  compact card. Both are integer scales, which matters because the raycaster already uses
  `TEXTURE_FILTER_POINT` and the art direction is pixel art, not painterly.
- **Horizontal strip PNG**, frames left to right, up to 8 frames.
- Moods **loop**; reflexes play **once** and hold the final frame until they expire. This is derivable
  from `category`, which is why there is no separate loop flag.
- Frame count and playback rate are **per character per state**, so they belong in a portrait manifest
  rather than in `PortraitStateDefinition` — the state catalog is character-agnostic on purpose.

**Paths**, following the convention the texture work established:

```
content/portraits/<stableKey>/<suffix>.png      content/portraits/starter.vanguard/hit.png
content/vo/<stableKey>/<suffix>/*.ogg           content/vo/starter.mystic/critical/01.ogg
```

`stableKey` is already on `CharacterDefinition` (`starter.vanguard`, `starter.ranger`,
`starter.mystic`), so nothing new needs authoring to address a character.

## The audio layer is not decoration

Voice lines key off the same state IDs, with a per-state cooldown (`barkCooldownSeconds`) so a
character reacting every frame does not chatter. `PortraitTrack::requestBark` gates this.

The reason this matters more here than in most crawlers: if darkness becomes the central pressure (see
`COMBAT_DIRECTION.md`), then in an unlit corridor **the portrait is the thing you cannot read**. The
bark still fires. Voice becomes the information channel exactly when vision fails — the Vanguard's
breathing tells you he is at critical when you cannot see his face.

That makes the audio layer load-bearing rather than flavour, and it is the one production task on this
project that plays to Jules's actual trade. If anything in this document gets built first, it should
be this.

## Integration points

Every reflex maps to a call site that already exists, except the two marked as waiting on systems that
have not been built:

| Reflex | Where it fires |
| --- | --- |
| `portrait.hit`, `portrait.hurt-badly` | `CombatSystem::updateEnemies`, at the `roster.damage(target, damage)` call |
| `portrait.ally-down` | Same site, on the branch where `roster.damage` returns true |
| `portrait.kill` | `CombatSystem::partyAttack`, where `enemy.hp <= 0` |
| `portrait.blocked` | `Game::move` ("Something blocks the way") and `partyAttack` ("cut empty air") |
| `portrait.heal` | `Game::drinkPotion`, after `roster.heal` succeeds |
| `portrait.pickup` | `Game::collectPickup` |
| `portrait.talking` | No call site yet — needs a dialogue or bark trigger |
| `portrait.spell-fizzle` | No call site yet — needs the Arcanist cast system |

Moods need no call sites; they read condition:

| Mood | Source |
| --- | --- |
| `dead`, `critical`, `wounded` | `CharacterRecord::hp` against `CharacterDefinition::maxHp`, and `status` |
| `alert` | `CombatSystem::frontEnemyIndex(dungeon, player, 6) >= 0` — already computed in `Game::drawWorld` |
| `peering` | `Lighting::sampleAt` at the player's cell, below a threshold |

One `PortraitTrack` lives per active party member in the presentation layer. It holds no authored data
and is never serialized — a portrait's animation state is not save data.

## HUD layout consequence

The party card is currently 272 × 92 (or × 48 compact) and holds name, HP text and a bar. A 64 px
portrait on the left pushes all three right by about 72 px. The compact card at 48 px tall fits a
32 px portrait with padding but will need the name and HP on one line. Worth mocking before drawing.

## Wiring it in

`Portrait.cpp` is written and verified but **deliberately not added to `CMakeLists.txt`**, because the
tree was being actively edited when it was written. Two lines, one in each target:

```cmake
add_executable(stoneveil
  ...
  src/Portrait.cpp
  ...
)

add_executable(stoneveil_core_tests
  ...
  src/Portrait.cpp
  ...
)
```

The verification block is scratch-only and should be appended to `tests/CoreTests.cpp` when that file
is free. It covers mood priority, reflex interruption and expiry, death outranking a running reflex,
fallback-chain resolution, and bark cooldown gating.

## Deliberately excluded

- **Per-character state machines.** All characters share one state list; personality comes from which
  frames and which voice lines are bound to the IDs, not from different logic.
- **Emotion as a continuous value.** A mood/priority split is legible and debuggable; a valence model
  is neither, at this scale.
- **Lip sync.** `portrait.talking` is a looping mouth-moving cycle, not phoneme matching.
- **Portrait state in saves.** It is derived from condition every frame by design.
