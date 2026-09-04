# Testing and release evidence

> **Readability note:** I ran this document through an “explain like I am five”
> chatbot to improve readability, explainability, and usability. The chatbot
> helped present the material; it did not originate DataCtrlLink, its
> functionality, or the underlying development work.

The public repository contains test source but deliberately contains no
proprietary game fixtures. The test source is compiled before use, and the
checks run against compiled test programs and the compiled addon. Tests that
inspect real WWE archives or Wwise packages require files supplied from the
tester’s own installation.

## Build all targets

```powershell
& .\build.ps1
```

The build uses warnings-as-errors and produces the addon plus six test executables beneath `build`.

## Tests that require no game data

```powershell
& .\build\status_message_tests.exe
& .\build\mod_manifest_tests.exe
& .\build\runtime_config_tests.exe
& .\build\proxy_smoke.exe .\build\dinput8.dll
```

These verify notification wording, bounded manifest and configuration parsing,
all six proxy exports, and absolute forwarding to the System32 DirectInput
implementation. The smoke host is not WWE 2K26, so the addon's version gate
prevents any game-internal behavior.

## Custom-music reconstruction test

```powershell
& .\build\bank_builder_tests.exe <music.pck> <stock-bank.bnk> <known-good-rebuilt-bank.bnk>
```

This verifies known Wwise event hashes, AKPK parsing, extraction of the expected stock bank, byte equality with a locally held known-good reconstruction, and malformed-input rejection. The three data files are intentionally not distributed.

## CAK validation test

```powershell
& .\build\cak_validator_tests.exe <archive1.cak> [archive2.cak ...]
```

For every tester-supplied archive, this verifies encrypted-header recovery and structural bounds. It also creates temporary truncated/corrupted samples and proves they are rejected.

## Live offline acceptance test

Before a release is marked compatible:

1. Confirm the exact game executable SHA-256 equals the documented allowlist value.
2. Start with known trusted CAKs and a known custom package.
3. Confirm the log records successful CAK mount calls and Wwise registration/bank load.
4. Confirm at least one distinctive replacement from every release candidate in-game.
5. Confirm custom audio is actually audible where expected.
6. Exit cleanly and inspect the log for errors.

A successful mount return alone is insufficient. It proves the native function accepted the archive, not that every internal path or asset is correct.

## Known coverage limits

- No complete fuzzing campaign has been run against the AKPK or encrypted CAK validators.
- Internal game ABIs are verified only for the exact allowlisted executable.
- Audio content and game asset semantics require manual in-game observation.
- Conflicts and priority between multiple valid mods cannot be proven by structural validation.
