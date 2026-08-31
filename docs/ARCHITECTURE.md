# Architecture and important code map

This is the shortest route through the source for reviewers and maintainers.

## Runtime flow

```text
Windows loads dinput8.dll
  → DllMain stores the module handle and starts one worker
  → DirectInput exports resolve the absolute System32 DLL on first use
  → worker verifies host name + complete executable SHA-256
  → worker validates CAK and Custom*.pck inputs
  → one-shot, signature-checked archive-phase hook fires
  → hook returns immediately; worker mounts accepted CAKs
  → worker registers audio packages and loads rebuilt music bank
  → status notification and detailed log
```

## Important source files

### `src/proxy.cpp`

The security boundary and orchestration layer.

- `supported_game()` hashes the complete host and enforces the allowlist.
- `custom_packages()` restricts package discovery to regular, non-reparse `Custom*.pck` files.
- `mount_cak_archives()` checks code signatures, installs a one-shot MinHook detour, and invokes the native mount function only after the stock archive phase.
- `register_packages()` uses the supported build’s Wwise resolver.
- `load_real()` resolves the genuine Microsoft DirectInput DLL from System32.
- `music_worker_impl()` sequences all initialization outside `DllMain`.
- the six exported functions at the bottom forward to System32.

The RVAs, signatures, ordinals, and function-pointer ABIs are version-specific. Treat them as one reviewed compatibility profile; never update one in isolation.

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

