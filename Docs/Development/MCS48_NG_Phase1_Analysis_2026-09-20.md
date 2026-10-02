# MCS48-NG: Phase 1 autopsy and replacement design

Date: 2026-09-20. Scope: read-only investigation, isolated CPU experiments, and design. No emulator implementation changes.

## 1. Executive summary

Replacing the inherited CPU is justified as an engineering direction, but replacing it alone is not a demonstrated cure for VP61. Several independently observable defects exist in instruction execution and timer/interrupt accounting. The machine bus, video model, and sound scheduling also differ substantially from the reference. A new CPU must be introduced behind a controlled interface, with traces that distinguish those layers.

The strongest findings are:

* **HIGH confidence, demonstrated:** sequential PC increment fails the 2 KiB bank boundary; BUS immediate opcodes 88/98 do not consume their operand; the timer subtracts 31 instead of 32; timer/counter start permits both modes simultaneously; interrupt-entry cycles are not consistently propagated to machine time.
* **HIGH confidence, source and experiment:** the local adjacent O2EM CPU and current CPU have 252 of 256 identical normalized opcode case bodies. The four differing bodies are MOVX diagnostics. This does not mean the surrounding scheduler is unchanged.
* **HIGH confidence, source:** video uses segmented rendering and compatibility conditions rather than one continuously advancing device timeline. Register aliases, access restrictions, status acknowledgements, collisions, and character geometry differ from MAME.
* **HIGH confidence, source:** the active audio path already generates frame audio into a ring buffer; SDL consumption does not itself invent chip time. However, chip progression is reconstructed at frame end, register events are not fully timestamped, and disabling sound prevents hardware progression in that path. Those are the important architectural problems.
* **VP61 first divergence is NOT established.** Its ROM identity is verified. No paired execution trace was produced. CPU, bus/timing and VDC remain candidates; no claim that one of the confirmed CPU defects actually triggers VP61 is justified yet.

C7010 must remain a protected baseline. MAME is useful but explicitly documents unresolved timing and title-specific problems. It is not a substitute for hardware measurements or human runtime QA.

## 2. Baseline, evidence and current architecture

The project inspected is `D:\Project\O2EM-NG`, at HEAD `79c37c7b6140935fdc1d91c70d7c13ba83c80604`, with substantial pre-existing working-tree changes. The working files, not HEAD alone, are the baseline. `Docs/Project.md` and `Docs/CHANGELOG.md` were read first. Their current observations include explicit PAL reaching 50 FPS with `evblclk=7259`, unresolved Backgammon flicker, Plus regressions and Helicopter Rescue freeze, working Chess, and ongoing updater work. Earlier conversational assumptions are not substituted for that state.

Evidence labels used below:

* **S:** directly inspected current/local source.
* **P:** isolated compiled experiment against current CPU code.
* **I:** Intel MCS-48 manual.
* **M:** pinned MAME implementation, not automatically silicon truth.
* **G:** Sören Gust's G7000 technical documentation, not a Philips-authored manual.
* **H:** hypothesis or inference.

Classification: **A** equivalent/correct in the scope examined; **B** simplification probably harmless; **C** potential compatibility problem; **D** definitely inaccurate where sufficient CPU documentation/experiment supports it; **E** unresolved. Confidence describes the finding, not proof that it causes a particular game failure. A source difference can be HIGH confidence while its hardware interpretation remains E.

`cpu.cpp` owns global CPU registers, an opcode switch, timer state and portions of machine scheduling. `vmachine.cpp` owns memory decoding, port behavior, input, frame transitions and VDC register access. `vdc_stub.cpp` is an active renderer despite its name. `audio_sdl.cpp` is the active sound implementation; `audio_config.h` defaults `O2EM_USE_AUDIO8245` to 0. `audio8245.cpp` is an alternative implementation, not the active default.

Execution advances one instruction, then updates several clocks and devices. Some IRQ calls subsequently add instruction cycles after machine/coprocessor time was already advanced. HBLANK, VBLANK, input polling and sound are therefore not consumers of one authoritative event timeline. Register state is public/shared rather than a complete owned CPU state object. Existing save/load data omit enough CPU/timer/audio/coprocessor state that they cannot serve as deterministic differential checkpoints.

The Visual Studio project also depends on adjacent O2EM headers, including `../o2em/vdc.h`; a future extraction must eliminate accidental ABI/header dependence explicitly, not by moving files and hoping they compile.

## 3. Original O2EM inheritance map

Two local references were inspected:

1. `D:\Project\o2em\cpu.cpp`: adapted local source, with no verified pristine upstream version identity.
2. `D:\Project\Projekt 2 Marks files\o2em120B5src\SRC\cpu.c`: the local 1.20B5 source distribution.

Comparing case bodies after removing comments and whitespace gives **252/256 identical** between current O2EM-NG and the adjacent local CPU. Cases 80, 81, 90 and 91 differ through MOVX tracing. This is a textual measure, not a proof of complete semantic identity: includes, globals, helpers, scheduling and memory callbacks matter.

