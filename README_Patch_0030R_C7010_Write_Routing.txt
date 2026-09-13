0030R - C7010 write routing
P16 high blocks cartridge writes (P1=E6, A0=A8).
VDC writes remain delivered even when C7010 also accepts a write.
P1=AE / C0 writes remain accepted. P10/P14 selection unchanged.
0030Q diagnostics retained. CPU scheduling unchanged.
Test board labels and ENTER; capture 0030Q BEGIN through END.
Reference: MAME src/mame/philips/odyssey2.cpp io_write.
