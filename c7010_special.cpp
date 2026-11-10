#include "c7010_special.h"

#include "c7010.h"
#include "types.h"

#include <cstdio>
#include <cstdlib>

namespace {

bool RuntimeTraceEnabled() {
    static int enabled = -1;
    if (enabled < 0) {
        const char* env = std::getenv("O2EM_C7010_RUNTIME_TRACE");
        enabled = (env && *env && *env != '0') ? 1 : 0;
    }
    return enabled == 1;
}

void TraceLines(const char* text, unsigned a, unsigned b) {
    static unsigned lines = 0;
    if (!RuntimeTraceEnabled() || lines >= 80)
        return;
    ++lines;
    std::fprintf(stderr, text, a, b);
    if (lines == 80)
        std::fprintf(stderr, "C7010_RUNTIME trace limit reached\n");
}

} // namespace

void C7010SpecialHardware::PowerOn() {
    C7010_Reset();
    C7010_WriteP1(0xFF);
}

void C7010SpecialHardware::Reset() {
    C7010_Reset();
    C7010_WriteP1(0xFF);
}

void C7010SpecialHardware::WriteP1(std::uint8_t value) {
    C7010_WriteP1(static_cast<Byte>(value));
}

bool C7010SpecialHardware::ExternalRead(std::uint8_t address,
                                        std::uint8_t& value) {
    Byte device_value = 0xFF;
    if (!C7010_ExternalRead(static_cast<ADDRESS>(address), device_value))
        return false;
    value = static_cast<std::uint8_t>(device_value);
    if (value != 0xFF)
        TraceLines("C7010_EREAD addr=%02X value=%02X\n",
                   static_cast<unsigned>(address),
                   static_cast<unsigned>(value));
    return true;
}

bool C7010SpecialHardware::ExternalWrite(std::uint8_t address,
                                         std::uint8_t value) {
    const bool handled = C7010_ExternalWrite(static_cast<ADDRESS>(address),
                                             static_cast<Byte>(value));
    if (handled && value != 0xFF)
        TraceLines("C7010_EWRITE addr=%02X value=%02X\n",
                   static_cast<unsigned>(address),
                   static_cast<unsigned>(value));
    return handled;
}

void C7010SpecialHardware::RunCpuCycles(unsigned cycles,
                                        unsigned frame_cycles) {
    C7010_Run8048Cycles(cycles, frame_cycles);
}

void C7010SpecialHardware::FrameTick() {
    C7010_RunDiagnosticStep();
}
