# OWNER DECISION 0002 — One controlled runtime debugger session (rev 2)

**Date:** 2026-10-18
**Owner decision:** APPROVED, with explicit limits below
**To:** Deep-Olle / OpenCode
**Project:** D:\Project\ETS2-LiveWorld\
**Reference:** Deep-Olle's LOCAL debugger procedure revision 2 described in AI-Collaboration/inbox/0005-deep-olle-to-gpt6.md, and GPT-6 review AI-Collaboration/outbox/0005-gpt6-to-deep-olle.md.

## Explicit approval

The project owner has expressly authorized **ONE controlled ETS2 runtime debugger session under the existing revision-2 procedure, lasting no more than 20 minutes.**

This approval includes starting ETS2/LWW for the specifically approved session if necessary, and attaching the debugger to the supported game process to use **hardware WRITE data breakpoints for observation**, provided all preconditions in revision 2 can be satisfied.

It does **not** authorize installing any experimental DLL, writing or patching engine memory, software breakpoints that patch code, changes to the installed production plugin, frozen RC1, mods, game binaries or release files, or any release/publish action. Do not treat this approval as authorization for follow-on debugger sessions, hooks, or deployments.

## Preflight

Read the full LOCAL revision-2 debugger procedure and relevant original technical sources first. Verify executable version and CodeView GUID, PID and start time, live manager pointer and selected/active field validity, memory mapping, alignment and the relevant threads. Do not reuse stale pointers/offsets. Exclude offsets already proven unreliable on build 1.61.1.1009. Confirm the existing installed plugin hash matches the verified production SHA-256 B9C65D12B349484ADA900CA4D8DCB1B3A5C084DAD2635DF7C0DF149B5313EDA5, and frozen RC1 installer hash B8B23EA42CBE130D951CA5F6DEAE226618871B364D707861A5F504FB3EC89149. Verify rollback and keep original files untouched.

If attaching safely requires owner interaction or would interrupt the owner's environment, coordinate first. Do not forcibly kill or manipulate unrelated processes.

## Observation

Watch validated selected-family and, where feasible, active-family writes. Record per hit: timestamp, thread coverage, RIP and module-relative offset, call/return stack, before/after value and relevant state; correlate to context/reload/relocation event, weather adoption and frame boundary. Clearly distinguish verified observations from guesses. Hardware breakpoints are generally thread-scoped: if coverage of relevant threads cannot be verified, do NOT infer absence of writes from missing hits.

Stop at the **first decisive captured sequence**, after at most **20 minutes**, or sooner on instability, hangs, invalid pointers, unverified safety preconditions or altered timing that makes results unusable. Detach cleanly and verify production artifacts are unchanged.

## Outcome classification

- No relevant engine event observed: **INCONCLUSIVE**.
- Relevant event but watchpoint coverage/address invalid: **INVALID OBSERVATION**, do not draw a negative conclusion.
- Relevant event plus verified writer coverage, no write: **reassess writer hypothesis**.
- Write captured: **document the instruction/call path and observed relative ordering**, identify what is still unknown regarding engine adoption/render, and whether the offline event-driven policy could plausibly win.

Do not automatically escalate to hooks, repeat the run or build another workaround. Submit specific recommended next step and risks for owner decision.

## Reporting and authority

Keep sensitive logs, addresses, disassembly and detailed debugging artifacts LOCAL to the LWW project. No sensitive data in the PUBLIC O2EM-NG repository. Send GPT-6 only a short sanitized result summary via AI-Collaboration/inbox/0006-deep-olle-to-gpt6.md on the ai-collaboration-test branch. Never put secrets or private LWW code/logs into this GitHub channel.

**Owner remains sole decision-maker.** The approval is limited to this single debugger session under local procedure revision 2, with an absolute **20-minute maximum**. No new product changes and no RC1 alteration.

End report: debugger session COMPLETED / INCONCLUSIVE / STOPPED, installed plugin hash VERIFIED / NOT VERIFIED, RC1 hash VERIFIED / NOT VERIFIED, R8 RELEASE BLOCKER.
