0030S - instruction interleave
Distributes the previous 10000 NSC800 instructions per frame across 8048 cycles.
No additional frame-end CPU burst. Fractional budget is retained between instructions.
This remains approximate instruction timing, not exact NSC800 T-states.
0030R bus fix and 0030Q communication tracing retained.
Test ENTER and capture 0030Q COMM BEGIN through END; check whether 2F is read before FF.
