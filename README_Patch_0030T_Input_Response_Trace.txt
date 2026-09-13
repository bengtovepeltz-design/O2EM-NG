0030T - input and response-path diagnostics
Retains 0030R bus correction and 0030S scheduling; no behavior changes.
INPUT logs non-FF writes and first reads of each generation from reset (512 lines).
DECISION logs NSC800 checkpoints 14A7, 1A11, 1AAB and 1759 (32 lines).
FF58/FF59/FF5A are raw firmware RAM bytes; their meaning is not assumed.
At 1A11 firmware conditionally jumps to 1759 when FF59 equals 4F.
1759 sends 63, waits for F7, then sends FF.
Capture from startup through move entry and 0030Q COMM END.
