#pragma once
// Development-only NG machine host.
//
// This lets the EXISTING O2EM-NG application launch the new NG G7000 machine
// with the existing user controls. It is gated by O2EM_CORE=ng and does not
// change any binding, frontend, library or settings behaviour. The adapter in
// core_ng/input/adapter.h is the only input boundary.

#include <string>

namespace o2em::ng::host {

// Run the new machine until the user quits. biosPath/cartPath are the same
// files the legacy core would load. Returns false on load/setup failure.
bool RunSdl(const std::string& cartPath, const std::string& biosPath,
            bool pal);

} // namespace o2em::ng::host
