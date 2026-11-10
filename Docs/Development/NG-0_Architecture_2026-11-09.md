# O2EM-NG New Machine Core — NG-0 Architecture

Date: 2026-11-09
Status: architecture checkpoint. No production behavior changed.
Scope: NG-0 only. CPU implementation starts in NG-1.

---

## 0. Decision and guardrails

The legacy O2EM machine is frozen as a **historical / A/B reference**.

- `cpu.cpp`, `vmachine.cpp`, `vdc_stub.cpp`, `vpp_sdl.cpp` and the current
  production path receive no new correctness fixes.
- The legacy core remains buildable and runnable for comparison.
- All new correctness work goes into `src/core_ng/`.
- No game-specific patches, CRC timing branches, raster offsets, IRQ
  suppression or fixed delays are permitted in the new core.
- Default production remains legacy until NG passes the migration gates.
- NG-0 itself adds documentation and inert headers only. It does not alter
  `O2EM-NG.vcxproj`, `main.cpp`, `emulator_core.cpp` or any legacy file.

The target is a modern Philips Videopac / Odyssey² machine, not “old O2EM with
more fixes.”

Reference priority:

1. real Philips hardware
2. original hardware documentation
3. MAME 0.289 behavior/source as a reference
4. Manopac / historical O2EM as provenance and regression evidence
5. old O2EM behavior last

---

## A. New-core directory and module architecture

```text
src/core_ng/
  README.md
  machine.h              public Machine API used by the app adapter
  machine_config.h       BIOS/cartridge/region/profile configuration
  timebase.h             rational clocks and absolute machine time
  scheduler.h            deterministic event scheduler
  scheduler_event.h      event kinds, priorities, timestamps
  state.h                snapshot/restore contract

  cpu/
    mcs48_ng.h           owned-state Intel 8048 CPU
    mcs48_bus.h          CPU-facing machine bus contract
    mcs48_state.h        CPU state POD
    mcs48_disasm.h       (later) deterministic disassembly/trace labels

  memory/
    bus.h                CPU/data/VDC address spaces
    cartridge.h          cartridge image + metadata
    mapper.h             banking / MegaCART / special hardware
    memory_map.h         BIOS overlay, ext RAM, open bus

  video/
    vdc.h                common i8244/i8245 device contract
    i8244.h              NTSC profile/device
    i8245.h              PAL profile/device
    vdc_state.h          register/raster/collision state
    framebuffer.h        visible frame + metadata
    vpp.h                G7400 VPP / EF934x device contract

  input/
    input.h              controller/keyboard matrix state

  audio/
    audio_device.h       clean audio boundary; legacy engine adapts later

  diagnostics/
    trace.h              adapters for existing TraceRing/InputSchedule
```

Design rules:

- One `Machine` instance owns all mutable state. No emulation globals are
  shared between instances.
- Devices do not know about SDL, the frontend, the database or cartridge
  filenames.
- CPU, VDC, VPP and audio receive only their interfaces.
- The legacy core is not included by any new-core translation unit.
- Cross-core code is limited to data tables and diagnostics with documented
  provenance.

---

## B. Frontend ↔ machine API

The frontend keeps its current library, database, media, settings and SDL
presentation. Only the emulation boundary changes.

```cpp
namespace o2em::ng {

struct MachineConfig {
    std::string bios_path;
    std::string cartridge_path;
    Region region = Region::Auto;
    bool scanlines = false;
    bool vpp_enabled = false;      // derived from BIOS/profile normally
};

struct MachineInfo {
    std::string bios_name;
    std::string cartridge_name;
    Region active_region;
    uint32_t cartridge_crc;
    int video_width, video_height;
    double frame_rate;
};

struct InputState {
    uint8_t p1_buttons = 0;
    uint8_t p2_buttons = 0;
    uint8_t keyboard[8] = {};
    bool reset = false;
};

class MachineHost {
public:
    virtual ~MachineHost() = default;
    virtual void PresentFrame(const Framebuffer&) = 0;
    virtual void PushAudio(const int16_t* samples, size_t count) = 0;
    virtual void OnFrameComplete(uint64_t frame_number) = 0;
};

class Machine {
public:
    static std::unique_ptr<Machine> Create(const MachineConfig&,
                                           MachineHost&);
    virtual ~Machine() = default;

    virtual bool Load() = 0;                 // BIOS + cartridge + profile
    virtual void Reset() = 0;
    virtual void RunFrame() = 0;
    virtual void RunCycles(uint64_t) = 0;
    virtual void SetInput(const InputState&) = 0;

    virtual const Framebuffer& GetFramebuffer() const = 0;
    virtual MachineInfo GetInfo() const = 0;

    virtual bool SaveState(std::vector<uint8_t>&) const = 0;
    virtual bool LoadState(const uint8_t*, size_t) = 0;
};

} // namespace o2em::ng
```

