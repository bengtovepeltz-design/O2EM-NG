# Development / reference notes

Technical research that supports ongoing development. These are working notes,
not public release documentation.

| Document | What it covers |
| --- | --- |
| `Core Development Notes.md` | Curated outcomes of the incremental root-level patch notes: Intel 8244/8245 audio, Videopac+ / G7400 VPP, the C7010 NSC800 module, and catalogue/media work. |
| `MCS48_NG_Phase1_Analysis_2026-09-20.md` | Read-only autopsy of the inherited 8048 core and the proposed replacement design. |
| `MCS48_NG_Phase1_Opcode_Inventory.md` | Static 256-slot opcode / mnemonic / cycle inventory versus MAME. |
| `MCS48_NG_Phase2A_Scaffolding_2026-09-20.md` | Test/trace scaffolding: conformance harness, CPU adapter contract, input schedule. |
| `MCS48_NG_Phase2C_VBL_Timing_Prep_2026-09-21.md` | Verified PAL VBL timing divergence and the development-only timing probe. |

The MCS48-NG work runs in parallel with the legacy core. The legacy `cpu.cpp`
remains the active production CPU; the standalone conformance harness under
`tests/mcs48/` pins the current behavior (see `../CHANGELOG.md` and
`../Project.md` for the current status of that work).

Current public documentation lives one level up in `Docs/`.

## Licensing, provenance and credits maintenance rule

Before any release:

1. Update `Docs/SOURCE_PROVENANCE.md`, `Docs/THIRD_PARTY_NOTICES.md` and
   `Docs/Development/NG-0_Provenance.md` for any new component, data table or
   third-party dependency.
2. Update `Docs/CREDITS.md` as the canonical credits source.
3. Synchronise the application About/Credits/Special Thanks pages, the official
   website Credits page, the GitHub README, the release notes and the
   distribution documents.
4. Verify `Docs/Licenses/` contains every applicable licence text and that
   `tools/release-manifest.json` includes it in the installer and portable ZIP.
5. Do not claim clean-room implementation or complete originality for
   mixed-provenance components.

User-friendly acknowledgements do not replace the exact copyright and licence
notices.
