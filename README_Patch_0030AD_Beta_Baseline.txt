0030AD - beta baseline based on user-tested 0030AC
Retains JR displacement fix, raster scale and renderer character snapshot.
Detailed C7010/NSC800/ENTER/video tracing defaults off in Debug and Release.
Build with O2EM_C7010_TRACE=1 to opt in again. Detection, firmware failures,
unsupported instructions and HALT reports remain visible.
No CPU execution budget, latch routing, move handling or video behavior changed.
User testing: several moves, pieces retained, flicker absent with 0030AC.
Not yet validated: complete games, castling, en passant, promotion, all levels.
Beta reports should include build, console/BIOS choice, start choices and moves.