The 1.20B5 copy contains bank-preserving fetch/increment handling and corresponding branch/call differences that the current copy lacks. It also contains C7420/Z80-related integration absent from the active NG machine path examined. Both older lineage and newer NG code retain important timer/scheduler assumptions. It would be incorrect to say that every defect is universal to every original O2EM version.

The MAME CPU header also credits original work by Dan Boris. Its implementation is substantially evolved and structurally distinct, but describing the two lineages as entirely unrelated is too strong.

## 4. Detailed CPU difference matrix

The separate [256-opcode inventory](MCS48_NG_Phase1_Opcode_Inventory.md) lists every dispatch entry and nominal cycle count. All 256 slots exist; illegal slots are not evidence of missing legal instructions. All tabulated nominal cycle counts match MAME's 8048 table. This excludes interrupt entry, bus phases and scheduling correctness.

| Area | Current behavior versus reference | Class / confidence / evidence |
|---|---|---|
| Basic accumulator operations | ADD/ADDC use expected byte arithmetic and C/AC formulas. Exhaustive immediate ADDC checked all 131,072 A/operand/carry combinations without mismatch. | A / HIGH / S,P,I,M |
| Logical operations, exchanges, increment/decrement, rotates | Ordinary switch semantics agree on static inspection; most have not been exhaustively executed. INC/DEC preserve arithmetic flags. | A / MEDIUM / S,M |
| Decimal adjust | Current carry/low-nibble adjustment agrees structurally with MAME. No exhaustive DAA test was run. | A / MEDIUM / S,M |
| Nominal instruction cycles | 1/2 machine-cycle counts agree across the complete dispatch inventory. This says nothing about when external effects occur. | A / HIGH / S,M |
| Illegal opcodes | Silent one-cycle handling versus reference illegal handler. No valid instruction should depend on them; undefined silicon effects are not established. | B/E / MEDIUM / S,M |
| PC increment | Plain `pc++` crosses 07FF to 0800 and 0FFF to 1000. 8048 increments the lower 11-bit field while preserving the bank bit. ROM masking hides some out-of-range accesses but does not repair CPU state. | D / HIGH / S,P,I,M |
| Conditional branch pages | The ordinary page choice at the operand location agrees with MAME. JNZ at 01FF correctly reaches 0245. Do not replace this with a blanket post-operand-page rule. Bank-boundary increment is still wrong. | A, with separate D boundary defect / HIGH / S,P,M |
| JMP/CALL and bank select | Page encoding and A11 bank latch broadly follow 8048 structure. Return addresses inherit the bad increment at boundaries. IRQ-bank handling needs dedicated tests. | C / HIGH / S,M |
| MOVP/MOVP3/JMPP | Ordinary page lookups agree on inspection; MOVP3 uses page 0300 in both implementations. Boundary fetch state remains relevant. | A/C / MEDIUM / S,M |
| PSW | C, AC, F0, register-bank bit and stack index are assembled separately; PSW bit 3 reads as one. | A / HIGH / S,M |
| Register banks and RAM | Banks at RAM 0–7 and 24–31 and 64-byte indirect RAM masking fit the target 8048. Larger MCS-48 variants must not silently change the mask. | A / HIGH / S,I,M |
| Stack | Eight two-byte frames in RAM 8–23, wrapping pointer, PC plus high PSW bits. Representation differs from MAME but is not itself wrong. | A / HIGH / S,M |
| RET / RETR | RET restores PC without flags; RETR restores flags/bank state and exits interrupt service. RET during interrupt does not apply the bank restriction; probe returned 0834 rather than 0034. Intel states PC bit 11 is held zero during service until RETR. | D / HIGH / S,P,I,M |
| Reset | `init_cpu()` leaves F1, timer flag and prescaler state. Intel specifies clearing F1 and timer flag. Accumulator retention is not automatically a defect. Whole-machine initialization also clears RAM/timer, so warm and cold reset are conflated. | D for F1/TF; C for reset contract / HIGH / S,P,I |
| Reset port effects | Latches become FF without passing normal output callbacks. External module state may remain inconsistent until a later write. | C / MEDIUM / S,H |
| Timer divider | On crossing 31, subtracts 31 rather than 32. Probe increments at cycles 32, 63, 94 instead of a continuing 32-cycle interval. | D / HIGH / S,P,I |
| STRT T | Does not clear the prescaler or disable counter mode. Intel specifies a cleared divide-by-32 prescaler on timer start. | D / HIGH / S,P,I,M |
| STRT CNT / STOP | Start counter does not disable timer; both flags can remain true. STOP disables both. | D for simultaneous modes; A for stop / HIGH / S,P,I,M |
| Counter input | Increments at selected horizontal-line transitions while outside VBLANK, rather than sampled T1 falling edges. It is a Videopac shortcut inside CPU execution. | D as a generic CPU model / HIGH / S,I,M |
| Overflow and TF | Overflow sets TF; JTF tests/clears it. Exact source ordering relative to IRQ delivery differs. | A for basic TF, C for ordering / HIGH / S,M |
| Pending timer IRQ | NG preserves enabled overflow while another interrupt runs, important for Chess. Other pending behavior is conditional on compatibility state. This is not a clean hardware pending latch. | C / HIGH / S,I,M |
| External IRQ | Five-cycle synthetic `int_clk` pulse and conditional pending behavior replace a sampled physical interrupt line. An event during service can be lost. | C / HIGH / S,M |
| Priority/nesting | External pending is serviced before timer pending; nested entry is guarded. New-event ordering and line persistence differ. | A for basic priority; C for ordering / HIGH / S,M |
| IRQ-entry cycle accounting | Entry adds two cycles after machine/coprocessor clocks have already advanced on some paths. Probe: elapsed machine time 1 while `clk` is 3 on timer IRQ entry. | D for inconsistent accounting / HIGH / S,P |
| JNI | Tests the synthetic pulse instead of a consistently sampled external input pin. MAME itself contains a polling-related workaround. | C/E / HIGH / S,M |
| T0 / ENT0 CLK | Voice test input is a stub returning zero; ENT0 CLK does not drive a clock output. | C for current machine; D as complete CPU support / HIGH / S,I |
| T1 / JT1 / JNT1 | Reads a machine approximation based on horizontal count or VBLANK. No per-machine-cycle history drives counter edges. | C / HIGH / S,M |
| OUTL BUS,A | Opcode 02 consumes cycles but has no BUS output effect. | D as CPU support / HIGH / S,I,M |
| ORL/ANL BUS,# | Opcodes 88/98 neither update BUS nor consume their immediate byte. Probe leaves PC at 0001 instead of 0002. | D / HIGH / S,P,I,M |
| IN A,P1 | Reads the latch directly; external pin levels cannot pull bits low through an input callback. | C / HIGH / S,M |
| IN A,P2 | Keyboard read changes the shared P2 variable, mixing externally sampled pins with the output latch. | C / HIGH / S,M |
| P1/P2 output timing | Effects occur inside the opcode body before aggregate instruction time advances. No documented bus subphase timestamp is supplied. | C / HIGH / S,I,M |
| MOVX | Uses R0/R1 as the eight-bit external address, with read/write callbacks. Basic opcode operation is present; decode and simultaneous device responses are machine issues. | A for opcode shape; C for integration / HIGH / S,M |
| 8243-style port operations | Direct `read_PB/write_PB` calls omit PROG strobing and the P2 low-nibble changes modeled by MAME. Plus circuitry consumes these operations. | C / HIGH / S,M |
| Variants | Current implementation is effectively fixed 8048. MAME supports other RAM sizes, reduced opcode sets, and extra variants; importing those indiscriminately would be wrong. | B / HIGH / S,M |