The app adapter is a thin `IMachineCore` implementation:

```text
FrontendApp / launcher
        |
        v
EmulatorCore_StartRom (existing entry point, later dispatch)
        |
        +-- legacy adapter -> current vmachine/cpu/vdc_stub
        |
        +-- NG adapter     -> o2em::ng::Machine
```

The frontend must never read `pc`, `master_clk`, `VDCwrite`, `ColorVector` or
other legacy globals. Diagnostics may be exposed through an optional
`MachineDebug` interface (framebuffer hash, register snapshot, trace ring).

---

## C. Machine scheduler and timebase

One authoritative machine time, expressed as exact integer ticks of a chosen
master oscillator. Rational ratios convert to CPU cycles, pixels, audio edges
and device clocks without floating-point drift.

```cpp
struct Rational { int64_t num; int64_t den; };

struct HardwareProfile {
    Rational cpu_hz;          // 8048 machine-cycle clock
    Rational pixel_hz;        // i8244/i8245 pixel clock
    int pixels_per_line;
    int lines_per_frame;
    int vblank_start, vblank_end;
    int hblank_start, hblank_end;
    int bgate_start;
    int visible_x, visible_y, visible_w, visible_h;
    Rational audio_hz;        // 8245 sound clock domain, later
};
```

Scheduler model:

- `Tick` is the absolute machine timestamp.
- An event is `{tick, priority, sequence, kind, payload}`.
- Ordering is deterministic: `(tick, priority, sequence)`.
- Explicit priority bands:
  1. CPU bus/IRQ/port events at the same timestamp
  2. video beam/HBL/VBL events
  3. timer/counter sampling
  4. audio edge events
  5. end-of-frame presentation
- The CPU advances one instruction, then the scheduler advances all devices to
  the new timestamp. A device access advances the device to the access
  timestamp before it responds (MAME `update_now` behavior).
- No device computes its own line/position from host FPS. PAL and NTSC are
  profiles of the same scheduler.

Frame pacing is a presentation policy applied after emulation time; it never
changes hardware time.

---

## D. MCS48-NG CPU production design

The CPU is a real owned-state Intel 8048, not an adapter or observer.

```cpp
class Mcs48Cpu {
public:
    void PowerOn();                  // power-on state
    void Reset();                    // reset-visible state
    StepResult Step();               // exactly one instruction
    void SetIrqLevel(bool);          // /INT level
    void SetT0(bool); void SetT1(bool);
    const Mcs48State& State() const;
    Mcs48State& MutableState();
};
```

Owned state:

- A, 12-bit PC, PSW/SP, F0, F1
- active register bank and all 64 bytes of internal RAM
- MB0/MB1/A11 latch
- P1/P2/BUS latches
- timer, 5-bit prescaler, timer/counter mode, timer flag
- T1 history for counter mode
- external IRQ enabled, pending, in-service
- timer IRQ enabled/pending
- cycle count and last instruction PC

Semantics to implement and verify:

- all documented opcodes, including `ORL BUS,#n`, `ANL BUS,#n`
- exact opcode cycle counts
- 12-bit PC boundary behavior
- CALL/RET/RETR stack and bank restoration
- external IRQ sampling before each fetch, `EN I`/`DIS I`, in-service state,
  `RETR`, and the interrupt entry cycle cost
- timer 32-cycle cadence, `STRT T` prescaler reset, `STRT CNT` exclusivity
- T0/T1 pin sampling and counter edges
- BUS read/write, MOVX read/write, P1/P2
- reset clears F1, timer flag and prescaler per hardware
- illegal opcode policy documented and tested

The CPU has no knowledge of the VDC, VPP, cartridge identity or SDL. It talks
only to `Mcs48Bus`.

The existing `tests/mcs48` harness becomes the NG conformance target: the 11
legacy expected failures flip to NG pass expectations, plus full-opcode and
property tests. Legacy keeps its own expected-failure labels.

---

## E. i8244 / i8245 device design

One device family, two hardware profiles.

```cpp
class VdcDevice {
public:
    virtual ~VdcDevice() = default;
    virtual void Reset() = 0;
    virtual void AdvanceTo(Tick) = 0;          // beam/raster progression
    virtual uint8_t Read(Reg) = 0;             // side effects at current tick
    virtual void Write(Reg, uint8_t) = 0;      // update-on-write
    virtual bool IrqLine() const = 0;
    virtual const Framebuffer& Framebuffer() const = 0;
    virtual VdcSnapshot Snapshot() const = 0;
};
```

