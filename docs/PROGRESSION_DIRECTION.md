# STONEVEIL — Progression, Classes, and Skills

> **Status: direction, not a spec.** Nothing here is approved. Do not implement from this document
> without Jules choosing the shape first. It also makes two assumptions the combat document left
> open — front/back rank exists, and the Arcanist's verb is a resource-costed cast. If either is
> ruled differently, the trees below change substantially.

Read `COMBAT_DIRECTION.md` first. This document is downstream of it.

## What exists today

- `CharacterDefinition` carries `maxHp`, `power`, and nothing else mechanical.
- `CharacterRecord` carries `hp`, `xp`, `status`. **XP accumulates and is never spent.** There are no
  levels, no points, no unlocks.
- Kills grant a flat +25 XP to every living party member equally, so there is no "who improves"
  decision.
- Three characters exist: Vanguard (Guardian, 42/9), Ranger (Pathfinder, 32/7), Mystic (Arcanist,
  27/6 — "fragile future spell specialist", with no spells behind it).
- `Party::increaseCapacity` already exists and already persists in save version 4. It is the one piece
  of permanent progression the codebase has.

## The problem a skill tree has to solve here, before it can be fun

**Permanent death and deep per-character investment fight each other.** If ten hours of build
decisions live inside the Vanguard and the Vanguard dies for good, the honest player reaction is not
drama — it is closing the game. Every crawler that has shipped permadeath alongside meaningful
character growth has needed an answer to this, and picking the answer first determines the entire
shape of the trees.

Three tracks, deliberately separate, because they carry different risk:

| Track | Scope | Survives death? | What it is for |
| --- | --- | --- | --- |
| **Character level** | One character | No | The thing you lose. Makes death cost something specific. |
| **Discipline mastery** | One class, all time | Yes | The thing you keep. Knowledge outlives the person. |
| **House upgrades** | The whole campaign | Yes | Recoverability. Capacity, revival, the stash. |

The middle track is the load-bearing one. When your Guardian dies, you lose *that* Guardian — their
levels, their spent points, their specific build. You do not lose your investment in *playing*
Guardian: the next one you recruit starts at the mastery floor you have earned. The sting stays, the
quit-moment goes.

If you reject that framing, the alternatives are: make characters cheap and interchangeable (death
stops mattering), or make death recoverable at steep cost (death becomes a resource tax). Both are
coherent. Neither preserves the pillar as well.

## The rule every node has to pass

A node must do one of three things:

1. **Add a verb** — something the player can now choose to do.
2. **Change a rhythm** — when acting is correct.
3. **Change where you stand** — make a cell better or worse to occupy.

Anything that is a flat percentage is filler. It makes the character sheet longer and the fight
identical. See "Rejected patterns" at the end.

## Class styles

Three disciplines, keeping the role names already authored. Each one owns a **distinct verb**, a
**distinct rhythm**, and a **distinct preferred rank**. Three verbs is the direct answer to the
one-button loop.

| Discipline | Verb | Rhythm | Rank | Fantasy |
| --- | --- | --- | --- | --- |
| **Guardian** | **Brace** — held stance | Slow, heavy (~0.9 s) | Front | Controls space and attention |
| **Pathfinder** | **Step-Strike** — attack that moves | Fast, light (~0.35 s) | Flexible | Controls tempo and information |
| **Arcanist** | **Cast** — form + modifier, costs Focus | Slow, committed, interruptible | Back | Spends safety for power |

### Guardian — the anchor

**Brace** is held, not tapped. While braced: melee attacks against the party redirect to the Guardian
and land reduced, but the Guardian cannot attack and drains stamina. It is the first mechanic in the
game that gives you a reason *not* to attack.

### Pathfinder — the tempo

**Step-Strike** is an attack that also moves you one cell sideways, on one cooldown. The sidestep
dance from `COMBAT_DIRECTION.md` becomes this class's native language rather than a universal trick.

### Arcanist — the gamble

**Cast** has a wind-up during which the Arcanist cannot act and can be interrupted — taking damage
mid-cast loses the spell *and* the Focus. Back rank exists so that this is survivable; a dead Guardian
means the Arcanist stops functioning, which is how you make losing your front line feel structural
instead of arithmetic.

**Focus** regenerates slowly, and faster while standing in a lit cell. That gives the lighting system a
second job and makes torch placement a resource decision rather than decoration.

## Levels and points

- Levels 1–10 across the campaign. Level 2 at 100 XP, each level ≈ ×1.5 from there, level 10 ≈ 3,800.
  At the current 25 XP per kill that is roughly 150 kills, or about ten Gatehouse-sized levels.
- **One skill point per level: 9 points by level 10.** Deliberately scarce.
- A tier-N node requires N−1 points already spent in that branch.
- Tier 4 is a **capstone and mutually exclusive** — taking one locks the other two capstones for that
  character, permanently.

Nine points across twelve nodes with tier gating means one capstone plus most of a second branch.
Two Guardians in the same six-person party can be genuinely different people, and the Guardian you
recruit to replace a dead one does not have to be a copy.

