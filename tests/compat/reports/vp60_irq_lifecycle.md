# VP60 VBL IRQ lifecycle trace

## 1. Current persistent-line implementation

Trace at the frame-642 early vector (production path, `O2EM_IRQ_TRACE=1`):

```text
frame 641, mclk 6153  IRQ_ASSERT  pc=4BB  prior=1  xirq_en=0  irq_ex=0
frame 642, mclk 5244  IRQ_EN      pc=178  irqLine=1
frame 642, mclk 5245  IRQ_VEC     pc_before=178  A=48  PSW=18  P1=9E  P2=E0
frame 642, mclk 5266  A1_READ     pc=012  p1=B6  p2=E0  irqLine=1  d=01
frame 642, mclk 5266  IRQ_ACK     pc=012  irqLine=1  xirq_en=1  irq_ex=1
frame 642, mclk 5604  IRQ_DIS     pc=601  irqLine=0  irq_ex=2
frame 642, mclk 5703  IRQ_EN      pc=636  irqLine=0  irq_ex=2
frame 642, mclk 6154  IRQ_ASSERT  pc=17A  prior=0  xirq_en=1  irq_ex=0
frame 642, mclk 6154  IRQ_VEC     pc_before=17A
frame 642, mclk 6175  A1_READ     pc=012  p1=B6  p2=E0  irqLine=1  d=08
frame 642, mclk 6175  IRQ_ACK
```

Key facts:

- The VBL at frame 641/mclk 6153 asserts the line while `xirq_en=0`.
- The game does **not** read A1 between that VBL and the frame-642 `EN I`.
- At frame 642/mclk 5244, `EN I` (PC=0x178) makes the held line eligible.
- The next pre-fetch takes the vector at mclk 5245.
- The early ISR reads A1 at PC=0x012; before the latch fix it returned `d=01`
  (no VBL bit), after the fix it returns `d=08` (VBL latched).
- The A1 read does call `CPU_IrqLineAck()`; the line is cleared at mclk 5266.
  Therefore this is **not** case A or B: the ACK is not missed.

The game's main loop is the BIOS F1 handshake:

```text
0x176  CLR F1
0x177  EN I
0x178  JF1 0x175   ; return if F1 set by the VBL handler
0x17A  JMP 0x178
```

The early vector sets F1 before the real VBL, so the handshake exits early.
That is the first player-visible divergence; the later `A3=06` write at frame
971 is skipped and the gameplay background stays black.

## 2. Historical pulse path

Same frames with `O2EM_I8244_LEGACY_VBL_IRQ=1`:

```text
frame 642, mclk 5245  IRQ_EN   pc=178  irqLine=0
frame 642, mclk 5604  IRQ_DIS
frame 642, mclk 5703  IRQ_EN
frame 642, mclk 6154  IRQ_VEC  pc_before=178
frame 642, mclk 6175  A1_READ  irqLine=0  d=08
```

The `EN I` at 5245 occurs at exactly the same cycle, but the line is 0 because
the historical `handle_vbl()` called `ext_IRQ()` as a 5-cycle pulse. If
`xirq_en` was 0 at the VBL, `ext_IRQ()` did not enter the ISR and the pulse
expired. No early vector.

## 3. MAME 0.289 i8244 semantics

From local `i8244.cpp`:

```cpp
TIMER_CALLBACK_MEMBER(i8244_device::vblank_start)
{
    m_control_status |= 0x08;
    m_irq_func(ASSERT_LINE);      // hold /INT
}
```

```cpp
case 0xa1:
    data = m_control_status;      // latched VBL bit
    ...
    m_irq_func(CLEAR_LINE);       // clear on A1 read
    m_control_status &= ~0xcc;
```

The i8244 asserts a **level**, keeps the VBL status latched, and clears both
only on an A1 read. There is no automatic clear at the end of VBL.

## 4. MAME 0.289 MCS48 semantics

From `mcs48.cpp` (`mame0289`):

```cpp
void mcs48_cpu_device::check_irqs()
{
    if (m_irq_in_progress)
        return;
    else if ((m_irq_state || (m_sts & STS_IBF) != 0) && m_xirq_enabled)
    {
        burn_cycles(2);
        m_irq_in_progress = true;
        execute_call(0x03);
    }
    ...
}
```

```cpp
void mcs48_cpu_device::execute_run()
{
    do
    {
        check_irqs();
        m_irq_polled = false;
        m_prevpc = m_pc;
        unsigned opcode = opcode_fetch();
        (this->*m_opcode_table[opcode])();
    } while (m_icount > 0);
}
```

`EN I` sets `m_xirq_enabled = true`; `DIS I` clears it; `RETR` clears
`m_irq_in_progress`. `execute_set_input(MCS48_INPUT_IRQ, state)` sets
`m_irq_state` directly from the level.

Consequence: MAME would see the held `m_irq_state=1` after `EN I` at the very
next `check_irqs()` and take the vector at 0x03. **MAME does not ignore a
pending VBL until the next VBL event.** The historical O2EM pulse is the
non-hardware behavior in this specific respect.

## 5. Classification

**E — MAME/hardware would service the held line immediately; historical GOOD
was depending on the inaccurate pulse behavior.**

