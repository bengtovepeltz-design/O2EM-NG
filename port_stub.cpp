/*
    O2EM-NG
    Patch 0029A Fix 1

    read_PB() and write_PB() are implemented by vpp_sdl.cpp.

    This file is intentionally left without those function definitions.
    Keeping the old placeholder implementations here caused LNK2005
    duplicate-symbol errors when the Videopac+ core was restored.
*/
