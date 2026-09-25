# Project workspace and Windows export

Open the integrated dungeon editor, then choose **Project** at the top.
The bundled STONEVEIL project lists its three registered levels automatically.

- **New Project:** create an empty folder in the Windows file picker, enter it,
  and choose a new .stoneveil filename. Starter textures/audio are copied in.
- **Open Project:** choose an existing .stoneveil manifest.
- **New / Save As:** create a level and save it inside the project's
  content/levels folder. Save automatically registers it; Save As makes a new ID.
- **Open Level:** open an existing .svl, or click a registered level in Project.
- **Set Start Level:** save the selected registered level first.
- **Export Windows Game:** save first, then select an existing empty folder.
  Distribute that entire folder, not just its stoneveil.exe.

Export includes the registered levels, referenced material textures, door art,
available level music, campaign registry, project manifest, and Windows executable.
It does not include engine source, build tools, unregistered levels, or saves.
The final stoneveil.game marker selects player-only mode: no editor button or
party debug shortcuts. The same executable provides both editor and runtime.
Release links the Microsoft runtime statically; only Windows system DLLs are
direct dependencies. Hardware still needs OpenGL 3.3 support.

Project-mode saves are stored in project-folder/saves/game.sav. That folder must
be writable. Projects currently have one save slot; install-folder-independent
user-profile saves remain future work.

Project format is STONEVEIL_PROJECT 1; the current level format is 10 and save format is 7.
Project paths are relative and constrained to the project root. Missing music or
material textures are warnings; invalid registries/levels are blocking errors.
Project open currently requires valid registered levels; fix malformed source
files before reopening. Full progression-graph validation is not implemented.

Each persisted file is written to an exclusive sibling temporary file, flushed,
and atomically replaced on Windows. This is per-file protection, not a multi-file
project transaction or a backup history. Interrupted create/export can leave a
partial folder; inspect it and choose another empty destination before retrying.
Export's playable marker is written last.

Developer checks:

    stoneveil.exe --validate-project path/to/game.stoneveil
    stoneveil.exe --project path/to/game.stoneveil
    stoneveil.exe --capture-ui capture-folder --project path/to/game.stoneveil

## Object inspection

Right-click an object on the map to open its inspector. Right-click again to
cycle objects sharing that cell (for example a note and a light). The top
**Inspect** button opens the player-spawn inspector.

Choose **Move**, then click a destination; Escape cancels the move. Escape again
closes the inspector. Invalid/occupied destinations leave the draft unchanged.
Doors move their structure marker with them; surfaces remain attached to cells.
Object identity and attached events survive movement. Delete is undoable but
refuses objects with attached events until those events are removed/retargeted.
Location-based EnterCell stories deliberately remain where authored.

Available properties are type-specific: spawn facing, door lock/story-unlock flag, pickup kind,
enemy archetype (resets authored HP), light type, and world-object movement
blocking/name/text/visibility flags. Other kinds have no authored facing field yet. Text editing
uses the existing Enter/Escape-to-finish behavior; Ctrl+Z undoes the edit after
finishing. Inspector mutations participate in document Undo/Redo.

## Character Creator

Open **Project → Characters**. Definitions are project data, separate from
mutable player-save records. Identity contains the player-visible name and
summary. Role contains broad role, traits, health, power, and the currently
supported basic ability choice. Recruitment controls 1–3 starting candidates,
reserve recruitability, and recruitment text. Advanced contains equipment
compatibility/starting-equipment IDs and the future portrait path contract.

Add generates a new non-starter character with a stable numeric ID and reference
key. Delete always retains at least one definition and one starting candidate.
Save validates the whole catalog before atomically replacing
`content/characters/characters.svc`; Revert reloads the last saved catalog.
Text entry ends with Enter or Escape. Catalog changes are used by the next
playtest/new game and are included in Windows export.

Current limits are deliberate and visible: melee is the only attack definition,
the basic heal/none toggle is the only ability choice, equipment strings are
authored metadata only, and portrait upload is not wired yet. The character list
scrolls when a project grows beyond nine definitions.

On the Object layer, **Recruit** places a non-starter recruitable character and
stores its stable character ID in the level. Right-click its map marker to cycle
the intended recruit. **Party Management** places a generic interaction point.
Recruitment adds the character to Reserve exactly once; it never heals or
duplicates an existing record. Interacting with a management point opens the
active/reserve screen, where up to three living characters can be active and a
larger roster remains available through a scrolling list. Health, XP, permanent
death, reserve state and the active party survive normal save/load.

ProjectTests covers two-level registration, start selection, identity preservation,
duplicate rejection, path traversal, missing-audio warnings, no-overwrite export,
and atomic replacement failures. Passing an executable path to that test retains
a real exported smoke project rather than deleting the unique test fixture.
