/*
    O2EM-NG - Videopac+ G7400 VPP/EF934x compatibility core
    Patch 0029C

    This is an SDL-independent rewrite of the legacy O2EM VPP register and
    display state.  It restores the G7400 data path that was previously
    replaced by vpp_stub.cpp.  Rendering is composited into O2EM-NG's existing
    indexed framebuffer; no Allegro dependency is required.
*/

#include "vpp.h"
#include <cstddef>
#include <cstdio>
#include "vmachine.h"
#include "vpp_cset.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{
constexpr int kCols = 40;
constexpr int kRows = 32;
constexpr int kVisibleRows = 25;
constexpr int kGlyphW = 8;
constexpr int kGlyphH = 10;
constexpr int kLayerW = kCols * kGlyphW;
constexpr int kLayerH = kVisibleRows * kGlyphH;

Byte gLum = 0xFF;
Byte gTransparency = 0xFF;
Byte gData = 0;
Byte gMode = 0;
int gCursorX = 0;
int gCursorY = 0;
int gY0 = 0;
int gSlice = 0;
bool gSliceMode = false;
bool gIncCursor = true;
bool gDirty = true;
int gBlinkCounter = 0;
bool gBlink = false;

Byte gMem[kCols][kRows][4]{};
Byte gDynamicChars[2][960]{};
std::array<Byte, kLayerW * kLayerH> gLayer{};

int remapColour3(int value)
{
    value &= 7;
    return (value & 2) | ((value & 1) << 2) | ((value & 4) >> 2);
}

int luminanceBit(int colour)
{
    static constexpr Byte mask[8] = { 0x01, 0x10, 0x04, 0x40, 0x02, 0x20, 0x08, 0x80 };
    return (gLum & mask[colour & 7]) ? 0 : 8;
}

bool underlyingColourRevealsVpp(int colour)
{
    // Legacy O2EM / EF934x semantics:
    // TraReg controls which *underlying 8244 colour* is transparent and
    // therefore lets the Videopac+ layer show through.
    static constexpr Byte mask[8] = { 0x01, 0x10, 0x04, 0x40, 0x02, 0x20, 0x08, 0x80 };
    return (gTransparency & mask[colour & 7]) == 0;
}

void advanceCursor()
{
    if (!gIncCursor)
        return;
    if (++gCursorX >= kCols)
    {
        gCursorX = 0;
        if (++gCursorY >= 24)
            gCursorY = 0;
    }
}

Byte reverseBits(Byte value)
{
    value = static_cast<Byte>(((value & 0xF0) >> 4) | ((value & 0x0F) << 4));
    value = static_cast<Byte>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
    value = static_cast<Byte>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
    return value;
}

Byte glyphRow(Byte ch, bool ext, int row)
{
    row = std::clamp(row, 0, 9);

    if (ch >= 0xA0)
    {
        const int index = (ch - 0xA0) * 10 + row;
        return gDynamicChars[ext ? 1 : 0][index];
    }

    if (ch >= 0x80)
        return 0xFF;

    // EF9340/EF9341 has its own 10-scanline character generator.
    // This table is taken from the working O2EM 1.20/1.21 VPP core.
    return vpp_cset[ext ? 1 : 0][static_cast<int>(ch) * 10 + row];
}

void drawGlyph(int cx, int cy, Byte ch, int background, int foreground,
               bool ext, int doubleWidth, int doubleHeight, bool underline)
{
    if (cx < 0 || cx >= kCols || cy < 0 || cy >= kVisibleRows)
        return;

    const int x0 = cx * kGlyphW;
    const int y0 = cy * kGlyphH;

    for (int py = 0; py < kGlyphH; ++py)
    {
        int srcY = py;
        if (doubleHeight)
            srcY = doubleHeight == 2 ? 5 + py / 2 : py / 2;
        srcY = std::clamp(srcY, 0, 9);

        Byte bits = (underline && srcY == 9) ? 0xFF : glyphRow(ch, ext, srcY);

        for (int px = 0; px < kGlyphW; ++px)
        {
            int srcX = px;
            if (doubleWidth)
                srcX = doubleWidth == 2 ? 4 + px / 2 : px / 2;
            srcX = std::clamp(srcX, 0, 7);

            const bool on = (bits & (0x80 >> srcX)) != 0;
            gLayer[(y0 + py) * kLayerW + (x0 + px)] =
                static_cast<Byte>(on ? foreground : background);
        }
    }
}