Profiles:

| | i8245 PAL | i8244 NTSC |
|---|---|---|
| pixels/line | 456 | 455 |
| lines/frame | 313 | 263 |
| VBL | line 242, h=365 | line 242, h=365 |
| HBL | 366..453 | 366..452 |
| BG sync | 414 | 413 |

Register model:

- A0 control (display enable, grid, strobe, IRQ enables)
- A1 status: latched VBL, hstatus, position strobe, sound IRQ, collision/
  external status as verified
- A2 collision status with read-clear
- A3 color/background
- A4 Y beam / latched Y
- A5 X beam / latched X
- object, quad, shape, grid and major/minor registers with hardware masks
- readback rules: live vs latched, write-only, open bus
- A1/A2 reads advance the device to the current timestamp and apply side
  effects exactly once

Rendering:

- beam/scanline-driven, not “draw frame then apply offsets”
- mid-frame writes affect only subsequent scanlines
- visible area is a profile crop of the hardware raster
- collision detection accumulates from drawn objects per hardware/MAME
- no per-title raster formula, `regionoff`, `palRenderOffset`, `snapline`,
  `mxsnap`, `/20`, `/22`, `/24` or CRC branch

---

## F. Memory and mapper design

```cpp
class MachineBus {
public:
    uint8_t ProgramRead(uint16_t addr);
    uint8_t DataRead(uint16_t addr);
    void    DataWrite(uint16_t addr, uint8_t value);
    uint8_t IoRead(uint8_t port);
    void    IoWrite(uint8_t port, uint8_t value);
    uint8_t BusRead();
    void    BusWrite(uint8_t value);
    bool    IrqLine() const;
    bool    T0() const; bool T1() const;
};
```

Memory map:

- 1 KiB BIOS overlay at the low program window
- cartridge program space in 1 KiB / 2 KiB / 3 KiB / 4 KiB and larger banks
- external RAM and open-bus behavior
- P1/P2-dependent address decode
- MegaCART banking, XROM, 3K/4K images
- clean cartridge interfaces for special hardware

Special cartridges implement `CartridgeDevice` (C7010 later); they do not add
branches to the base CPU or VDC. CRC is used for identity/selection only, never
to change cycle timing.

---

## G. G7400 / VPP interface design

```cpp
class VppDevice {
public:
    virtual ~VppDevice() = default;
    virtual void Reset() = 0;
    virtual void AdvanceTo(Tick) = 0;
    virtual void WriteControl(uint8_t port, uint8_t value) = 0;
    virtual uint8_t ReadControl(uint8_t port) = 0;
    virtual void WriteMemory(uint16_t addr, uint8_t value) = 0;
    virtual uint8_t ReadMemory(uint16_t addr) = 0;
    virtual void Compose(Framebuffer&, Tick) = 0;
    virtual VppSnapshot Snapshot() const = 0;
};
```

The VPP is a first-class device, not a post-process in the legacy VDC stub:

- EF9340/EF9341 register and memory model
- dynamic character set
- layer rebuild and transparency/luminance
- composition against the current VDC pixel at the same timestamp
- explicit VBL/VPP timing relationship from the machine scheduler
- no accidental legacy `gDirty`/`gLatchedValid` semantics

The existing `vpp_cset` data may be reused only after provenance review; the
new device gets its own state and composition contract.

---

## H. Legacy components that can be reused safely

| Component | Reuse |
|---|---|
| `src/frontend/*`, `main.cpp`, `launcher.*` | keep; adapt the launch boundary |
| library/database/collection/media/settings | keep unchanged |
| SDL presentation, texture, input mapping | keep behind `MachineHost` |
| `src/mcs48/trace_record.h` | reuse as diagnostics (POD ring) |
| `src/mcs48/input_schedule.h` | reuse for deterministic input timestamps |
| `src/mcs48/legacy_observation.h` | reuse only as an optional observer |
| `crc32.cpp` | identity/selection only |
| `vpp_cset.cpp` data | reuse only after license/provenance review |
| `audio_sdl.cpp` | temporary adapter behind `AudioDevice` |
| `c7010.cpp` | later special-cartridge device, not base core |
| `tests/compat` inventory/matrix scripts | extend with an NG column |
| `cset.cpp` / character tables | candidate data reuse after verification |

---

## I. Legacy components that must be replaced

