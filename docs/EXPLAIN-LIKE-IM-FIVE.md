**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Explain Like I’m Five: how Secure DataCtrlLink works

Imagine WWE 2K26 is a big library.

The game’s pictures, models, settings, and other files are stored in large books called **CAK archives**. A mod CAK is a small extra book containing replacement pages.

Secure DataCtrlLink is the librarian who tells the game:

```text
“There are extra mod books in the mods folder. Please put them on the shelf too.”
```

## Why it is named `dinput8.dll`

The game normally asks Windows for DirectInput, which helps with controllers. Windows allows the game to find a file named `dinput8.dll` beside the game first.

Secure DataCtrlLink stands in that doorway, but it does not pretend to implement Microsoft’s controller system. It forwards the six expected calls to the real Windows copy using its full System32 path:

```text
Game → Secure DataCtrlLink → real Windows DirectInput
```

While it is there, it performs its two tightly limited mod jobs.

## Job one: mount CAK mods

The addon looks only at regular files ending in `.cak` directly inside the game’s `mods` folder. Before asking the game to mount one, it checks that the archive has the expected WWE 2K26 header, sensible counts, table sizes, offsets, and bounds.

It then waits until the game finishes its normal archive phase and uses the supported game build’s own mount function.

A mod replaces an original file by using the exact same virtual address:

```text
Game asks for:  some/directory/1.dds
Mod contains:   some/directory/1.dds
Game can use:   the modded 1.dds
```

If the address is wrong—even by one folder—the archive may mount successfully while nothing visible changes.

## Job two: custom music

Custom music is kept in Wwise package files. The addon accepts only regular files matching `sound/Custom*.pck`, checks that their AKPK tables stay inside the file, and registers them through the game’s own audio system.

It extracts the expected stock music bank, adds a fixed set of normal and looping custom music event records, and asks the game to load the rebuilt bank from memory. It never overwrites the original bank on disk.

## Why it checks the whole game file

The private game functions used for mounting and audio can move whenever WWE 2K26 updates. Calling yesterday’s address in today’s game could crash it.

Secure DataCtrlLink therefore calculates the SHA-256 fingerprint of the entire game executable. If it is not the exact supported build, mod behavior stops:

```text
Expected fingerprint?  Yes → continue with signature checks
Expected fingerprint?  No  → log the mismatch and do not touch game internals
```

Updating support means rechecking the addresses, nearby instructions, function calling rules, timing, and real in-game results—not simply changing the allowed fingerprint.

## Why this is safer than a general plugin loader

A native `.asi`, `.dlp`, or DLL plugin is executable code. A general loader that runs every matching file is effectively saying:

```text
“If your filename has the right ending, you may do anything inside the game process.”
```

Secure DataCtrlLink contains no such scanner. It recognizes data archives and audio packages under narrow rules; it does not execute arbitrary addon files.

## What the startup message really means

`Registered 5 CAK archives` means five archives passed this addon’s structural checks and the game’s mount call returned success.

It does not mean every file inside them is correct. You must still inspect the intended texture, value, character, or other change in-game.

## The whole thing in one sentence

Secure DataCtrlLink safely passes controller calls to Windows, checks that it is inside the one game build it understands, validates mod data, and asks the game’s own systems to mount CAKs and register custom music—without opening a door for arbitrary native plugins.
