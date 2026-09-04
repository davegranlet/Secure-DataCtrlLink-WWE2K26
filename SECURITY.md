# Security design

> **Readability note:** I ran this document through an “explain like I am five”
> chatbot to improve readability, explainability, and usability. The chatbot
> helped present the material; it did not originate DataCtrlLink, its
> functionality, or the underlying development work.

## What was removed

The developer supplied their own already-working DataCtrlLink plugin and asked
for help securing their own code. It delivered its intended addon behavior
in-game before AI tools were involved. The later security audit found that its
general plugin loader also enumerated compatible `.dlp` and `.asi` files as
native code. That made file placement equivalent to granting code execution
inside the game process.

The secure replacement preserves the intended custom-music function, removes
the general native-code loading surface, and adds narrowly scoped CAK mounting
requested during the redesign. This is an audit-driven redesign of a working
human-created addon, not an AI-originated addon concept.
It is also not a rewrite of Tribute, PWM, or CakeHook source: no source code
from those projects was available or copied.

The rebuilt addon performs only two fixed operations: validated custom-music loading and validated `.cak` mounting.

## Defensive controls

- **Exact host allowlist:** mod behavior runs only in `WWE2K26_x64.exe` with the supported SHA-256.
- **Absolute DirectInput forwarding:** the real `dinput8.dll` is loaded from Windows System32 rather than the game directory or search path.
- **No arbitrary native extensions:** `.dlp`, `.asi`, and unrelated DLL files are never enumerated or executed.
- **CAK structural validation:** the encrypted catalog header is decoded and checked for the expected `FDIR` magic/version, bounded counts, consistent table sizes, contiguous table offsets, and archive bounds.
- **Restricted CAK discovery:** only regular, non-reparse `.cak` files directly inside `<game>\mods` are considered; at most 32 archives are processed.
- **Restricted package discovery:** only regular, non-reparse `sound\Custom*.pck` files are considered; at most 16 packages are processed and each must pass AKPK table validation.
- **Bounded explicit ordering:** an optional manifest accepts only direct-child `.cak` and `Custom*.pck` names, rejects traversal, duplicates, unknown fields, and excessive entries, and fails closed when invalid.
- **Bounded runtime configuration:** optional numeric settings are individually capped; unknown, malformed, duplicate, or out-of-range values restore the complete safe default configuration.
- **Size limits:** Wwise inputs are capped at 64 MiB and CAKs at 8 GiB before parsing.
- **Narrow runtime hook:** the CAK mount address and nearby machine-code signature are checked against the supported executable. The hook triggers once on the game's known archive-mount thread and is then removed.
- **No loader work under `DllMain`:** initialization runs on a worker thread after DLL attachment.
- **Fail-closed behavior:** mismatched game builds, malformed inputs, missing exports, and readiness timeouts produce log errors instead of falling through to unverified addresses.

## Remaining risks

This is still native code injected into a game process. A defect can crash the game, and a future game update can invalidate internal addresses. The exact executable hash check is the primary guard against that failure mode.

Structural validation establishes that a CAK has a plausible catalog; it does not prove the semantic safety of every asset in the archive. A successful internal mount return also does not prove that file overrides became effective. Only use CAKs from sources you trust, prefer a game-proven baker, and require an in-game content check before declaring success. Likewise, a valid Wwise package may still contain undesirable content even though it is not executable native code.

The addon is intended for the owner's local, offline modding setup. It is not an anti-cheat bypass and should not be represented as compatible with competitive or protected online play.

## Updating for a new game build

Do not merely replace the executable hash. Reconfirm the Wwise export ordinals, CAK mount routine, signature, ABI, native-thread trigger, and full live test evidence before adding a new supported hash.
