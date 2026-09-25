# STONEVEIL — Combat Direction

> **Status: direction, not a spec.** Nothing in this file is approved. It is a set of options with
> tradeoffs, written to be argued with. Do not implement from it without Jules picking the option
> first. The open questions at the end are open.

## Where combat stands today

From `src/CombatSystem.cpp`, exactly as built:

- One player verb. `SPACE` → `partyAttack`, on a flat **0.55 s** party-wide cooldown.
- Damage is `sum(power of living members) / 2 + rand(0, 5)`, minimum 1. The party is a single damage
  number; six characters and one character differ only in that figure.
- Attacks reach the **front cell only** (`frontEnemyIndex(..., 1)`), and facing already matters for
  the player.
- Enemies attack at **Manhattan distance 1 from any side**, every 1.25 s, for `rand(3, 8)`, against a
  uniformly random living member. They have no facing.
- Enemies chase greedily on the longer axis, one cell per 0.65 s, inside an aggro radius of 7.
- `Enemy` carries `x, y, hp, alive, attackCooldown`. No type, no power, no speed, no behavior.
- The only other verb is `H` — a healing draught, targeting the lowest-HP-fraction member.

## The diagnosis

The loop is: walk into contact, hold `SPACE`, drink at low HP. That is not a content shortage. It is
that there is **one verb and never a reason not to use it**. No positioning pays, no timing pays, no
resource is spent, no information is hidden.

Every idea below is judged by one test: *does it create a reason to not attack this instant?* Ideas
that fail that test add numbers, not interest.

## Pillars this has to respect

From `AGENTS.md` and the handoff, treated as fixed unless Jules says otherwise:

- Real-time / cooldown-driven, **not** turn-based.
- Character death is permanent.
- Data-driven definitions; authored files store stable IDs.
- The integer-cell coordinate convention and the centered eye.
- Systems stay independent of final art.

## The levers, ordered by leverage per line of code

### 1. Give enemies a facing — the grid becomes the combat system

The bones of the Dungeon Master / Eye of the Beholder sidestep dance are already here: 90° turning,
strafing, per-enemy positions, per-enemy cooldowns. The missing piece is that enemies have no facing,
so there is no such thing as a flank.

Add `direction` to `Enemy`, let it attack only into its front cell, and charge it time to turn. The
dance appears: hit, strafe to its flank, it spends its turn-time rotating, hit again. Add a flank or
rear damage bonus and the dance *pays* rather than only avoiding damage.

Cheapest change on this list by a wide margin, and it changes the feel more than any amount of enemy
content. It also makes corridors, corners and doorways tactically different from open rooms, which
makes the level editor a combat-design tool rather than a map-drawing tool.

### 2. Telegraph the danger

`rand(3, 8)` on a uniformly random living member, every 1.25 s, is not danger — it is noise. A 27 HP
Mystic can die with no warning and no counterplay.

Give heavy attacks a **wind-up**: an enemy enters a visible committed state for ~0.5–0.8 s before it
lands. The player can strafe out of the arc, turn and interrupt, or choose to eat it and keep hitting.
That single mechanic converts a dice roll into a decision, and it is what makes "dangerous" mean *you
saw it coming and chose wrong* rather than *the RNG picked your Mystic*.

This is also the prerequisite for raising difficulty at all — see the tension below.

### 3. Stop treating the party as one blob

Two changes, both of which make **composition** the engine of variety. That matters because
composition scales without authoring: enemy content has to be built one at a time, party interactions
multiply on their own.

- **Per-character attack cooldowns** instead of one party cooldown. Vanguard slow and heavy, Ranger
  fast and light. The rhythm of a fight becomes a texture rather than a metronome, and it gives the
  HUD something to show.
- **Front rank / back rank.** Only the first two party slots are reachable by melee. Then party order
  is a real decision, the Mystic has a reason to exist before spells land, enemy targeting stops being
  uniform random, and losing the Vanguard is a structural collapse rather than −9 power. That is
  permadeath biting the way it is supposed to.

Front/back rank commits to a camp/roster screen where order is editable — which the handoff already
names as the next party-facing feature, so the timing lines up.

### 4. Make enemy variety behavioral, not statistical

Follow the pattern the codebase already uses twice (`materialCatalog`, `lightCatalog`): an
`EnemyDefinition` catalog with stable IDs — hp, power, attack interval, move interval, reach, turn
time, behavior — with levels referencing the ID. It drops straight into the Object layer that is
already next for the editor.

Three archetypes that demand **different answers** beat twelve stat blocks:

- **Rusher** — closes fast, dies fast, punishes you for being mid-cooldown.
- **Breaker** — slow, long wind-up, enormous damage. The fight is about reading the tell.
- **Spitter** — refuses to step adjacent, attacks at range 2–3. Forces you to close through damage,
  which makes the other two dangerous as escorts.

A fight is interesting when two archetypes are present and the right answer to one is the wrong answer
to the other.

### 5. Make the lighting load-bearing

The lighting system exists now and is currently decorative. If the wind-up tell only reads clearly in
a lit cell, then fighting in an unlit corridor means trading blind — the dance stops working and you
are back to mashing, by choice, under pressure.

That makes torches a tactical resource, gives the editor's Lights layer real gameplay weight, and
hands you a difficulty dial that costs nothing to author. Pulling a fight toward or away from a sconce
becomes a level-design decision.

## The tension worth resolving before tuning anything

**Permanent death + high danger + `F5`/`F9` = savescumming, not tension.** Right now the quicksave key
is the real difficulty setting. Three coherent ways out, and they are mutually exclusive enough that
picking one early saves rework:

1. **Danger is legible.** Every death is preceded by a tell the player could have read. Keeps
   quicksave harmless, because reloading feels like admitting a mistake rather than rerolling dice.
2. **Combat is escapable.** Breaking contact, shutting a door on a pursuer, or routing around a room
   turns a losing fight into a retreat. Attrition and retreat is better drama than wipe-and-reload,
   and it lets the danger dial go much higher.
3. **Saving is constrained.** Save points, or no mid-dungeon save. The most old-school option and the
   most likely to annoy.

(1) and (2) compose well. (3) is a separate decision about what kind of player you want.

## Open questions

- **Is combat avoidable?** This is the biggest fork. It sets the ceiling on how dangerous anything is
  allowed to be, and it decides whether enemies need disengage/leash behavior at all.
- **Does party order mean something?** Front/back rank is the highest-leverage single change for
  making composition interesting, but it commits to the roster screen and to rank-aware targeting.
- **What does the Mystic actually do?** Currently a worse Vanguard with a promise attached. Whatever
  answers this probably defines the second verb, and the second verb is what ends the one-button loop.
- **Is there a resource besides HP?** Nothing currently makes attacking cost anything, which is why
  mashing is always correct.

## Explicitly not decided

Spell systems, equipment, inventory, status effects, enemy factions, and anything that would require
final art. None of the above needs them, and all of them can hang off the `EnemyDefinition` /
per-character-cooldown scaffolding later.