**Change XP from flat-to-everyone to participation-weighted** — a character earns full XP for acting
in the fight and a reduced share for being present. Otherwise the reserve roster free-rides and there
is no cost to benching someone.

---

## GUARDIAN

### Branch A — Bulwark (survive the hit)

1. **Set Stance** — Brace damage reduction 30% → 45%.
2. **Second Wind** — the first time you drop below 25% HP in a dungeon, recover 15 HP. Once per run.
3. **Immovable** — enemies that strike you while braced lose their next move step and recoil.
4. **CAPSTONE — Stone Vigil** — Brace costs no stamina while any ally is below half HP.

### Branch B — Warden (protect others)

1. **Interpose** — Brace also covers the back rank against ranged attacks.
2. **Shield Wall** — the other front-rank ally gains your Brace reduction at half value.
3. **Rally** — releasing Brace after absorbing 3+ hits clears one ally's attack cooldown.
4. **CAPSTONE — Oathbound** — when an ally would take lethal damage, you take it instead at 50%.
   Once per dungeon.

Oathbound is the anti-permadeath capstone and the strongest argument for fielding a Guardian at all.
It should be exactly one save, loudly telegraphed, and unavailable again until you leave.

### Branch C — Breaker (punish)

1. **Heavy Swing** — +50% damage against an enemy in a wind-up state.
2. **Stagger** — landing a hit on a winding-up enemy cancels the wind-up entirely.
3. **Wall Slam** — enemies you hit with a wall directly behind them take bonus damage.
4. **CAPSTONE — Executioner** — a killing blow resets your attack cooldown.

Wall Slam makes map geometry a damage modifier, which quietly turns the level editor into a combat
design tool. Fighting in a corridor and fighting in a room stop being the same fight.

---

## PATHFINDER

### Branch A — Skirmisher (the dance)

1. **Footwork** — Step-Strike cooldown −20%.
2. **Flanker** — +100% damage attacking an enemy from its flank or rear.
3. **Disengage** — Step-Strike can move you away from an adjacent enemy without provoking it.
4. **CAPSTONE — Perfect Circle** — consecutive Step-Strikes that land from a *different* facing than
   the last escalate damage, up to ×3. Break the circle and it resets.

Perfect Circle is the clearest expression of the whole combat thesis: the reward is for moving, and
mashing from one cell is strictly the worst thing you can do.

### Branch B — Hunter (range)

1. **Loose** — a ranged attack up to 3 cells along your facing, on a slow cooldown.
2. **Steady** — ranged attacks from the back rank gain +1 range and +25% damage.
3. **Pin** — a ranged hit prevents the target's next move step.
4. **CAPSTONE — Killing Ground** — ranged attacks against enemies that have not yet noticed the party
   are automatic criticals.

Killing Ground only pays if enemies have an unaware state, which means aggro has to be a real thing
the player can read and manipulate. Worth building for its own sake.

### Branch C — Scout (information)

1. **Dark-Adapted** — you can read enemy wind-up tells in unlit cells.
2. **Keen Eye** — secret doors are outlined in the viewport within 3 cells.
3. **Waymark** — drop a temporary light in your current cell, short cooldown.
4. **CAPSTONE — Forewarned** — enemies within 6 cells appear on the HUD through walls. The party
   cannot be ambushed.

Dark-Adapted is deliberately an *alternative* to carrying light, not an addition to it. See
"Overlapping answers" below.

---

## ARCANIST

Spells are **Form + Modifier**, not a list. Five forms and five modifiers give 25 meaningful spells
from ten authored rows, and the tree unlocks forms and modifiers independently.

**Forms** — what it does:

| Form | Effect |
| --- | --- |
| **Ember** | Direct damage to the front cell |
| **Sever** | Cancels an enemy wind-up and denies its next attack |
| **Mend** | Heals the lowest-HP member |
| **Ward** | Absorbs the next N damage to one member |
| **Lantern** | Places a real light in the target cell for ~20 s |

**Modifiers** — how it is shaped:

| Modifier | Effect |
| --- | --- |
| **Reach** | Becomes a line out to 3 cells |
| **Spread** | Also affects cells adjacent to the target |
| **Linger** | Persists in the cell over time |
| **Swift** | Half cast time, half effect |
| **Deep** | Double cast time, double effect |

Lantern + Linger is a placed torch you did not have to author into the level. Lantern + Reach lights a
distant cell so you can read a telegraph before you close. Sever + Swift is a panic interrupt. Ember +
Linger + a corridor is a killzone. The combinations do the content work.

### Branch A — Kindler (damage)

1. **Ember** — unlocks the base damage form.
2. **Reach** — unlocks the modifier.
3. **Spread** — unlocks the modifier.
4. **CAPSTONE — Conflagration** — Linger-modified damage forms ignite the cell for 6 s, damaging
   anything standing in it.

### Branch B — Weaver (control)

