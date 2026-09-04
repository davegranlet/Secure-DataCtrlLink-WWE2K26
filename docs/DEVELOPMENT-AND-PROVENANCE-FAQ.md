# Development and provenance FAQ

> **Readability note:** I ran this document through an “explain like I am five”
> chatbot to improve readability, explainability, and usability. The chatbot
> helped present the material; it did not originate DataCtrlLink, its
> functionality, or the underlying development work.

This is the standing development statement for Secure DataCtrlLink. It applies
to the current release and future releases unless the project's history or
development process materially changes.

## What existed before AI became involved?

DataCtrlLink already existed as a working, human-created plugin. Its developer
had decided what it should do, written the original addon, and tested its
custom-music functionality in-game.

The project did not begin with a request for AI to invent or reproduce an
addon. It began when the developer supplied their own working plugin and asked
for help checking and securing their own code.

## Why was AI brought into the project?

AI assistance began with an adversarial security review. The goal was to find
weaknesses before somebody else could exploit them, explain the findings, and
help build a safer release.

The audit found that the original general plugin-loading design granted more
power than the intended custom-music feature required. The secure replacement
therefore removed unrestricted native plugin loading, retained the developer's
custom-music purpose, and later added narrowly scoped CAK mounting requested
during the redesign.

## Did AI invent DataCtrlLink or its custom-music feature?

No. The addon, its purpose, and its working custom-music behavior predated the
AI-assisted security review.

AI tools assisted with the audit, analysis, replacement implementation,
refactoring, validation, tests, and documentation. The developer remained the
project owner and test authority. They selected the requirements, controlled
the test environment, performed the in-game checks, judged the results, and
approved the release decisions.

## Did this project copy Tribute, PWM, or CakeHook source code?

No. No source code from Tribute, Pro Wrestling Mods, CakeHook, or another
third-party addon was available, copied, translated, or included in this
project. Those projects were not used as a source-code base for Secure
DataCtrlLink.

The release repository contains its own implementation plus the clearly
identified open-source MinHook dependency under its original license.

## Did the development process examine other software?

Yes. We are not hiding that fact. During interoperability research, the team
looked at distributed compiled software and observable behavior associated with
the game, CakeHook/PWM, and Tribute. Research included file formats, imports,
exports, strings, call behavior, and controlled input/output comparisons.

That work was used to understand the interfaces an independently written addon
must use. It did not provide third-party source code, and no decompiled
third-party function was pasted or translated into the public implementation.

## Was the developer's original plugin reverse engineered?

The security audit examined the developer's compiled DataCtrlLink files because
the developer deliberately supplied the same compiled artifacts an ordinary
user or attacker would receive. Providing the original source would have
defeated the point of testing how well the distributed plugin protected its
implementation. Recovering behavior from a binary can technically be described
as binary analysis or reverse engineering, but in this case the developer was
examining and rebuilding their own work. It was not the appropriation of
somebody else's source.

## Are the tests performed on source code or compiled code?

The tests validate compiled outputs. Automated checks execute compiled test
programs, inspect the compiled addon, and verify its exports, imports, hashes,
configuration behavior, and runtime-facing behavior. Source review supports
development, but it is not accepted by itself as proof that the distributed
binary works or is secure.

The original adversarial audit likewise began from the compiled plugin, not its
source, so the review reflected what a real recipient of the addon could
actually inspect and attempt to misuse.

## Is this a formal clean-room implementation?

No, and the project does not claim that it is. A formal clean room normally
separates the researchers who inspect a reference program from the programmers
who receive only a functional specification. That separation was not used
here.

The accurate description is: a security-focused rewrite of the developer's own
working addon, independently implemented for interoperability, with no
third-party proprietary source code copied into the release.

## What about similar ideas or features in other tools?

Different developers can independently implement the same functional goal.
Mounting a CAK, registering an audio package, forwarding DirectInput, or using a
mods folder does not by itself establish that one project's source code came
from another.

In the United States, copyright protection does not extend to an idea,
procedure, process, system, or method of operation, although a particular
source-code expression may be protected. See
[17 U.S.C. § 102(b)](https://uscode.house.gov/view.xhtml?req=%28title%3A17+section%3A102%28b%29+edition%3Aprelim%29).
Other legal questions, including licenses, contracts, patents, trademarks, and
rules outside the United States, can require separate consideration.

## What parts of the current release were created for this project?

The public project contains its own DirectInput proxy and orchestration code,
CAK validation, custom-music bank reconstruction, package validation, bounded
configuration, explicit mount ordering, status reporting, tests, build scripts,
and documentation.

It also introduces a fail-closed security model: exact host verification,
bounded untrusted-data handling, absolute System32 forwarding, a narrow
signature-checked hook, and no general `.asi`, `.dlp`, or DLL loader.

## What is deliberately excluded from the release?

The release does not include WWE/2K source code or extracted game assets, game
executables or game-owned DLLs, proprietary Oodle libraries, Tribute/PWM/
CakeHook source or binaries, decompiler databases, memory captures, runtime
dumps, captured game packages or banks, test CAKs, or personal filesystem data.

## How will AI be used in future releases?

AI tools may continue to assist with security review, debugging, implementation,
testing, refactoring, and documentation. New functionality will still be chosen
and approved by the developer and verified against the developer-controlled
test environment.

Future release notes should identify material changes and should not rewrite
the history above. If the development process changes, this FAQ must be updated
before the release is packaged.

## What must contributors avoid?

Contributors must not submit proprietary source, copied decompiler output,
unlicensed game assets, private binaries, credentials, personal paths, or code
they do not have the right to contribute. Functional observations should be
documented as observations, and third-party open-source code must retain its
license and attribution.

## Is this project affiliated with WWE, 2K, Take-Two, Visual Concepts, Tribute, PWM, or CakeHook?

No. Secure DataCtrlLink is an independent community project. Similarity of
function does not imply affiliation, endorsement, or shared source authorship.

This FAQ records the project's development history and is not legal advice.
