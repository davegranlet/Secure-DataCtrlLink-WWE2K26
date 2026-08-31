# Secure DataCtrlLink for WWE 2K26

Secure DataCtrlLink is a small, open-source Windows addon that gives a specifically supported WWE 2K26 build two modding features:

1. mount validated `.cak` archives from the game’s `mods` folder;
2. register validated `sound/Custom*.pck` packages and load the reconstructed custom-music event bank.

It deliberately does **not** load arbitrary `.asi`, `.dlp`, or DLL plugins.

> New to all of this? Read **[Explain Like I’m Five](docs/EXPLAIN-LIKE-IM-FIVE.md)** first.

## Supported game build

The current release activates mod features only when all of these match:

- host: `WWE2K26_x64.exe`;
- executable SHA-256: `7F558607BD6881C4547D752EA107FB03282A2980E47EF19ADE597E096DE4A0E9`;
- tested Steam build ID: `24193653`.

DirectInput forwarding still uses the real DLL from Windows System32 if the game check fails. Mod loading fails closed and the reason is recorded in `DataCtrlLink-MusicOnly.log`.

## Install

1. Close WWE 2K26.
2. Back up any existing `dinput8.dll` in the WWE 2K26 game folder.
3. Download the newest release ZIP and verify its published SHA-256 checksum.
4. Copy `dinput8.dll` into the game folder beside `WWE2K26_x64.exe`.
5. Put mod archives directly in `<game>/mods` and custom packages directly in `<game>/sound`.
6. Launch the game offline and verify the intended content in-game.

At startup, the addon reports a count such as:

```text
Registered 5 CAK archives.
Verify mod content in-game.
```

The count means the game’s internal mount call accepted those archives. It does not prove that every internal path overrides the intended asset; only an in-game content check proves that.

## How CAK replacement works

A mod CAK supplies files at virtual paths. If a mounted mod contains the exact path the game requests, its file can replace the original:

```text
Original virtual path: characters/example/face.dds
Mod virtual path:      characters/example/face.dds
Result:                the game receives the modded file
```

WWE 2K26 v9.9 archive keys are derived from the CAK filename. Do not rename a completed CAK; rebuild it under the new final name.

## Rollback

Close the game, remove this project’s `dinput8.dll`, and restore your previous file. Removing the proxy disables both features and does not alter the original game archives.

## Build from source

Requirements:

- Windows x64;
- PowerShell;
- LLVM-MinGW with `clang++.exe` and `clang.exe` on `PATH`, or `LLVM_MINGW_ROOT` pointing to its installation folder.

```powershell
git clone https://github.com/davegranlet/Secure-DataCtrlLink-WWE2K26.git
Set-Location Secure-DataCtrlLink-WWE2K26
& .\build.ps1
```

Artifacts are written to `build`. See [Testing](TESTING.md) for the public test layers and [Architecture](docs/ARCHITECTURE.md) for the important code.

## Independent implementation and provenance

This implementation was independently reconstructed for interoperability from observable behavior, controlled testing on software owned by the developer, and documentation/source code from openly available tools. The code in this repository was written for this project. It does not contain proprietary WWE/2K source code, game assets, game executables, Oodle binaries, third-party addon binaries, decompiler databases, or captured game data.

The repository includes MinHook under its own 2-clause BSD license. See [Methodology and provenance](docs/METHODOLOGY-AND-PROVENANCE.md) and [Third-party notices](THIRD_PARTY_NOTICES.md).

This factual provenance statement is not legal advice. Users and contributors are responsible for following the laws, licenses, and platform terms that apply to them.

## Project status and safety

This is a version-locked community modding addon, not an anti-cheat bypass. It is intended for local/offline modding and is not represented as compatible with protected or competitive online play. A game update may require a reviewed new build.

Released under the [MIT License](LICENSE). This independent project is not affiliated with or endorsed by WWE, 2K, Take-Two, or Visual Concepts.