Primary current anchors: `cpu.cpp:97` reset, `:111` external IRQ, `:128` timer IRQ, `:578` counter start, `:659` timer start, `:1007` RET, `:1032` and `:1128` BUS immediates, `:1694` prescaler. Line numbers refer to the inspected working files.

## 5. Experiments and compatibility implications

An isolated C++ harness compiled an include-stripped copy of the current CPU with inert machine callbacks using MSVC `/O2`. It executes the actual instruction bodies and scheduler, not a newly written approximation. It does not emulate the complete console, and no MAME core was linked into it. Expected values came from the manual/reference inspection. Stubs limit conclusions about peripherals.

Recorded results:

```text
NOP at 7FF -> 0800; reference 0000
NOP at FFF -> 1000; reference 0800
BUS 88 and 98 -> PC 0001; reference 0002
Timer increments: 32, 63, 94; remainder 1 each time
STRT T with previous prescaler 17 -> 18, rather than reset
STRT T followed by STRT CNT -> timer=1 and counter=1
init_cpu retains F1=1, TF=1 and prescaler=17
Pending timer IRQ -> PC 007, clk=3, machine elapsed=1
RET during IRQ with stacked 834 -> 834; MAME 034
JNZ at 1FF -> 245, matching reference
ADDC immediate result/C/AC mismatches: 0 / 131072
```

These findings justify new conformance tests. They do not justify changing all of these behaviors in the production CPU in one patch. Timer corrections can move video register writes, Chess communication and sound events even when instruction results become more accurate. Preserve a frozen legacy baseline to diagnose that movement.

No full emulator Rebuild, gameplay run, exhaustive all-opcode semantics test, or Debug-versus-Release validation was performed in this read-only phase.

## 6. VP61: verified identity, unresolved first divergence

The inspected `x64/Debug/ROMS/vp_61.bin` is 4096 bytes, CRC32 `69D21F8F`, SHA1 `C0B10B79461BC1939E08E4C05166126D2B8D7DDA`. This matches the supplied identity and MAME's `pr_interpol.bin` entry. MAME selects ordinary `o2_rom`, not the VP31/VP40 XROM slot.

The user's runtime evidence is that NG reaches gameplay with incomplete graphics, related O2EM builds behave similarly, and MAME 0.289 renders correctly. That is strong motivation for differential work, not a localization of the defect. No synchronized PC/bus/VDC traces were available or captured here. Therefore no first divergent instruction, cycle, register or scanline can honestly be reported.