- `cpu.cpp` — production CPU
- `vmachine.cpp` — frame/VBL/IRQ/clocks/mapper/register globals
- `vdc_stub.cpp` — VDC register/raster/renderer/collision/readback
- `vpp_sdl.cpp` — legacy VPP composition path
- `emulator_core.cpp` boot/run path for NG (legacy path retained)
- `setvideomode` CRC rules and `evblclk`/`fps` coupling
- `master_clk`, `h_clk`, `LINECNT`, `VBLCLK`, `EVBLCLK_*`
- `/20`, `/22`, `/24` raster scales
- `palRenderOffset`, `regionoff`, `snapline`, `mxsnap`
- `pendirq`, `enahirq`, `useforen`, `sproff`, `tweakedaudio` compatibility
- per-title `do_kluges` raster/timing offsets
- pulse IRQ behavior and synthetic `int_clk`
- ad-hoc VDC readback, coltab collision path and sprite offsets
- global-state save/load as a deterministic checkpoint

They remain in the tree, frozen, for A/B reference until NG is promoted.

---

## J. Migration sequence

### NG-0 — Architecture (this checkpoint)
Documentation and inert interfaces only. No build/production change.

Gate: architecture review and approval.

### NG-1 — CPU
Implement `Mcs48Cpu` + `Mcs48Bus` stub. Compile a standalone
`tests/mcs48_ng` target. Flip the 11 legacy expected failures to NG pass
expectations; add full-opcode, boundary, IRQ, timer and pin tests. Compare
small deterministic programs against MAME traces. No games.

Gate: all valid CPU conformance cases pass; no shared globals.

### NG-2 — Machine + memory
Connect CPU, BIOS, cartridge, memory map, P1/P2, mapper and scheduler.
Run deterministic ROM execution and bus replay. No full video required.

Gate: timestamped bus traces match the reference harness for deterministic
programs; banking and open-bus tests pass.

### NG-3 — i8244/i8245 VDC
Implement register map, status/collision, beam timing, scanline rendering,
update-on-write, PAL/NTSC profiles. Bring up the framebuffer.

Gate: register replay, A1/A2/A3/A4/A5 tests, frame geometry and collision
fixtures pass; no per-title offsets.

### NG-4 — Full G7000
Run the broad VP01–VP65 differential matrix against MAME and legacy. Fix
machine semantics, never titles.

Gate: NG converges broadly on MAME/hardware; regressions are explained by
machine behavior, not title patches.

### NG-5 — G7400 / VPP
Implement EF9340/EF9341/VPP cleanly. Use VP60, VP59+ and other `+` titles as
system probes.

Gate: VPP register/memory/composition tests and the `+` matrix pass without
game-specific branches.

### NG-6 — C7010 / special hardware
Integrate the NSC800 module, keyboard, inter-CPU communication and rendering
through `CartridgeDevice`. Keep it out of the base G7000 core.

Gate: C7010 gameplay and state tests pass with no base-core timing changes.

### NG-7 — Audio and polish
Wire the existing audio engine behind `AudioDevice`; later replace with the
hardware-timed 8245 model. Snapshot/restore, deterministic replay, frontend
lifecycle and performance.

### NG-8 — Default switch
`O2EM_CORE=ng` becomes the default only after full matrix parity and human QA.
Legacy remains selectable for one release, then archived.

---

## K. Legacy and NG side by side

Development selector:

```text
O2EM_CORE=legacy   (default)
O2EM_CORE=ng       (development only until promotion)
```

Implementation:

- `EmulatorCore_StartRom` remains the single frontend entry point.
- A small `IMachineCore` adapter dispatches to the legacy path or
  `o2em::ng::Machine`.
- Both adapters present frames/audio through the same `MachineHost`.
- Both can be built into the same executable during migration.
- The NG core must not link legacy globals; the adapters are the only boundary.
- The compatibility runner gains a `-Variant ng` option and an `ng` column.
- Default remains legacy; no user-facing compatibility switch is advertised
  before NG promotion.

---

## L. Test strategy

1. **CPU conformance** — full opcode table, cycle counts, PC/stack/PSW/bank,
   IRQ entry/RETR, timer/counter, T0/T1, BUS/MOVX, reset, illegal policy.
2. **Property/boundary tests** — PC 0x7FF/0xFFF, stack wrap, bank selection,
   timer prescaler edges, interrupt while disabled.
3. **Bus/mapper tests** — BIOS overlay, 2K/3K/4K/8K/16K/32K, MegaCART, open
   bus, external RAM, P1/P2 decode.
4. **VDC tests** — register masks/readback, status latch and read-clear,
   collision, PAL/NTSC geometry, update-on-write, scanline boundaries.
5. **VPP tests** — register/memory writes, dynamic chars, transparency,
   composition order, VBL relationship.
