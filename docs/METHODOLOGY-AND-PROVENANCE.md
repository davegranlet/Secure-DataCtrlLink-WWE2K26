# Methodology and provenance

## What this repository is

This repository is an independent interoperability implementation of two narrowly scoped addon behaviors: WWE 2K26 CAK mounting and the intended custom-music registration/bank path.

The implementation was produced through controlled local testing, observation of runtime inputs/outputs and file formats, comparison against known-good behavior, and use of publicly available/open-source development and analysis tools. The developer used software installed on and controlled by their own computer.

## What is original here

The C++ proxy, validators, bank reconstruction, status reporting, tests, build automation, and documentation in this repository were written for this project. No original proprietary source code for WWE 2K26 or the prior closed addon was available to or copied into this repository.

The public source expresses the minimum behaviors required for interoperability and adds a new fail-closed security design: exact host allowlisting, bounded data validation, absolute System32 forwarding, a one-shot signature-checked hook, and removal of generic plugin execution.

## What is intentionally not published

The repository and release do not include WWE/2K source code or extracted game assets, the game executable or game-owned DLLs, Epic’s proprietary Oodle libraries, third-party addon binaries, decompiler databases, memory captures, runtime dumps, captured music packages/banks, test CAKs, personal filesystem paths, or rollback copies.

Those materials are neither needed to understand the public source nor licensed as part of this project.

## Publicly available dependencies

MinHook is the only source dependency vendored into this repository. It is a public open-source project and retains its 2-clause BSD license. Windows cryptographic and DirectInput facilities are called through documented platform APIs.

## Reproducibility

Anyone can inspect the complete addon source, build it with LLVM-MinGW, run the source-level tests, inspect the produced imports/exports, and compare the release DLL checksum with a local build. Tests that require real WWE data intentionally require the tester to provide files from their own installation; fixtures containing proprietary game data are not published.

## Important qualification

This document records the project’s development provenance; it is not a legal opinion or a promise about every jurisdiction, license, or platform term. Contributors and users must make their own compliance decisions and should obtain professional advice when needed.