Best available candidates are: timer/IRQ phase drift; different simultaneous bus responses; rendering that sees different intermediate register states; object height/enable/priority semantics. PC/BUS opcode defects are proven generally but are **not shown to be exercised** in this failing path. Rank VP61 causal confidence LOW until a trace links one to the failure.

The first trace should begin before BIOS initialization, with the same BIOS hash, ROM, PAL machine configuration, reset and scheduled key-0 press/release. Record an absolute timestamp and sequence number, pre-PC/opcode, all architectural registers, RAM checksum plus changed bytes, timer/prescaler/mode, TF/pending/active IRQ state, pin levels and port latches. Record program and MOVX reads/writes, BUS, P1/P2 changes, VDC access and IRQ transitions in the same timeline. Frame numbers alone are insufficient.

Compare in layers:

1. Match reset and input events; normalize memory aliases and clock units.
2. Find the first architectural-state difference with identical input/read values: CPU case A.
3. If input/read values or event timestamps differ first, retain the preceding bus/IRQ window: case B.
4. Only if CPU state and timestamped VDC stream agree, replay that stream into the two video models and compare device state/pixels: case C.

Keep pre-divergence history in a bounded ring, stop at the first difference, and retain a reproducible fixture. A whole-session console flood makes this harder. Identical untimed register sequences do not establish case C: their timing must agree too.

## 7. Machine bus and ownership

| Component | Current implementation / reference difference | Assessment |
|---|---|---|
| Program ROM | Standard 4 KiB loading rearranges two 2 KiB banks; the reversed P1 bank expression compensates the storage order. This is not by itself a wrong-bank proof. | A/MEDIUM, S,M |
| MOVX read selection | Current `ext_read_impl` prioritizes C7010, VDC, RAM, Plus, XROM, Mega paths. MAME combines selected device responses with wired-AND starting at FF. Current unclaimed read returns 00. | C/HIGH, S,M |
| MOVX writes | Current `else` selection generally chooses one target, with a Chess exception. MAME broadcasts to independently selected RAM/cart/VDC/Plus listeners. | C/HIGH, S,M |
| External RAM | Current access can expose the high half of a 256-byte array where the reference gates ordinary 128-byte RAM with address bit 7. Special cartridge registers complicate the upper range. | C/HIGH, S,M |
| P1 | Drives selects, program banking, luminance and C7010 reset. These are machine wiring, not generic CPU instruction semantics. | Boundary fact/HIGH, S |
| P2 | Keyboard scan plus XROM high address bits; pin sampling must not overwrite the CPU latch. | C/HIGH, S,M |
| VP31/VP40 | Full XROM data access uses P11 and P2 high address selection. Existing support must remain outside CPU. Gust documents G7400 startup bus contention for these cartridges; a G7400 failure is not automatically a CPU defect. | C/E, MEDIUM hardware, S,G,M |
| MegaCART | Dedicated allocation/mapping and RAM[81] data-bank selection exist. A corresponding ordinary high-address write path was not clearly established in the inspected decoder. Trace before calling this broken. | C/MEDIUM, S,H |
| C7010 | Dedicated 8 KiB firmware, mirrored 2 KiB RAM, two communication directions, reset/select hooks and NSC execution exist. A roughly 10,000-instruction frame budget is distributed using 8048 cycles; this is not a verified NSC clock/T-state model. | C/HIGH, S |
| C7420 | Firmware recognition/catalog/frontend references found; no active C7420 CPU/bus emulation integration found in the inspected NG source/project. Do not represent it as an already working coprocessor. | S/HIGH, bounded source search |
| Keyboard/joystick | Current keyboard priority and latch mutation differ from pin-level modeling. BUS joystick select uses P1 conditions differing from MAME's P12 gate and P2 row. | C/HIGH, S,M |
| Voice | `voice_stub.cpp` is inactive functionality, including zero test input. | S/HIGH |
| IRQ and frame | Frame/pulse logic substitutes for persistent device lines and acknowledgment. Input is sampled around frame events. | C/HIGH, S,M |

Machine anchors: `vmachine.cpp:130` VBLANK; `:180` frame end; `:360` T1; `:369` P1; `:393` keyboard/P2; `:461` MOVX decoder; `:580` BUS input; `:614` writes.

The CPU must own registers, RAM/stack, instruction execution, port latches, pin sampling rules, timer/counter and interrupt recognition. The machine must own address decoding, pull-ups/wired responses, keyboard/joystick wiring, ROM/BIOS, RAM devices, VDC, Plus and cartridge/coprocessor state. Neither side should inspect game CRCs to implement a CPU instruction.

## 8. 8244/8245: register and rendering comparison

Reference register map, including unused areas:

| Address | Function and important reference semantics |
|---|---|
| 00–0F | Four minor-object slots: Y, X, attributes, unused byte |
| 10–3F | Twelve major characters: Y, X, character pointer, color |
| 40–7F | Four quad groups; shared X/Y aliases within each group, individual character/color fields |
| 80–9F | Four sets of eight sprite-pattern bytes |
| A0 | Control/display/grid/interrupt enables |
| A1 | Status read and acknowledgement |
| A2 | Collision selection on write, accumulated collision result on read |
| A3 | Color control |
| A4/A5 | Beam Y/X readback / latched coordinate behavior |
| A6 | Unused |
| A7–A9 | Three sound shift-register bytes |
| AA | Sound control |
| AB–AF | Unused |
| B0–BF | A0–AF aliases in MAME |
| C0–C8 | Horizontal grid low segment bits |
| C9–CF | Unused |
| D0–D8 | Horizontal grid high bit, other bits masked |
| D9–DF | Unused |
| E0–E9 | Vertical grid segments |
| EA–EF, F0–FF | Unused/unmapped |

The NG flat register array is not equivalent to that access map. Important differences:

| Topic | Difference | Class / confidence |
|---|---|---|
| Aliases/access | Quad XY aliases partly handled; B-register aliases absent. Several read-only/unused locations can retain writes. Some write-only/unused reads return the address rather than reference zero. | C/HIGH S,M |
| Bit masks | Reference masks character color, low Y bit and high grid bits. Current implementation applies some masks during rendering, not consistently at register storage/readback. | C/HIGH S,M |
| Enabled object access | Both restrict object writes with display enabled. Current object reads can alternate FF/00; MAME returns zero. | C/E HIGH difference, hardware unresolved |
| Active grid writes | Current code stores clock-derived garbage in some circumstances; MAME ignores restricted writes. A legacy comment is not hardware measurement. | C/E HIGH difference |
| Status | Current A1 synthesizes parts from clocks/control and clears sound state, but does not reproduce all latched overlap/status bits or MAME IRQ-line acknowledgment. | C/HIGH S,M |
| Beam | Current X derives from `h_clk*12`, Y from `master_clk/22`. Assigning to a byte before clamping can wrap at 256. Reference derives from its raster and boundary rules. | C/HIGH S,M |
| Visible writes | NG draws selected regions on control changes, counter overflow and VBLANK, with color vectors separately. MAME advances video before relevant reads/writes. | C/HIGH S,M |
| Collision | NG uses rendered collision buffers and later mask evaluation/clearing. MAME accumulates at raster time, has read-to-clear status, major-object overlap and external collision behavior. Sampling time and changing masks can matter. | C/HIGH S,M |
| Display enable | Foreground gating includes compatibility-dependent `useforen`; it is not uniformly the device's display bit. Chess also uses a snapshot compatibility path. | C/HIGH S |
| Character height | Current single-character formula differs from reference: e.g. even Y=32, pointer=0 gives eight source rows locally versus seven in MAME. This is a source-level example, not a VP61 trace. | C/HIGH S,M |
| Quads | Shared addressing and last-character-derived height broadly resemble reference, but common sampling/access restrictions differ. | A/C MEDIUM S,M |
| Grid | Dot count/geometry and edge behavior differ; local nine versus reference ten horizontal dot positions is one concrete difference. Crop/origin must be normalized before pixel comparisons. | C/MEDIUM S,M |
| Sprites | Pattern/zoom/shift ideas agree broadly; clipping limits, priority and interactions differ. Current fixed limits include X<164/Y<232. | C/MEDIUM S,M |
| Priority | Local object iteration/compositing differs from MAME's priority map, including overlaps involving transparent character pixels. | C/HIGH S,M |
| Colors | Bit ordering, palette conversion, luminance and Plus masking happen in different layers. Raw palette index differences do not prove wrong output color. | E/MEDIUM S,M |
| PAL/NTSC | Reference has different raster lengths, clock ratios and some character suppression rules. NG relies on fixed shared approximations with frame-length changes. | C/HIGH S,M |

No independently verified manufacturer 8244/8245 timing specification was obtained in this phase. Consequently the VDC differences are not all labeled definitely wrong silicon behavior. Plus EF934x composition was traced as a boundary but not exhaustively audited.

### Timing arithmetic

NG uses line counter 21 cycles, beam/audio conversions of 22 cycles, and a legacy draw-region conversion that can use 20; Chess introduces another offset/conversion. There is no single raster relationship.

| Quantity | NG configured arithmetic | MAME reference configuration |
|---|---|---|
| NTSC frame | 5964 machine cycles, 60 nominal FPS | 455 dots × 263 lines; CPU/dot ratio 1:20 |
| PAL frame | 7259 machine cycles, 50 nominal FPS | 456 dots × 313 lines; CPU/dot ratio 1:18 |
| Audio ticks requested per frame | Integer frameCycles/22: 271 NTSC, 329 PAL | One per raster line: 263 NTSC, 313 PAL |
| PAL nominal CPU budget/second | 362950 | About 394099 from the reference oscillator/divider |

