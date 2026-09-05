**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Secure DataCtrlLink for WWE 2K26

Secure DataCtrlLink is the shared, fail-closed Windows loader for specifically supported WWE 2K26 builds. Features are supplied by separately named addons rather than being built into the loader.

Current approved addons are:

1. `AuroraForge.CAKModLoader.ftrib` — validates and mounts ordered `.cak` mods;
2. `AuroraForge.CustomMusicLoader.ftrib` — registers validated custom packages and rebuilds the custom-music event bank;
3. `AuroraForge.Example.MyGMStartingCash50M.ftrib` — clearly labeled example mod.

See [addon management](docs/ADDON-MANAGEMENT.md) for installation, enable/disable, ordering, and security behavior.

It deliberately does **not** load `.asi`, `.dlp`, arbitrary DLLs, or arbitrary `.ftrib` files. A native addon runs only when its exact filename and reproducible SHA-256 appear in the loader's compiled registry.

> New to all of this? Read **[Explain Like I’m Five](docs/EXPLAIN-LIKE-IM-FIVE.md)** first.

## Project history and AI assistance

> **Project origin:** The developer brought an already-working DataCtrlLink
> plugin that they had written and asked for help securing their own code.

DataCtrlLink was therefore a functioning, human-created addon before AI tools
were introduced. Its developer had designed its purpose, implemented the
original plugin, and proved the intended functionality in-game. AI entered the
project later, when the developer requested an adversarial security review of
that working build.

That review led to this separate, security-focused open-source replacement. The
developer supplied the requirements and test environment, directed the
experiments, verified every meaningful result in-game, and made the design and
release decisions. AI tools assisted with binary analysis, implementation,
refactoring, tests, and documentation. They did not originate DataCtrlLink or
its intended functionality.

This repository contains the later replacement source; it is not presented as
the original pre-audit source tree. The precise development and reverse-
engineering history is recorded in
[Development and provenance FAQ](docs/DEVELOPMENT-AND-PROVENANCE-FAQ.md) and
[Methodology and provenance](docs/METHODOLOGY-AND-PROVENANCE.md).

No source code from Tribute, Pro Wrestling Mods, CakeHook, or another third-
party addon was available, copied, translated, or included. Similarity of
purpose—such as mounting a CAK or registering custom music—is not a claim of
shared source authorship. This project implements those functions in its own
published code.

## Supported game build

The current release activates mod features only when the host is `WWE2K26_x64.exe` and its SHA-256 hash matches one of the explicitly reviewed builds:

- Original release: `dfb08c59f547ccd9c2a5820dd7520af064ca93d8642959b9f005e0c6f2958466`
- v1.14: `7f558607bd6881c4547d752ea107fb03282a2980e47ef19ade597e096de4a0e9`
- v1.16: `5f82c813e905ceb96053280cf3488f7f54bc35258c1a0eb027355d45f6c56da7`

DirectInput forwarding still uses the real DLL from Windows System32 if the game check fails. Mod loading fails closed and the reason is recorded in `DataCtrlLink.log`.

## Install

1. Close WWE 2K26.
2. Back up any existing `dinput8.dll` in the WWE 2K26 game folder.
3. Download the newest release ZIP and verify its published SHA-256 checksum.
4. Copy `dinput8.dll` into the game folder beside `WWE2K26_x64.exe`.
5. Copy only the clearly named approved addon files you want into `plugins` beside `dinput8.dll`.
6. Put mod archives directly in `<game>/mods` and custom packages directly in `<game>/sound`.
7. Launch the game offline and verify the intended content in-game.

At startup, the addon reports a count such as:

```text
Registered 5 CAK archives.
Verify mod content in-game.
```

The count means the game’s internal mount call accepted those archives. It does not prove that every internal path overrides the intended asset; only an in-game content check proves that.

## Optional mount order

Addon enable/disable and initialization order are controlled separately by
`plugins/addons.txt`. Within the CAK and Custom Music features, content order is
controlled by `mods/manifest.json`:

```json
{
  "version": 1,
  "cakOrder": ["base.cak", "zzz-fix.cak"],
  "packageOrder": ["Custom_Music.pck"]
}
```

The manifest is an allowlist. When present, files omitted from it are not
loaded. Invalid manifests fail closed instead of falling back to directory
scanning.

## Optional bounded configuration

`DataCtrlLink.ini` may be placed beside the game executable. It accepts only
these numeric settings and ranges:

```ini
MaxFileBytes=67108864        ; 1-64 MiB
MaxCustomPackages=16        ; 1-16
MaxCakArchives=32           ; 1-32
ReadyTimeoutMs=120000       ; 5000-300000
CakPostPrimeDelayMs=2500    ; 0-10000
```

An unknown, duplicate, non-numeric, or out-of-range setting causes the complete
file to be ignored and safe defaults to be used.

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

This implementation is a security-focused rewrite of the developer's own
working addon. Compatibility details were established through observable
behavior, controlled testing on software owned by the developer, analysis of
compiled programs, and documentation/source code from openly available tools.
No third-party proprietary source code was available or copied. The repository
does not contain WWE/2K source code, Tribute/PWM/CakeHook source code, game
assets, game executables, Oodle binaries, third-party addon binaries,
decompiler databases, or captured game data.

The repository includes MinHook under its own 2-clause BSD license. See the
[Development and provenance FAQ](docs/DEVELOPMENT-AND-PROVENANCE-FAQ.md),
[Methodology and provenance](docs/METHODOLOGY-AND-PROVENANCE.md), and
[Third-party notices](THIRD_PARTY_NOTICES.md).

This factual provenance statement is not legal advice. Users and contributors are responsible for following the laws, licenses, and platform terms that apply to them.

## Project status and safety

This is a version-locked community modding addon, not an anti-cheat bypass. It is intended for local/offline modding and is not represented as compatible with protected or competitive online play. A game update may require a reviewed new build.

Released under the [MIT License](LICENSE). This independent project is not affiliated with or endorsed by WWE, 2K, Take-Two, or Visual Concepts.
