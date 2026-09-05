**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Third-party notices

## MinHook

This project includes MinHook from:

```text
https://github.com/TsudaKageyu/minhook
```

MinHook is distributed under the 2-clause BSD license. The upstream license text is retained at `third_party\minhook\LICENSE.txt`.

The runtime uses MinHook only for the narrow, signature-checked, one-shot CAK mount detour described in `SECURITY.md`.

## Platform and game-owned components

The addon calls documented Windows APIs for DirectInput forwarding, SHA-256 hashing, filesystem access, logging, and notifications. Microsoft platform components are not redistributed in this repository.

WWE 2K26, its assets, executable, Wwise data, and game-owned libraries are not distributed with this project. WWE, 2K, Take-Two, Visual Concepts, Audiokinetic/Wwise, Epic/Oodle, and related names and marks belong to their respective owners.