These are source-derived budgets, not measured speed or an assertion that MAME's PAL model is perfect. IRQ cycle loss adds another source of disagreement. MAME marks PAL slave timing as incomplete and questions some G7400 timing. Its HBLANK boundaries are approximately dot 366 to 453/454, with separate background-gate/status windows; NG's simple `h_clk` thresholds are not equivalent to all of those signals.

## 9. Audio autopsy

The active path writes the 24-bit state from A7–A9, records some AA values by scanline, then calls `audio_generate_frame()` at frame end. Samples enter a mono U8 44.1 kHz ring; SDL drains that buffer. This already separates production from consumption to a degree. It is not the simplistic SDL-demand-driven architecture described in the initial concern.

The remaining problems are substantial:

| Feature | NG active path versus MAME | Assessment |
|---|---|---|
| Write timing | A7–A9 mutate shift state immediately; no complete timestamped event sequence preserves multiple writes through a frame. Later generation can apply final state to earlier times. | C/HIGH S,M |
| Pending/clock | Pending/write suppression exists, but line ticks are replayed at frame end. MAME clocks on actual HBLANK events and updates its audio stream before state changes. | C/HIGH S,M |
| Divider | Both contain fast/slow 4/16 line division concepts; different line counts and event order alter frequency/phase. | A for concept, C for timing / HIGH |
| Rotation | NG AA bit 6 chooses recirculation versus zero-fill. MAME masks that bit and recirculates. Gust describes selectable behavior, so sources conflict. | E/HIGH difference, unresolved hardware |
| Enable | NG enable gates clock progression; reference enable gates audible output while shift progression continues. Entire NG sound generation also returns when host sound is disabled. | C/HIGH S,M |
| Noise | NG uses an additional 16-bit LFSR, seed ACE1 and taps 15/13/0, XORing output. MAME uses feedback from bits 0 and 5 within the 24-bit register into bits 15/23. | C/E HIGH difference |
| Output phase | NG samples post-shift bit state; MAME latches the outgoing bit. | C/HIGH S,M |
| Volume | NG scales amplitude; MAME models a 16-phase duty-cycle output. Averaging can be acceptable for host PCM only if gain and phase effects are justified. | B/C MEDIUM |
| Interrupt | NG counts 24 shifts and delivers sound effects/IRQ around frame generation. MAME handles them on sound edges with enable-dependent status. MAME notes uncertainty over 8244 versus 8245 IRQ behavior. | C/E HIGH difference |
| SDL | Ring/stream remains a useful sink. Underrun policy must not determine chip time. | B/HIGH S |

The inactive `Audio8245` class has a different atomic pending-register scheme and another LFSR polynomial, but shares questionable control-bit assumptions. Merely enabling it is not a proven accuracy fix.

Current source anchors: `audio_sdl.cpp:262` register updates; `:347` pending/shift processing; `:436` output-bit sampling; `:492` frame generation and sound-disabled early return.

## 10. Proposed MCS48-NG architecture

Use an owned `Mcs48State` with A, 12-bit PC, PSW/SP, F1, register-bank selection, 64-byte RAM, memory-bank latch, port/BUS latches, timer/prescaler/mode, sampled T1 history, timer flag, IRQ enable/pending/in-service state and clock count. Explicit power-on and reset APIs must distinguish retained state. Snapshot all state needed to resume identically.

CPU-facing machine operations should be narrow: program read, external data read/write, BUS and port pin sampling/output, T0/T1 and IRQ inputs, PROG output, and advancement to a timestamped bus phase. Trace observers are optional and non-mutating. CPU code has no SDL, database, cartridge identity or VDC register knowledge.

A machine scheduler owns absolute time and oscillator ratios. CPU instructions consume machine cycles, including interrupt entry; port/bus events occur at defined offsets within those cycles. Start with documented machine-cycle granularity and preserve an API capable of finer phases. Do not promise transistor-level timing. Devices advance to an access timestamp before responding. Same-timestamp event ordering must be explicit and testable.

VDC, audio and coprocessor clocks derive from rational oscillator ratios, not independent FPS guesses. Pacing throttles presentation after emulation; it does not change hardware time. C7010 gets a separate NSC cycle model in its own later work, retaining the current adapter initially. Future C7420 belongs behind a machine expansion interface, not in generic CPU dispatch.

Keep `LegacyCpuAdapter` and `Mcs48NgAdapter` behind an internal development selector. Legacy remains default. Tests may run two independent machine instances; they must not share mutable globals. The selector is not a permanent user-facing compatibility switch.

## 11. Hardware-timed sound design

Treat video and sound as domains of the same emulated chip timeline. A7–AA writes first advance the chip to their timestamp, then update state according to verified write rules. HBLANK edges advance divider/shift/noise and assert device IRQs on that timeline. Muting affects only output gain/queueing, never timer, shift or IRQ progression.

An edge-integrating resampler converts the held output level/PWM waveform into host samples using a persistent fractional phase. It feeds the existing PCM ring and SDL stream. PAL/NTSC oscillator changes naturally alter frequency and edge times; they do not require invented line counts per host frame. Buffer underruns produce a host-output policy, not extra emulated cycles. Fast-forward may discard output but must retain chip progression. Snapshot divider, pending writes, shift state, output latch and resampler phase.