void rebuildLayer()
{
    std::fill(gLayer.begin(), gLayer.end(), 0);

    bool verticalParity = false;
    bool doubleHeightLatch = false;

    for (int y = 0; y < kVisibleRows; ++y)
    {
        // Mark's original VPP core latches double-height once encountered.
        // From that point the row parity alternates for subsequent rows.
        verticalParity = doubleHeightLatch ? !verticalParity : false;
        const int memY = y == 0 ? 31 : (y - 1 + gY0) % 24;

        int background = 0;
        bool underline = false;
        bool conceal = false;
        bool boxed = false;
        bool horizontalParity = false;
        bool doubleWidthLatch = false;
        int dw = 0;
        int dh = 0;

        for (int x = 0; x < kCols; ++x)
        {
            // Same latch behaviour as O2EM 1.20 vpp.c: after a double-width
            // cell appears, horizontal parity keeps alternating across the row.
            horizontalParity = doubleWidthLatch ? !horizontalParity : false;

            const Byte ch = gMem[x][memY][0];
            const Byte attr = gMem[x][memY][1];
            const Byte serialCh = gMem[x][memY][2];
            const Byte serialAttr = gMem[x][memY][3];

            int foreground = remapColour3(attr & 7);
            const bool ext = (attr & 0x80) != 0;

            if (serialCh)
            {
                background = remapColour3((serialAttr >> 4) & 7);
                underline = (serialCh & 4) != 0;
                conceal = (serialCh & 1) != 0;
                boxed = (serialCh & 2) != 0;
            }

            // Preserve the original EF934x serial-attribute state machine.
            // Serial control cells inherit the current size state. Extended
            // characters explicitly cancel double width/height.
            if (!serialCh)
            {
                if (ext)
                {
                    background = remapColour3((attr >> 4) & 7);
                    dw = 0;
                    dh = 0;
                }
                else
                {
                    dw = (attr & 0x20) ? (horizontalParity ? 2 : 1) : 0;
                    dh = (attr & 0x10) ? (verticalParity ? 2 : 1) : 0;
                    if (dw) doubleWidthLatch = true;
                    if (dh) doubleHeightLatch = true;
                }
            }

            bool invert = false;
            if (x == gCursorX && memY == gCursorY && (gMode & 0x10))
            {
                invert = !invert;
                if ((gMode & 0x80) && gBlink)
                    invert = !invert;
            }
            if (!ext && (attr & 0x40))
                invert = !invert;

            int bg = background | luminanceBit(background);
            int fg = foreground | luminanceBit(foreground);

            if ((gMode & 0x80) && !(attr & 8) && !gBlink)
            {
                if (!(gMode & 0x10) || x != gCursorX || memY != gCursorY)
                    fg = bg;
            }

            const bool rowEnabled = (y == 0) ? ((gMode & 8) != 0) : ((gMode & 1) != 0);
            const bool visible = rowEnabled && (!conceal || !(gMode & 4)) && (boxed || !(gMode & 2));

            if (visible)
            {
                int sourceX = x - (dw > 0 ? (dw - 1) : 0);
                sourceX = std::clamp(sourceX, 0, kCols - 1);
                const Byte drawCh = gMem[sourceX][memY][0];

                if (invert)
                    drawGlyph(x, y, drawCh, fg, bg, ext, dw, dh, underline);
                else
                    drawGlyph(x, y, drawCh, bg, fg, ext, dw, dh, underline);
            }
        }
    }

    // Global double-height mode: keep top cell row fixed, stretch the rest.
    if (gMode & 0x20)
    {
        auto copy = gLayer;
        for (int y = kLayerH - 1; y >= 10; --y)
        {
            const int srcY = (y - 10) / 2 + 10;
            std::memcpy(&gLayer[y * kLayerW], &copy[srcY * kLayerW], kLayerW);
        }
    }

    gDirty = false;
}
}