6. **Differential lab** — MAME vs legacy vs NG at identical frames/inputs;
   screenshots, geometry metrics, compact state snapshots.
7. **Determinism** — mid-frame save/restore re-runs byte-identically.
8. **No title acceptance** — titles are probes; a title-specific patch fails
   review.

---

## M. MAME / hardware reference strategy

- Use MAME 0.289 source for i8244/i8245, MCS48 and EF934x behavior.
- Run MAME `videopac` for G7000 references.
- Complete a legal MAME G7400 reference environment (BIOS + EF9340 charset)
  where possible. VP60 is confirmed working by Bengt-Ove in MAME; do not mark
  it UNKNOWN again.
- For every subsystem, record representative titles, BIOS, region, input,
  checkpoints and hashes in `tests/compat/reference_manifest` (new).
- Hardware documentation (Intel 8048/8244/8245, Philips/Thomson EF9340/9341)
  outranks MAME when they disagree.
- Architecture decisions must not rest on UNKNOWN reference cases.

---

## N. Licensing and provenance

- MAME is BSD-3-Clause. It is a behavioral/source reference. Do not copy code
  without preserving copyright/license notices and documenting the exact
  source file/commit.
- Prefer independent implementation from hardware documentation plus MAME
  behavior.
- O2EM-derived tables/code (e.g. `vpp_cset`) require a license/provenance
  review before entering the new core. If provenance is unclear, rederive from
  hardware/EF934x documentation.
- Maintain `Docs/Development/NG-0_Provenance.md` (first file list) with every
  reused table/file, origin, license and transformation.
- No code of uncertain provenance enters `src/core_ng/` silently.

---

## O. First files to create

Documentation:

```text
Docs/Development/NG-0_Architecture_2026-11-09.md   (this file)
Docs/Development/NG-0_Provenance.md                (provenance register)
```

Interfaces (inert, not added to the production project yet):

```text
src/core_ng/README.md
src/core_ng/machine.h
src/core_ng/machine_config.h
src/core_ng/timebase.h
src/core_ng/scheduler.h
src/core_ng/scheduler_event.h
src/core_ng/state.h
src/core_ng/cpu/mcs48_ng.h
src/core_ng/cpu/mcs48_bus.h
src/core_ng/cpu/mcs48_state.h
src/core_ng/memory/bus.h
src/core_ng/memory/cartridge.h
src/core_ng/memory/mapper.h
src/core_ng/video/vdc.h
src/core_ng/video/i8244.h
src/core_ng/video/i8245.h
src/core_ng/video/vdc_state.h
src/core_ng/video/framebuffer.h
src/core_ng/video/vpp.h
src/core_ng/input/input.h
src/core_ng/audio/audio_device.h
src/core_ng/diagnostics/trace.h
```

NG-0 does not add these to `O2EM-NG.vcxproj`. They are a contract, not a
production path.

---

## P. First code milestone after architecture approval

**NG-1a: standalone MCS48-NG CPU conformance target.**

Deliverable:

```text
tests/mcs48_ng/mcs48_ng_conformance.cpp
tests/mcs48_ng/mcs48_ng_stub_bus.cpp
tests/mcs48_ng/mcs48_ng_conformance.vcxproj
src/core_ng/cpu/mcs48_ng.cpp
src/core_ng/cpu/mcs48_state.h
src/core_ng/cpu/mcs48_bus.h
```

Acceptance:

- MCS48-NG compiles as an owned-state CPU with zero legacy globals.
- All 11 former `EXPECTED_LEGACY_FAILURE` cases pass under NG expectations.
- All 5 existing positive controls pass.
- Full legal-opcode smoke suite passes with exact cycle counts.
- Deterministic instruction traces match MAME for a small fixture set.
- No game runs yet; no VDC, VPP, frontend or legacy file is modified.

Only after NG-1a passes do we connect the machine bus in NG-2.

---

## Acceptance checklist for NG-0

- [x] New-core directory/module architecture defined.
- [x] Frontend ↔ machine API defined.
- [x] One authoritative machine timebase defined.
- [x] MCS48-NG production CPU design defined.
- [x] i8244/i8245 device design defined.
- [x] Memory/mapper design defined.
- [x] G7400/VPP interface defined.
- [x] Safe legacy reuse and replacement lists defined.
- [x] Migration sequence and gates defined.
- [x] Legacy/NG side-by-side selector defined.
- [x] Test strategy defined.
- [x] MAME/hardware reference strategy defined.
- [x] Licensing/provenance rules defined.
- [x] First files and first code milestone defined.
- [x] Production behavior unchanged in NG-0.
