#pragma once

#include <string>
#include "settings.h"

bool EmulatorCore_StartRom(const std::string& romPath, RegionMode regionMode, const std::string& biosFile, bool scanlines);