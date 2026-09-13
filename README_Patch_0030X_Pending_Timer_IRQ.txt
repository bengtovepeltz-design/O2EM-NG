0030X - pending timer/counter interrupt correction
An enabled overflow during an active interrupt sets tirq_pend instead of being lost.
The existing pending check delivers it after RETR; interrupts are not nested.
DIS TCNTI still clears pending state. Existing legacy disabled-IRQ handling retained.
Applies to the shared 8048 core; ordinary games also need regression testing.
0030R/S fixes and diagnostics retained. Test lower board rows and E2-E4, C2-C4.