Not A: the A1 read does call `CPU_IrqLineAck()`.
Not B: the A1 read is reached (trace `pc=012`, `p1=B6`) and the ack runs.
Not C in the timing sense: the level is re-asserted at the next VBL; the prior
assert was never ACKed because the game was in `DIS I`.
Not D in the sense of the enable/disable sampling: MAME's `check_irqs` also
services immediately after `EN I`.
Not F: the trace is the VBL line, not the timer/sound line.

The current tree was missing the **latched A1 VBL status bit**, which is a real
MAME mismatch: O2EM returned `d=01` at the early ISR, while MAME returns the
latched `0x08`. That has been fixed generically:

- `CPU_VBL_IRQ()` and `handle_vbl()` set `g_vblStatus`.
- The A1 read returns bit 0x08 from `g_vblStatus`.
- `CPU_IrqLineAck()` clears `g_vblStatus`.

After that fix the early ISR sees `d=08`, matching MAME. **VP60 still fails**,
because the early vector still runs the game's VBL handler early and completes
the F1 handshake before the real VBL. The A1 latch alone is not sufficient.

## 6. What this means for the fix

The smallest generic change that is unambiguously hardware-correct is the A1
VBL status latch (implemented). It is library-neutral: the full legacy matrix
is unchanged at 18 GREEN / 39 YELLOW / 9 ORANGE / 19 UNKNOWN, and the unified
matrix remains 17 / 40 / 9 / 19.

It does not restore VP60. The remaining failure is the early VBL handler
executing before the VBL beam, which the historical pulse avoided by dropping
the request while `DIS I` was active. A rule that ignores a pending IRQ until
the next VBL would make VP60 work, but **MAME 0.289 does not implement such a
rule**, so it is not accepted as a hardware-correct fix without further
evidence.

To settle the remaining question, a MAME G7400 run of VP60 is required. The
local MAME install lacks the `ef9340_1` charset ROM, so `videopacp` cannot
start. Until that reference is available, the legacy VBL pulse switch remains
the only proven-working VP60 control and must stay opt-in
(`O2EM_I8244_LEGACY_VBL_IRQ=1`), not the default.

## 7. Current state

- A1 VBL status latch: implemented, matches MAME i8244 `m_control_status`.
- VBL IRQ lifecycle traces: opt-in via `O2EM_IRQ_TRACE=1`
  (`IRQ_ASSERT`, `IRQ_ACK`, `IRQ_EN`, `IRQ_DIS`, `IRQ_VEC`, `IRQ_RETR`,
  `A1_READ`).
- Legacy pulse A/B control: `O2EM_I8244_LEGACY_VBL_IRQ=1`.
- VP60 default: still black (held line, correct per MAME source).
- VP60 + legacy pulse: full gameplay, frame 1200 = 4455 bytes.
- VP48: GREEN, unchanged.
- C7010: full board, unchanged (frame 700 = 5743 bytes).
- Full legacy matrix: unchanged 18/39/9/19.
- Full unified matrix: unchanged 17/40/9/19.

## 8. Early-ISR instruction trace (frame 642)

Opt-in trace: `O2EM_ISR_TRACE=1` (plus `O2EM_IRQ_TRACE=1`).

### Common entry path

Both the early and real-VBL ISRs follow the same BIOS path:

```text
vector 0x003 -> 0x402 -> 0x009 -> A1 read -> 0x018 -> 0x406 -> 0x657
```

The A1 read returns `d=08` in both cases after the VBL latch fix. A2 returns
`d=00` in both. No VDC write occurs before the first divergence.

### Early vs real-VBL state

```text
early entry: mclk 5248  A=48 PSW=19 P1=9E P2=E0
             R=01,31,EE,00,01,14,A4,48
real entry:  mclk 6153  A=34 PSW=D9 P1=9E P2=E0
             R=25,17,00,00,44,27,89,00
```

### First divergence

At PC `0x663` (`MOV A,@R0`, R0=`0x38`):

```text
early: RAM[0x38] = 0x01  -> A=0x01
real:  RAM[0x38] = 0x0C  -> A=0x0C
```

At PC `0x664` (`DEC A`):

```text
early: A=0x00
real:  A=0x0B
```

At PC `0x665` (`JZ`, opcode `C6`), the zero flag diverges:

```text
early: A=0x00 -> Z=1 -> jump 0x66A (frame-counter wrap path)
real:  A=0x0B -> Z=0 -> fall through 0x667 -> 0x01A (normal path)
```

This is the first PC/control-flow divergence. It is a **cartridge RAM state
difference**, not a VDC/I/O read difference: the A1 and A2 values are already
correct and identical before this point.

### Consequence

The early handler runs the wrap path while the real VBL handler runs the normal
path. The later `A3=06` write is downstream of this branch and is skipped.
The historical pulse path never runs the early handler, so the real VBL handler
always sees RAM[0x38]=0x0C.

### What this means

The remaining mismatch is **frame-relative CPU/main-loop state**: the early
ISR is entered before the cartridge's frame counter has reached the value the
handler expects. The A1 latch is correct and should stay. The timer-cadence
experiment (`O2EM_MCS48_TIMER_32=1`) did not change VP60 and was removed.

The exact reason MAME/hardware reaches the handler with RAM[0x38]=0x0C at the
early point is not proven from source alone. It requires a MAME G7400 run of
VP60, which is blocked by the missing `ef9340_1` charset ROM. Until that
reference exists, the legacy VBL pulse control remains the only proven-working
VP60 path and stays opt-in; it is not promoted.
