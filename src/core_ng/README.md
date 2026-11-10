# O2EM-NG new machine core (`core_ng`)

Status: NG-0 architecture, NG-1 MCS48-NG CPU and NG-2 deterministic Philips
machine are complete. NG-2 is green in Debug and Release x64. NG-3 (real
i8244/i8245 rendering) has not started.

```text
frontend / SDL / database
        |
        v
   o2em::ng::Machine          (machine.h / machine.cpp)
        |
        +-- MCS48-NG CPU       (cpu/)
        +-- scheduler/timebase (scheduler.*, timebase.h)
        +-- MachineBus         (memory/bus.*)
        +-- BIOS/cart/mapper   (memory/)
        +-- StubVdc            (video/stub_vdc.*, NG-2 only)
        +-- input              (input/)
        +-- bus trace          (diagnostics/)
        +-- snapshot           (state.h)
```

Rules:

- One `Machine` owns all mutable state. No emulation globals are shared.
- No SDL, frontend, database or cartridge-filename knowledge in devices.
- One authoritative machine time; no subsystem-local line/clocks.
- 12 NG ticks == 1 MCS-48 machine cycle; PAL/NTSC are profiles, not paths.
- No per-title timing branches, CRC raster compensation, IRQ suppression or
  fixed delays.
- Legacy `cpu.cpp` / `vmachine.cpp` / `vdc_stub.cpp` / `vpp_sdl.cpp` stay
  frozen as A/B reference. New correctness work goes here.

Tests:

```text
tests/mcs48_ng/    NG-1 CPU conformance (667 checks)
tests/machine_ng/  NG-2 machine conformance (327 checks)
```

See `Docs/Development/NG-2_Machine_2026-11-09.md`.
