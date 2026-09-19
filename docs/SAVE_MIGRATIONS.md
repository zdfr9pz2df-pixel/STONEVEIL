# Persistence compatibility — Creator integration

## Authored levels

Level format remains version 6. Readers accept versions 1–6. Older levels gain
deterministic pickup/enemy/door IDs in memory and upgrade when saved. Map row
shape is now checked before legacy ID migration. Compiled door event IDs must
not collide with authored trigger IDs.

## Player saves

Current writer: `STONEVEIL_SAVE 5`. Readers: pre-versioned, 2, 3, 4, 5.

Version 5 adds quoted stable IDs before each pickup/enemy record and an `EVENTS`
section before `END`. Each event entry is a quoted ID and positive fired-count.
Entries are written sorted by ID. Runtime one-shot state therefore survives a
save/reload. Authored list reordering no longer changes enemy archetypes or
reattaches pickup/enemy state to the wrong identity.

The level must contain the same pickup/enemy identities and the saved event IDs
must still exist. Missing, duplicate, or changed required identities reject the
load without modifying the running game. Cross-version content migration after
deleting or replacing those identities is not yet supported. The level ID and
dimensions must also match. Title-level selection must match the save's level.

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

Release-active suites cover level versions 1–6, save layouts pre-versioned/2–5,
stable-ID reorder restoration, death, reserve swaps, non-healing duplicate
recruitment, cap enforcement, door key consumption, one-shot restoration,
duplicate/derived-ID collisions, malformed old maps, callback exceptions,
priority ordering, and nested/branching dispatch limits.
