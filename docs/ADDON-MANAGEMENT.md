**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Secure DataCtrlLink addon management

## Install an approved addon

Place the addon's clearly named `.ftrib` file in the game's `plugins` folder. Secure DataCtrlLink checks both its exact filename and SHA-256 against the registry compiled into that loader release.

Copying an unknown or modified native file into `plugins` does not approve it. The loader rejects it and records the reason in `DataCtrlLink.log`.

## Remove an addon

Delete only that addon's `.ftrib` file from `plugins`. The shared `dinput8.dll` loader and other addons can remain installed.

## Enable, disable, and order installed addons

Create `plugins/addons.txt`. Put one approved addon filename on each line, in the order it should initialize. Only listed addons are enabled.

```text
# Enabled addons, in load order
AuroraForge.CAKModLoader.ftrib
AuroraForge.CustomMusicLoader.ftrib
```

To disable an addon without moving it, remove its line or place `#` at the beginning of that line. An empty valid `addons.txt` disables all addons.

If `addons.txt` is absent, installed approved addons load in the official registry order. Unknown names, duplicate names, paths, more than 64 lines, or a reparse-point manifest cause all addon loading to stop for that run.

## Why users cannot approve arbitrary native files

An `.ftrib` addon is executable native code. Allowing any file—or trusting a user-editable hash list—would recreate the insecure generic plugin loader. New official addons and updated addon builds therefore require an intentional Secure DataCtrlLink registry update. Unsupported files fail closed.

## Current approved filenames

- `AuroraForge.CAKModLoader.ftrib`
- `AuroraForge.CustomMusicLoader.ftrib`
- `AuroraForge.Example.MyGMStartingCash50M.ftrib`

