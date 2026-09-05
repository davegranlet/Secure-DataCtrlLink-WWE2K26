**Readability note:** I ran this document through an “explain like I am five” chatbot to improve readability, explainability, and usability. The chatbot helped present the material; it did not originate Aurora Forge, DataCtrlLink, their functionality, or the underlying development work.

# Methodology and provenance

## Development timeline

DataCtrlLink began as a human-created addon. Before any AI-assisted review, its
developer had already:

- decided what the addon was intended to do;
- designed and implemented a working plugin;
- tested it in WWE 2K26; and
- established the custom-music behavior and broader development plan.

The request that started this project was to help the developer **secure their
own existing code**. It was not a request to reproduce Tribute, PWM, CakeHook,
or another developer's source. AI tools were introduced only after the
developer asked for an adversarial security audit of their working build. The
audit identified an overly broad native-plugin loading surface and reliability
risks around version-specific game calls. The decision was then made to publish
a narrower replacement that preserved the developer's intended custom-music
function and added separately requested, narrowly scoped CAK mounting without
unrestricted plugin execution.

The developer remained the project owner and test authority: they chose the
requirements, supplied the locally owned software and test files, directed the
investigation, performed the in-game navigation, judged visible and audible
results, and approved design changes. AI tools assisted with analysis,
implementation, refactoring, test creation, and documentation for the secure
replacement.

## The developer's own compiled plugin

The original audit examined the developer's compiled DataCtrlLink files because
the developer intentionally supplied the same compiled artifacts an ordinary
user or attacker would receive. Giving the audit the original source would have
defeated the purpose of testing how well the distributed build protected its
implementation. Recovering behavior from that binary can technically be called
binary analysis or reverse engineering, but it was analysis of the developer's
own work. It was not appropriation of a third party's source or idea.

The original addon, its purpose, and its working custom-music behavior all
predated the AI-assisted audit. The replacement is entitled to preserve those
developer-owned requirements while changing their implementation and security
model.

## Third-party interoperability research

No source code from Tribute, Pro Wrestling Mods, CakeHook, or any other third-
party addon was available, copied, translated, or included in this project.
Tribute/PWM source was not used as an implementation base.

During later interoperability research, compiled game behavior and a distributed
CakeHook binary were inspected as behavioral references. Work included
examining imports, exports, strings, call shapes, file structures, and
observable inputs and outputs. That research helped identify how independently
created software could communicate with the game. It did not provide or copy
CakeHook/PWM source code, and no decompiled third-party function was pasted or
translated into the public implementation.

Tests were run on software and files installed on and controlled by the
developer. The public implementation was written separately to satisfy the
developer's requirements and the observed game interfaces. It is described as
an independent interoperability implementation, not as a formal clean-room
process.

Similarity in functional result does not establish copying. Under United States
copyright law, copyright protection does not extend to an idea, procedure,
process, system, or method of operation, although particular source-code
expression can be protected. See
[17 U.S.C. § 102(b)](https://uscode.house.gov/view.xhtml?req=%28title%3A17+section%3A102%28b%29+edition%3Aprelim%29).
This factual statement does not resolve every possible license, contract,
patent, trademark, or jurisdiction-specific question.

## What this repository contains

This repository publishes the later security-focused replacement, not the
original pre-audit source tree. Its C++ proxy, validators, bank reconstruction,
status reporting, tests, build automation, and documentation were assembled for
this public project.

The implementation limits itself to two addon behaviors: WWE 2K26 CAK mounting
and the custom-music registration/bank path. It adds exact host allowlisting,
bounded data validation, absolute System32 forwarding, a one-shot signature-
checked hook, explicit mount ordering, bounded configuration, and removal of
generic plugin execution.

## Role of AI tools

AI did not originate DataCtrlLink, its purpose, or the fact that its core
features worked. The developer asked AI to help secure their own existing
plugin. AI assistance began with that security review and continued during the
replacement’s implementation and publication.

The plain-language tutorial was produced by running the developer’s technical
work and the finished implementation through an “explain like I’m five” style
of explanation, followed by review against the actual code. Technical security
documentation remains separate because it records real implementation
requirements rather than merely simplifying them for beginners.

## What is intentionally not published

The repository and release do not include WWE/2K source code or extracted game
assets, the game executable or game-owned DLLs, Epic’s proprietary Oodle
libraries, third-party addon binaries, decompiler databases, memory captures,
runtime dumps, captured music packages/banks, test CAKs, personal filesystem
paths, or rollback copies.

Those materials are neither needed to understand the public source nor licensed
as part of this project.

## Publicly available dependencies

MinHook is the only source dependency vendored into this repository. It is a
public open-source project and retains its 2-clause BSD license. Windows
cryptographic and DirectInput facilities are called through documented platform
APIs.

## Reproducibility

Anyone can inspect the complete published source and build it with LLVM-MinGW.
The automated tests then execute compiled test programs, and release checks
inspect the compiled DLL, its imports/exports, and its checksum. Source review
is useful, but it is not treated as proof that the distributed binary behaves
correctly. Tests that require real WWE data ask the tester to provide files
from their own installation; fixtures containing proprietary game data are not
published.

## Important qualification

This document records the project’s development provenance. It is not a legal
opinion or a promise about every jurisdiction, license, or platform term.
Contributors and users must make their own compliance decisions and should
obtain professional advice when needed.
