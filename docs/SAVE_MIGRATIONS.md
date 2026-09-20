# Persistence compatibility — Creator integration

## Authored levels

Level format is version 10. Readers accept versions 1–10. Version 7 adds a stable
numeric character reference to world-object records so recruit interactions do
not depend on display names or list order. Version 8 adds arrival facing and
stable destination-level/arrival IDs for campaign transitions. Older objects
load with empty transition data. Version 9 adds optional required-true and
set-true story flags to authored triggers. Older triggers remain unconditional
and gain no consequence flag when loaded. Version 10 lets consequences write
true or false, adds required/hidden flags to world objects, and adds a story-unlock
flag to doors and gates. Empty fields preserve the old behavior. Older levels gain
deterministic pickup/enemy/door IDs in memory and upgrade when saved. Map row
shape is now checked before legacy ID migration. Compiled door event IDs must
not collide with authored trigger IDs.

## Player saves

Project character definitions live in the v1 `.svc` catalog and are loaded
before the player save. Save v5 continues to store only mutable CharacterRecord
state by stable numeric ID. Loading rejects missing IDs or HP above the current
definition maximum without mutating the active game. Adding an unrecruited
definition is compatible with an older save; removing or renumbering a character
referenced by a save requires a future explicit project migration.

Current writer: `STONEVEIL_SAVE 7`. Readers: pre-versioned, 2, 3, 4, 5, 6, 7.

Version 7 adds `STORY_FLAGS`, a bounded collection of stable named boolean facts.
Facts are campaign-wide and survive level transitions and reloads; level-local
one-shot history remains in each level's event fired-counts.

Version 6 adds `CAMPAIGN_STATES`: one stable-ID snapshot for every visited,
inactive level. Each snapshot stores mutable tiles, pickups, enemies, and fired
event counts. The active `LEVEL` remains in the main save block. Load first
reads that active level ID so the game can instantiate the correct registered
authored level before applying state. Roster records, permanent deaths, reserve
recruits, active party, inventory, and XP remain campaign-wide.

Version 5 adds quoted stable IDs before each pickup/enemy record and an `EVENTS`
section before `END`. Each event entry is a quoted ID and positive fired-count.
Entries are written sorted by ID. Runtime one-shot state therefore survives a
save/reload. Authored list reordering no longer changes enemy archetypes or
reattaches pickup/enemy state to the wrong identity.

The level must contain the same pickup/enemy identities and the saved event IDs
must still exist. Missing, duplicate, or changed required identities reject the
load without modifying the running game. Cross-version content migration after
deleting or replacing those identities is not yet supported. The level ID and
dimensions must also match. Save v6 exposes the active level ID first, allowing
the game to open the correct registered authored level before restoring it; the
title-screen level selection no longer needs to match.

Versions 3–4 have no entity IDs; their records necessarily use authored list
order. Keep old content order when loading those saves. Pre-versioned/version 2
saves never recorded world tile/pickup changes, so those changes cannot be
recovered. Older formats also never recorded event history; events begin unfired
on migration. No loader can infer whether old dialogue has already been read.

The new brief fixes active-party capacity at 3. Version 4 capacities of 4–6
migrate to 3. If a future compatible catalog contains an old party larger than
three, the first three saved members remain Active and the rest become Reserve;
HP, XP, and death are not reset. Current shipped catalogs only contain three
characters, so larger *membership* fixtures are not yet possible without the
authored Character Creator. Version 5 rejects capacity above 3.

Transient HUD message queues are not saved. Event side effects and fired-state
are saved; loading does not replay already-fired speech. Combat cooldown/RNG
serialization is still incomplete and remains an explicit Gate 7 defect.

## Common event contract

`EventRuntime` is shared by the PR's `Authoring` compilers, existing story-trigger
compatibility facade, and the `WorldEvents` gameplay adapter. IDs are explicit
and unique. Priority is descending; ties retain authored order. Conditions and
one-shot state are evaluated immediately before execution. An event is marked
before executing actions, preventing one-shot recursion. Nested signals share a
4096-unit event/action budget and maximum depth 64. A limit produces a diagnostic,
not a crash. Definition/state mutation is rejected during dispatch; fact reads
must be pure. Callback exceptions unwind dispatch depth, but already-executed
side effects are not rolled back.

Existing `.svl` triggers compile to message actions; existing door metadata
compiles through `DoorBehavior`. Door reactions and key consumption now use that
runtime. The richer PR `StoryTileBehavior`/`DoorBehavior` compiler API is retained
and tested, but advanced speech speaker/voice, trap, cutscene, fact-editing and
music fields still need authored format, contextual editor, and full runtime
adapters. They are not claimed as creator-facing features yet. Unsupported
adapter actions report a message rather than pretending to run.

## Regression evidence

Release-active suites cover level versions 1–10, save layouts pre-versioned/2–7,
stable-ID reorder restoration, death, reserve swaps, non-healing duplicate
recruitment, cap enforcement, door key consumption, one-shot restoration,
duplicate/derived-ID collisions, malformed old maps, callback exceptions,
priority ordering, and nested/branching dispatch limits.
