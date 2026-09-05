**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Secure DataCtrlLink v1.7.7 bugs and fixes

## SDCL-177-001 — Generic native addon discovery

- **Bug:** The v1.7.6 modular prototype loaded every `.ftrib` file found in `plugins`.
- **Risk:** Renaming arbitrary native code to `.ftrib` granted it execution inside the game process.
- **Fix:** v1.7.7 recognizes only explicit addon filenames with compiled SHA-256 values. It rejects unknown names, altered binaries, directories, reparse points, missing exports, and failed initialization with specific log reasons.
- **Verification:** Registry tests reject legacy generic names, unknown names, and suffix-confusion names.

## SDCL-177-002 — Users could not toggle addons without moving files

- **Bug:** Presence in the folder was the only enable/disable mechanism.
- **Fix:** Optional `plugins/addons.txt` now supplies an explicit enabled set and initialization order.
- **Verification:** Selection tests cover absent configuration, preserved ordering, unknown names, paths, and case-insensitive duplicates.

## SDCL-177-003 — Official addon hashes changed between identical builds

- **Bug:** Windows linker timestamps made otherwise identical addon builds hash differently.
- **Impact:** A compiled hash registry would require needless updates and could not be reproduced independently.
- **Fix:** Native DLL and addon links now use the LLVM-MinGW reproducible PE timestamp option.
- **Verification:** Two consecutive full builds produced identical SHA-256 values for every named addon.

