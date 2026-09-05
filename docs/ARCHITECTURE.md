**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Architecture and important code map

This is the shortest route through the source for reviewers and maintainers.

## Runtime flow

```text
Windows loads dinput8.dll
  → DllMain stores the module handle and starts one worker
  → DirectInput exports resolve the absolute System32 DLL on first use
  → worker verifies host name + complete executable SHA-256
  → loader reads the optional bounded plugins/addons.txt selection
  → loader checks exact addon filename + compiled SHA-256
  → only approved addons receive the reviewed game-build profile
  → each addon validates its own data and records detailed status
```

## Important source files

### `src/proxy.cpp`

The security boundary and orchestration layer.

- `sha256_hex()` hashes the complete host and every candidate approved addon.
- `load_plugins()` rejects unknown names, altered hashes, reparse points, missing exports, and failed initialization.
- `load_real()` resolves the genuine Microsoft DirectInput DLL from System32.
- `main_worker()` sequences verification and addon initialization outside `DllMain`.
- the six exported functions at the bottom forward to System32.

The RVAs, signatures, ordinals, and function-pointer ABIs are version-specific. Treat them as one reviewed compatibility profile; never update one in isolation.

### `src/core/addon_registry.*`

Defines the only native addon filenames and reproducible SHA-256 values accepted by this loader release.

### `src/core/addon_selection.*`

Parses the optional bounded `plugins/addons.txt` enable/order list. Unknown names, paths, duplicates, excessive lines, and reparse points fail closed.

### `src/cak_validator.cpp`

A bounded structural validator for encrypted FDIR v9.9 catalogs. It checks:

- file type and maximum archive size;
- `FDIR 09 09 00 81` prefix;
- recoverable encrypted header;
- bounded file/folder counts;
- consistent section sizes and contiguous offsets;
- section and payload bounds inside the archive.

It intentionally does not extract or execute content.

### `src/bank_builder.cpp`

Contains strict AKPK package-table validation, extraction of the expected stock music bank entry, and in-memory reconstruction of the HIRC hierarchy with fixed custom normal/looping sounds, actions, and events.

Every chunk, record, count, and offset is checked before use. The output is capped and the original file on disk is never modified.

### `src/status_message.cpp`

Pure formatting logic for accepted/rejected/failed CAK counts. Keeping it separate makes the user-visible singular/plural and warning behavior unit-testable.

### `src/mod_manifest.cpp`

Parses the optional, bounded `mods/manifest.json` allowlist. It rejects unknown
fields, duplicate names, path traversal, unsupported extensions, and excessive
entry counts while preserving the developer-selected mount order.

### `src/runtime_config.cpp`

Parses the optional `DataCtrlLink.ini`. Only five documented numeric settings
are accepted, each within a fixed range. Invalid or unknown settings restore
the complete safe default configuration.

### `exports.def`

Fixes the names and ordinals of the six DirectInput proxy exports expected by the host.

### `third_party/minhook/`

Unmodified MinHook source under its 2-clause BSD license. It is used only for the one-shot archive-phase detour.

## Threading rule

`DllMain` performs minimal loader-safe work. Filesystem access, hashing, validation, waits, hooks, Wwise calls, and notifications happen after attachment on worker threads. Do not move complex initialization into `DllMain`.

## Failure rule

Unsupported or ambiguous state must disable the mod feature and write a useful log entry. It must never fall through to guessed addresses. DirectInput forwarding is kept independent so an unsupported game hash does not require calling game internals.

## Updating for a new WWE 2K26 build

Reconfirm the complete executable hash, Wwise exports and ABIs, CAK mount RVA/ABI, archive-phase RVA and signatures, native timing, malformed-input rejection, and visible/audible in-game results. Changing only `kExpectedExeSha256` is unsafe and is not an acceptable compatibility update.