Byte read_PB(Byte p)
{
    switch (p & 3)
    {
    case 0: return gLum >> 4;
    case 1: return gLum & 0x0F;
    case 2: return gTransparency >> 4;
    default: return gTransparency & 0x0F;
    }
}

void write_PB(Byte p, Byte val)
{
    val &= 0x0F;
    switch (p & 3)
    {
    case 0: gLum = static_cast<Byte>((val << 4) | (gLum & 0x0F)); break;
    case 1: gLum = static_cast<Byte>((gLum & 0xF0) | val); break;
    case 2: gTransparency = static_cast<Byte>((val << 4) | (gTransparency & 0x0F)); break;
    case 3: gTransparency = static_cast<Byte>((gTransparency & 0xF0) | val); break;
    }
    gDirty = true;
}

Byte vpp_read(ADDRESS adr)
{
    static Byte first = 0;
    static Byte second = 0;

    switch (adr & 0xFF)
    {
    case 4:
        return first;
    case 5:
    {
        const Byte result = second;
        if (gSliceMode)
        {
            const Byte ch = gMem[gCursorX][gCursorY][0];
            const bool ext = (gMem[gCursorX][gCursorY][1] & 0x80) != 0;
            first = (ch >= 0xA0) ? reverseBits(gDynamicChars[ext ? 1 : 0][(ch - 0xA0) * 10 + gSlice]) : 0;
            second = 0xFF;
            gSlice = (gSlice + 1) % 10;
        }
        else
        {
            first = gMem[gCursorX][gCursorY][1];
            second = gMem[gCursorX][gCursorY][0];
            advanceCursor();
        }
        return result;
    }
    case 6:
        return 0;
    default:
        return 0;
    }
}

void vpp_write(Byte dat, ADDRESS adr)
{
    static Byte sliceData = 0;

    switch (adr & 0xFF)
    {
    case 0:
        if (gSliceMode) sliceData = dat;
        else gMem[gCursorX][gCursorY][1] = dat;
        break;
    case 1:
        if (gSliceMode)
        {
            const Byte ch = gMem[gCursorX][gCursorY][0];
            const bool ext = (gMem[gCursorX][gCursorY][1] & 0x80) != 0;
            if (ch >= 0xA0)
                gDynamicChars[ext ? 1 : 0][(ch - 0xA0) * 10 + gSlice] = reverseBits(sliceData);
            gSlice = (gSlice + 1) % 10;
        }
        else
        {
            gMem[gCursorX][gCursorY][0] = dat;
            if (dat > 0x7F && dat < 0xA0 && !(gMem[gCursorX][gCursorY][1] & 0x80))
            {
                gMem[gCursorX][gCursorY][2] = dat;
                gMem[gCursorX][gCursorY][3] = gMem[gCursorX][gCursorY][1];
            }
            else
            {
                gMem[gCursorX][gCursorY][2] = 0;
                gMem[gCursorX][gCursorY][3] = 0;
            }
            advanceCursor();
        }
        break;
    case 2:
        gData = dat;
        break;
    case 3:
        switch (dat & 0xE0)
        {
        case 0x00: gCursorY = gData & 0x1F; gCursorX = 0; break;
        case 0x20: gCursorY = gData & 0x1F; break;
        case 0x40: gCursorX = (gData & 0x3F) % kCols; break;
        case 0x60: ++gCursorX; if (gCursorX >= kCols) { gCursorX = 0; if (++gCursorY >= 24) gCursorY = 0; } break;
        case 0x80:
            gSliceMode = false;
            gSlice = (gData & 0x1F) % 10;
            switch (gData & 0xE0)
            {
            case 0x00: case 0x20: gIncCursor = true; break;
            case 0x40: case 0x60: gIncCursor = false; break;
            case 0x80: case 0xA0: gSliceMode = true; break;
            default: break;
            }
            break;
        case 0xA0: gMode = gData; break;
        case 0xC0: gY0 = (gData & 0x1F) % 24; break;
        default: break;
        }
        break;
    default:
        break;
    }

    gDirty = true;
}