## 12. Staged migration and gates

| Stage | Deliverable | Gate before proceeding |
|---|---|---|
| 1 | This evidence map and interface design | Review uncertainties; retain baseline |
| 2 | Legacy adapter, isolated NG state/interface skeleton, deterministic trace/test infrastructure | Legacy default and behavior unchanged; repeatable traces |
| 3 | NG ordinary opcodes, PC/RAM/stack and cycle consumption | All legal opcode tests, boundary/property tests, illegal policy |
| 4 | Reset, timer/counter, IRQ, pins, ports, MOVX | Edge/order tests including interrupted execution and reset |
| 5 | Paired reference differentials | VP61 first divergence localized; no game-specific CPU patch |
| 6 | Unified machine clock and bus decoder | Timestamped bus replay; PAL/NTSC and selected-device tests |
| 7 | VDC register and raster semantics | Register replay plus frame/collision/status comparisons |
| 8 | Hardware-timed sound | Register-event fixtures, frequency/noise/IRQ checks, listening QA |
| 9 | Full regression campaign | Human approval of protected games and frontend lifecycle |
| 10 | NG default | Documented known differences, rollback path, beta feedback |
| 11 | Retire legacy from active build | Sustained compatibility; archive/reference preserved |

Stages are not a license to postpone diagnostics until after implementation. Trace infrastructure precedes the new engine, and each subsystem changes behind a separate comparison gate.

## 13. Regression risks

Most dangerous: changing CPU clocks without changing device phase conventions; exposing latent bus decode errors after CPU corrections; losing Chess's currently successful pending-IRQ behavior; removing rendering workarounds before accurate device semantics replace them; accidental ABI/global-state coupling between cores; audio mute altering machine execution; and using incompatible saved state as a baseline.

VP48 cannot be reduced to PAL selection: explicit PAL is already verified. MAME itself lists Backgammon character-update problems. VP59+ needs a long run through the tank/desert transition; MAME also lists Helicopter Rescue timing-related lockups. Neither is a simple universal pass/fail oracle. VP31/VP40 G7400 behavior needs independent hardware evidence because technical documentation describes incompatibility while MAME permits operation.

No current compatibility condition should be silently moved into generic CPU semantics. Maintain a dated ledger of each machine workaround, its evidence and its eventual retirement criterion.

## 14. Required human QA and reproducible fixtures

| Coverage | Required checks |
|---|---|
| Race, Bowling/Basketball, Golf | Start, movement, scoring, collisions, sound and exit |
| Gunfighter, Munchkin, Atlantis, Pickaxe Pete, Cosmic Conflict | Gameplay, enemies/projectiles, object priority, collisions, sound, both controllers |
| C7010 Chess | Same documented 1 / Yes / 1 setup, legal moves, capture, opponent reply, longer play, reset; stable complete board; both BIOS choices where already supported |
| VP31 and VP40 | G7000 PAL interaction/music/gameplay; separately record G7400 behavior without assuming a CPU bug |
| G7400/Plus library | Ordinary and enhanced graphics, masking, colors, transitions and return to library |
| VP48 | Capture sustained flicker with explicit PAL and configuration recorded |
| VP59+ | Reach former freeze location and continue beyond it |
| VP61 | Reproduce select-game, key 0 and representative gameplay, compared with a pinned MAME configuration |
| Platform lifecycle | Keyboard/controller mapping, reconnect, reset, multiple launches, return to frontend, audio enabled/muted, fullscreen/windowed |
| Timing/build matrix | Explicit PAL and NTSC with appropriate BIOS/software; Debug and Release; same ROM/BIOS hashes and settings |

Record build identity, BIOS/ROM hashes, region, machine, inputs, checkpoints and known defects. Human gameplay/visual/listening QA is the product acceptance authority; automated traces explain failures and prevent regressions, not replace that acceptance. This phase does not certify these checks as passed.

## 15. Sources and licensing/attribution

Official MAME sources were pinned to **6512b9adc62a2458c412342268f500f8489edb95** rather than relying on a moving master. The `mame0289` versions of CPU, machine driver and VDC were also inspected. Current versus 0.289 CPU changes did not change the 8048 opcode implementation examined; the VDC change is a screen-configuration readiness check. A complete runtime equivalence claim between releases is not made.

