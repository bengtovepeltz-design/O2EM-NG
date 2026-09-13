#pragma once

// Patch 0027A transition switch.
// 0 = verified Patch 0026H audio path (default)
// 1 = new Audio8245 core connected to the existing SDL/ring-buffer path (development)
#define O2EM_USE_AUDIO8245 0