void init_vpp(void)
{
    gLum = 0xFF;
    gTransparency = 0xFF;
    gData = 0;
    gMode = 0;
    gCursorX = 0;
    gCursorY = 0;
    gY0 = 0;
    gSlice = 0;
    gSliceMode = false;
    gIncCursor = true;
    gDirty = true;
    gBlinkCounter = 0;
    gBlink = false;
    std::memset(gMem, 0, sizeof(gMem));
    std::memset(gDynamicChars, 0, sizeof(gDynamicChars));
    std::fill(gLayer.begin(), gLayer.end(), 0);

    if (app_data.vpp)
    {
        std::printf("O2EM-NG: Videopac+ VPP core enabled (G7400/Jopac)\n");
        std::fflush(stdout);
    }
}

void vpp_compose(Byte* framebuffer, int width, int height)
{
    if (!app_data.vpp || !framebuffer || width <= 0 || height <= 0)
        return;

    if (++gBlinkCounter >= 50)
    {
        gBlinkCounter = 0;
        gBlink = !gBlink;
        gDirty = true;
    }

    if (gTransparency == 0xFF)
        return;

    if (gDirty)
        rebuildLayer();

    // Match Mark Guttenbrunner's working O2EM 1.20 VPP compositor.
    // The crucial detail is that TraReg is tested against the colour of the
    // existing 8244/G7000 framebuffer pixel.  Patch 0029A/B incorrectly
    // tested the colour of the VPP pixel itself.
    const int dstX = 9;
    const int dstY = 5;
    const int copyW = std::min(kLayerW, std::max(0, width - dstX));
    const int copyH = std::min(kLayerH, std::max(0, height - dstY));

    // Preserve the legacy border behaviour used by vpp_finish_bmp().
    bool clearTop = false;
    bool clearLeft = false;

    if (dstY < height)
    {
        for (int x = 0; x < width && !clearTop; ++x)
            clearTop = underlyingColourRevealsVpp(framebuffer[dstY * width + x] & 7);
    }

    if (dstX < width)
    {
        for (int y = 0; y < height && !clearLeft; ++y)
            clearLeft = underlyingColourRevealsVpp(framebuffer[y * width + dstX] & 7);
    }

    if (clearTop)
    {
        for (int y = 0; y < std::min(dstY, height); ++y)
            std::memset(framebuffer + y * width, 0, static_cast<std::size_t>(width));
    }

    if (clearLeft)
    {
        for (int y = 0; y < height; ++y)
            std::memset(framebuffer + y * width, 0, static_cast<std::size_t>(std::min(dstX, width)));
    }

    for (int y = 0; y < copyH; ++y)
    {
        Byte* dst = framebuffer + (y + dstY) * width + dstX;
        const Byte* plus = gLayer.data() + y * kLayerW;

        for (int x = 0; x < copyW; ++x)
        {
            const Byte baseColour = dst[x];

            // Original O2EM only performs VPP replacement for normal palette
            // entries.  Collision bookkeeping is handled elsewhere in the
            // legacy renderer and is not required for the SDL visual merge.
            if (baseColour < 16 && underlyingColourRevealsVpp(baseColour & 7))
                dst[x] = plus[x];
        }
    }
}
