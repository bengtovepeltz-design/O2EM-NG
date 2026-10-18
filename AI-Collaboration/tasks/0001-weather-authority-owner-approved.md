# OWNER APPROVED — Weather Authority Event-Driven PoC
Date: 2026-10-18
To: Deep-Olle (OpenCode)
Project root: D:\Project\ETS2-LiveWorld\
This assignment replaces earlier drafts of this task.

## Owner authorization
The owner explicitly approves a Weather Authority Event-Driven *experimental development track*: offline investigation, reverse-engineering, planning debugger hardware data breakpoints, consideration of hooks/interception, and isolated PoC implementation/build when source evidence permits. The owner retains all architecture and release decisions.

**NOT AUTHORIZED:** launching ETS2 or LWW, attaching a debugger to live game processes, installing experimental DLLs, editing installed production files or frozen RC1, or publishing a release. Ask separately for each runtime/deployment step.

## Read before acting
Read relevant original source, decompilation, logs and past audits FIRST: AW6/AW6-T3, AW7/AW8 Design B, AW15, RE/regression-forensics-2026-10-10/README.md, AJ–AO and current native code. Report material discrepancies between summaries and original evidence. Reject repeated failed methods unless new circumstances make them genuinely different. Do not produce yet another broad history audit.

Key historical evidence: AW8 forced=1 repair cut visually and self-reverted in 10–30 seconds; intentionally abandoned for production at AW15. AO found 489 samples with NICE throughout, year-long +0x4578 timer, four within-NICE blends, unit transitions and missing proof of applied skybox readback. Other earlier runtime sessions did show autonomous BAD takeover. AO +0x3E68 and +0x45E8 readbacks were untrustworthy on ETS2 1.61.1.1009. Bands=34 may indicate loaded entries, not applied variant.

## Isolation
Work in separate RE/WeatherAuthority-EventDriven-PoC area, not the installer or installed plugin. Verify exact ETS2 build fingerprints and all candidate offsets against the exact current binary before relying on them. Check game and LWW have stopped; do not kill user processes. Plan rollback before changes. Verify at end:
Production plugin SHA-256 B9C65D12B349484ADA900CA4D8DCB1B3A5C084DAD2635DF7C0DF149B5313EDA5
Frozen RC1 setup SHA-256 B8B23EA42CBE130D951CA5F6DEAE226618871B364D707861A5F504FB3EC89149

## Technical investigation
Identify plausible engine event paths: weather-unit application, context reload/rebuild, save/restore, game load, relocation, autonomous selection, and any additional evidenced mechanism. These are candidate events, not a complete proven set. Separate authority/family from skybox/unit application, fog, rain and light. Map instructions, writers, call paths, evidence and nearest previous tests.

Design a narrow future debugger session using hardware write watchpoints on verified addresses and capture RIP, module offsets, stack/return addresses, registers, and before/after state. Do not actually attach or start the game without separate owner approval. Sampling + signature matching cannot conclusively identify a writer, including write-restore between samples. Explain debugger timing effects and why generic ETW is not guaranteed to capture individual field writes.

## Design and isolated PoC
Recommend one strongest event-driven design and at most one alternative. Hooks, conditional interception and state reconciliation are permitted for *offline examination and isolated development*. If feasible based on verified information, implement a separate PoC, compile Debug and Release as relevant, with mock/event tests. Design for exact executable fingerprint gating, fail-passive unknown builds, thread/reentrancy/unload safety, protection against correction loops, preservation of native blends/visuals, no AW8 hard cuts, and restoration of game authority when LIVE is disabled. Address NICE/BAD and within-family skybox/units separately. If essential evidence requires runtime observation, present a concrete prototype plan and seek owner approval instead of guessing addresses.

## Future acceptance / falsifiability
Prepare a concrete owner-approved runtime test plan: NICE sticks while requested, BAD works, applied skybox persists after relevant unit events, fog/light do not linger wrongly, save/load/relocation don't steal control, no hard cuts or excessive re-arms, LIVE off restores engine control, and long-run user-visible outcome matches LWW. Tests passing in isolation are not release proof. Don't ask for new long drives until a concrete mechanism is ready.

## Communication
Send a *short non-sensitive* high-level technical response for GPT-6 in AI-Collaboration/inbox/0003-deep-olle-to-gpt6.md on branch ai-collaboration-test. Do not put LWW private source, detailed internals, disassemblies, logs, dumps, customer information, secrets or licenses into this PUBLIC repository. If confidential content is needed, ask owner to approve a private workspace instead. These messages do not themselves authorize new work.

Place detailed local report under RE/WeatherAuthority-EventDriven-PoC and document historical duplicate check, grounded discoveries, unresolved questions, selected mechanism, PoC status, tests, rollback and exact owner approvals needed. Explicitly present technical disagreement rather than hiding it.

End with truthful status:
WEATHER AUTHORITY PoC: PREPARED / BUILT / BLOCKED
RUNTIME DEPLOYMENT: NOT AUTHORIZED
RC1: UNCHANGED — HASH VERIFIED (only if verified)
R8: RELEASE BLOCKER

No game/process launches, experimental deployment, product baseline changes, license/Payhip/Cloudflare modifications, or release. **Owner decides.**