1. **Sever** — unlocks the interrupt form.
2. **Swift** — unlocks the modifier.
3. **Deep** — unlocks the modifier.
4. **CAPSTONE — Unravel** — Sever also strips the target's facing for 3 s; it attacks in a random
   direction and can be walked around freely.

### Branch C — Anchor (sustain and light)

1. **Mend** — unlocks the healing form.
2. **Lantern** — unlocks the light form.
3. **Ward** — unlocks the absorb form.
4. **CAPSTONE — Hearthfire** — cells lit by your Lantern restore Focus and slowly heal the party
   standing in them. The Arcanist becomes a mobile safe zone you have to decide when to plant.

---

## Overlapping answers are the point

A class system is working when two classes can solve the same problem at different costs, so party
composition is a real choice rather than a checklist:

- **Interrupting a wind-up** — Guardian's Stagger (melee, reactive, free) or Arcanist's Sever (ranged,
  proactive, costs Focus and cast time).
- **Fighting in the dark** — Pathfinder's Dark-Adapted (permanent, passive, one character only) or
  Arcanist's Lantern (costs Focus, helps everyone, also feeds Focus regen back).
- **Surviving a heavy blow** — Guardian's Brace (positional, continuous) or Arcanist's Ward
  (pre-cast, targeted, one hit).

If you cut the Arcanist from a run, you need a Pathfinder who went Scout. That is party composition
mattering without anyone writing "required class" anywhere.

## Discipline mastery — what survives death

Earned from the total XP every character of that discipline has ever accumulated, dead ones included.

| Mastery | Unlock |
| --- | --- |
| 1 | New characters of this discipline start at level 2 |
| 2 | +1 starting skill point |
| 3 | A new recruit archetype of that discipline becomes findable |
| 4 | New characters start at level 3 with +2 points |

Cheap to store — a per-discipline XP total — and it is the entire reason a player keeps going after
losing someone they liked.

## House upgrades — what makes a run recoverable

Earned on **extraction**, not on kills: you bank the currency only by leaving the dungeon alive. That
turns retreat into a decision with stakes rather than an admission of failure, and it is the cleanest
answer to the "is combat avoidable" question in `COMBAT_DIRECTION.md`.

- **Capacity** — 3 → 4 → 5 → 6. The code already supports this via `Party::increaseCapacity`.
- **Sanctum** — the party heals between expeditions instead of hoarding draughts.
- **Memorial** — a dead character's spent skill points return once, as a grant to a new recruit of any
  discipline. Death still costs the person; it stops costing the whole afternoon.
- **Stash** — items persist across runs.
- **Cartographer** — retain explored map knowledge for a level you left alive.

Note that the extraction loop is a genuine genre commitment. It makes STONEVEIL an expedition game
rather than a linear dungeon crawl. Worth deciding deliberately rather than drifting into.

## Data model

Same catalog pattern as `materialCatalog` / `lightCatalog`, all raylib-free so it lands in the test
target (see `ARCHITECTURE.md`):

```
struct SkillDefinition {
    std::string id;          // "skill.guardian.bulwark.set-stance"
    std::string discipline;  // "guardian"
    std::string branch;      // "bulwark"
    int tier;                // 1-4
    bool capstone;
    std::string name, summary;
};

struct SpellFormDefinition {
    std::string id;          // "spell.form.ember"
    int focusCost; float castSeconds; int magnitude; EffectKind kind;
};

struct SpellModifierDefinition {
    std::string id;          // "spell.modifier.deep"
    float costScale, castScale, magnitudeScale; ShapeChange shape;
};
```

A known spell is a `(form id, modifier id or none)` pair. `CharacterRecord` gains `level` and a
`vector<std::string> spentSkills`. Save format goes to **version 5**: per-record level and spent
skills, plus a global block for discipline mastery totals and house upgrades. Version 4 still loads,
with everyone at level 1 and no points spent.

## Build order

Do not build the whole tree. The tiers are worthless until the loop underneath them is fun.

1. **Per-character cooldowns and front/back rank.** Prerequisite for everything above; nothing here
   works while the party is one damage number.
2. **`SkillDefinition` catalog, XP → level → point, tiers 1 and 2 only.** Six nodes per discipline.
3. **Forms Ember and Mend, modifiers Swift and Deep.** Four spells. Proves the form × modifier model
   before anyone authors twenty rows.
4. **Save version 5.**
5. Stop. Play it. Only then decide whether tiers 3–4, mastery and house upgrades are the right shape.

## Rejected patterns

Written down so they do not creep back in under deadline pressure:

- **Flat +X% stat nodes.** They lengthen the sheet and change nothing about how a fight is played.
- **Nodes that strictly dominate the tier below.** If tier 3 is just tier 1 with a bigger number, the
  tree is a queue, not a choice.
- **Long authored spell lists.** Form × modifier produces more variety from less content and stays
  data-driven.
- **Deep 40-node trees.** With permanent death, depth converts loss into a quit trigger.
- **Anything that needs final art to read.** Every mechanic above has to be legible with placeholder
  swatches, per the standing constraint.
- **Required classes.** If a party without an Arcanist cannot function, the roster is a checklist.