| External material inspected | Declared license / attribution |
|---|---|
| [mcs48.cpp](https://github.com/mamedev/mame/blob/6512b9adc62a2458c412342268f500f8489edb95/src/devices/cpu/mcs48/mcs48.cpp), mcs48.h; 0.289 CPU copy | BSD-3-Clause; Dan Boris, Mirko Buffoni, Aaron Giles, Couriersud |
| [odyssey2.cpp](https://github.com/mamedev/mame/blob/6512b9adc62a2458c412342268f500f8489edb95/src/mame/philips/odyssey2.cpp), 0.289 copy | BSD-3-Clause; Peter Trauner, Wilbert Pol, hap |
| [i8244.cpp](https://github.com/mamedev/mame/blob/6512b9adc62a2458c412342268f500f8489edb95/src/devices/video/i8244.cpp), i8244.h; 0.289 copy | BSD-3-Clause; Peter Trauner, Wilbert Pol, hap |
| [4in1.cpp](https://github.com/mamedev/mame/blob/6512b9adc62a2458c412342268f500f8489edb95/src/devices/bus/odyssey2/4in1.cpp), chess.cpp | BSD-3-Clause; hap |
| rom.cpp in the same cartridge directory | BSD-3-Clause; Wilbert Pol, Fabio Priuli, hap |
| [videopac.xml](https://github.com/mamedev/mame/blob/6512b9adc62a2458c412342268f500f8489edb95/hash/videopac.xml) | Hash database covered by MAME COPYING's CC0 designation for hash material; this does not license the ROM bytes |
| MAME COPYING and license text BSD-3-Clause | Aggregate MAME distribution is GPL-2.0; individual file headers are decisive for the studied BSD files. Dependencies not inspected are not cleared by this table |
| Adjacent local O2EM source and 1.20B5 source | Local COPYING / distribution LICENSE.TXT contain the Clarified Artistic License; source credits Daniel Boris, André de la Rocha and Arlindo Oliveira. Local provenance/version needs preservation |
| [Intel MCS-48 User's Manual, July 1978, 9800270D](https://bitsavers.trailing-edge.com/components/intel/8048/9800270D_MCS-48_Family_Users_Manual_Jul78.pdf) | Copyright Intel 1978, all rights reserved; technical reference, not a source-code reuse license |
| [G7000 BIOS documentation](https://www.videopac.nl/g7kbios.pdf) | Copyright Sören Gust 1997–2006; no explicit reuse license identified. This is not a Philips publication |

Relevant Intel manual locations include PDF page 35 (timer/counter and prescaler), page 38 (reset state), page 56 (bank-preserving PC increment and interrupt bank restriction), and the instruction descriptions. Gust's XROM discussion appears on PDF pages 27–28; sound discussion appears around pages 50–51. These documents were used as references, not copied into the project.

Future BSD code reuse must retain copyright notices, conditions and disclaimer, reproduce required notices in binary distribution materials and respect non-endorsement. An NG-native implementation can study semantics without copying MAME's device framework. If code is copied later, identify the exact files/lines and inspect every new dependency's license first. This report does not establish the current project's overall outbound license, or grant rights to BIOS/ROM/media. No external implementation was pasted into the emulator.

## 16. Uncertainties and limits

* No measured first VP61 divergence; no silicon measurements; no paired MAME/NG runtime trace.
* MAME explicitly questions timer-IRQ timing, has JNI polling accommodations, incomplete PAL slave timing, and uncertainty about sound IRQs on 8245. Treat those details as research targets.
* MAME's sound bit-6 behavior conflicts with Gust's account. Exact noise, output and interrupt semantics need stronger documentation or hardware tests.
* Whole-system reset retention versus power-on initialization is not yet specified in NG.
* Current C7010 success is user-verified functionality, not proof of exact NSC timing.
* No active C7420 integration was found; the scope of future support must be established separately.
* Static agreement is not an exhaustive opcode proof; only ADDC received exhaustive arithmetic input coverage here.
* The adjacent O2EM reference is locally adapted. A precise upstream ancestry history remains unverified.
* Full EF934x/Plus device behavior and every cartridge variant were not exhaustively audited.

## 17. Exact recommended Phase 2 task

**Implement diagnostic scaffolding and coexistence interfaces only, with the existing CPU remaining the default and unchanged in behavior.**

Create a minimal CPU state/interface contract and a legacy adapter. Add an unselected MCS48-NG skeleton and a standalone conformance target, without implementing a new production opcode engine yet. Define absolute timestamps, input scheduling, bus event records and snapshot completeness. Add bounded optional tracing at the CPU/machine boundary, disabled by default. Preserve all pre-existing working-tree edits and update Visual Studio membership only for the newly added files.

Turn this phase's PC, BUS, prescaler, mode-switch, reset and IRQ-accounting experiments into explicit legacy-known-failure fixtures and future NG expectations. Keep the exhaustive ADDC success as a positive control. Trace enabled/disabled must leave legacy framebuffer, machine state and bus sequence unchanged for deterministic fixtures; only wall-clock overhead may differ.

Capture a short VP61 baseline with exact ROM/BIOS hashes and explicit PAL, reproduce the same input schedule in pinned MAME 0.289, and publish the first meaningful difference or a precise instrumentation blocker. Do not patch that difference during the capture task. If extracting a legacy adapter would change timing, reduce Phase 2 to the observation hooks and external harness first.

Acceptance: Debug and Release build; legacy remains selected; ordinary game and Chess smoke QA; deterministic repeatable trace; bounded memory use; no UI/database/media/updater changes; no CRC workaround; no CPU replacement, commit or push without a subsequent request.

**Phase 1 ends here.** The evidence supports modernization, but the next step is a trustworthy comparison boundary, not a wholesale rewrite.
