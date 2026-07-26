#include "vdc_stub.h"
#include "vmachine.h"
#include "config.h"
#include "cpu.h"
#include "cset.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

using Byte = uint8_t;

#define BMPW 340
#define BMPH 250
#define WNDW 320
#define WNDH 240

#define COL_SP0   0x01
#define COL_SP1   0x02
#define COL_SP2   0x04
#define COL_SP3   0x08
#define COL_VGRID 0x10
#define COL_HGRID 0x20
#define COL_VPP   0x40
#define COL_CHAR  0x80

Byte coltab[256] = { 0 };
long clip_low = 0;
long clip_high = BMPW * BMPH;
int show_fps = 0;

static SDL_Window* gWindow = nullptr;
static SDL_Renderer* gRenderer = nullptr;
static SDL_Texture* gTexture = nullptr;

static char gOverlayMessage[64] = { 0 };
static Uint64 gOverlayUntil = 0;

void VDCStub_ShowMessage(const char* message)
{
    if (!message)
        return;

    SDL_snprintf(gOverlayMessage, sizeof(gOverlayMessage), "%s", message);
    gOverlayUntil = SDL_GetTicks() + 1800;
}

static Byte vscreen[BMPW * BMPH] = { 0 };
static Byte col[BMPW * BMPH] = { 0 };
static uint32_t pixels[BMPW * BMPH] = { 0 };

static const uint32_t colortable[2][16] = {
    { 0x000000, 0x0e3dd4, 0x00981b, 0x00bbd9, 0xc70008, 0xcc16b3, 0x9d8710,
      0xe1dee1, 0x5f6e6b, 0x6aa1ff, 0x3df07a, 0x31ffff, 0xff4255, 0xff98ff,
      0xd9ad5d, 0xffffff },
    { 0x000000, 0x0000b6, 0x00b600, 0x00b6b6, 0xb60000, 0xb600b6, 0xb6b600,
      0xb6b6b6, 0x494949, 0x4949ff, 0x49ff49, 0x49ffff, 0xff4949, 0xff49ff,
      0xffff49, 0xffffff }
};

static uint32_t palette_to_argb(Byte index)
{
    int colorIndex = index & 0x0F;
    bool halfBright = (index & 0x10) != 0;

    uint32_t rgb = colortable[app_data.vpp ? 1 : 0][colorIndex];

    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    if (halfBright)
    {
        r /= 2;
        g /= 2;
        b /= 2;
    }

    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | b;
}

static void ensure_texture()
{
    if (!gRenderer || gTexture)
        return;

    gTexture = SDL_CreateTexture(
        gRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        BMPW,
        BMPH);

    if (!gTexture)
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
}

void VDCStub_SetWindow(SDL_Window* window)
{
    gWindow = window;

    if (!gRenderer && gWindow)
    {
        gRenderer = SDL_CreateRenderer(gWindow, nullptr);
        if (!gRenderer)
            SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
    }

    ensure_texture();
}

static inline void mputvid(unsigned int ad, unsigned int len, Byte d, Byte c)
{
    if ((ad > static_cast<unsigned long>(clip_low)) &&
        (ad < static_cast<unsigned long>(clip_high)))
    {
        for (unsigned int i = 0; i < len && ad < BMPW * BMPH; i++)
        {
            vscreen[ad] = d;
            col[ad] |= c;
            coltab[c] |= col[ad];
            ad++;
        }
    }
}

static void draw_grid(void)
{
    unsigned int pnt, pn1;
    Byte mask, d;
    int j, i, x, w;
    Byte color;

    if (VDCwrite[0xA0] & 0x40)
    {
        for (j = 0; j < 9; j++)
        {
            pnt = (((j * 24) + 24) * BMPW);
            for (i = 0; i < 9; i++)
            {
                pn1 = pnt + (i * 32) + 20;
                color = ColorVector[j * 24 + 24];
                mputvid(pn1, 4, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
                color = ColorVector[j * 24 + 25];
                mputvid(pn1 + BMPW, 4, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
                color = ColorVector[j * 24 + 26];
                mputvid(pn1 + BMPW * 2, 4, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
            }
        }
    }

    mask = 0x01;
    for (j = 0; j < 9; j++)
    {
        pnt = (((j * 24) + 24) * BMPW);
        for (i = 0; i < 9; i++)
        {
            pn1 = pnt + (i * 32) + 20;
            if ((pn1 + BMPW * 3 >= static_cast<unsigned long>(clip_low)) &&
                (pn1 <= static_cast<unsigned long>(clip_high)))
            {
                d = VDCwrite[0xC0 + i];
                if (j == 8)
                {
                    d = VDCwrite[0xD0 + i];
                    mask = 1;
                }
                if (d & mask)
                {
                    color = ColorVector[j * 24 + 24];
                    mputvid(pn1, 36, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
                    color = ColorVector[j * 24 + 25];
                    mputvid(pn1 + BMPW, 36, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
                    color = ColorVector[j * 24 + 26];
                    mputvid(pn1 + BMPW * 2, 36, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_HGRID);
                }
            }
        }
        mask = mask << 1;
    }

    w = (VDCwrite[0xA0] & 0x80) ? 32 : 4;
    for (j = 0; j < 10; j++)
    {
        pnt = (j * 32);
        mask = 0x01;
        d = VDCwrite[0xE0 + j];
        for (x = 0; x < 8; x++)
        {
            pn1 = pnt + (((x * 24) + 24) * BMPW) + 20;
            if (d & mask)
            {
                for (i = 0; i < 24; i++)
                {
                    if ((pn1 >= static_cast<unsigned long>(clip_low)) &&
                        (pn1 <= static_cast<unsigned long>(clip_high)))
                    {
                        color = ColorVector[x * 24 + 24 + i];
                        mputvid(pn1, w, (color & 0x07) | ((color & 0x40) >> 3) | (color & 0x80 ? 0 : 8), COL_VGRID);
                    }
                    pn1 += BMPW;
                }
            }
            mask = mask << 1;
        }
    }
}

static void draw_char(Byte ypos, Byte xpos, Byte chr, Byte colr)
{
    int j, c;
    Byte cl, d1;
    int y, b, n;
    unsigned int pnt;

    y = (ypos & 0xFE);
    pnt = y * BMPW + ((xpos - 8) * 2) + 20;

    ypos = ypos >> 1;
    n = 8 - (ypos % 8) - (chr % 8);
    if (n < 3)
        n = n + 7;

    if ((pnt + BMPW * 2 * n >= static_cast<unsigned long>(clip_low)) &&
        (pnt <= static_cast<unsigned long>(clip_high)))
    {
        c = static_cast<int>(chr) + ypos;
        if (colr & 0x01)
            c += 256;
        if (c > 511)
            c -= 512;

        cl = ((colr & 0x0E) >> 1);
        cl = ((cl & 2) | ((cl & 1) << 2) | ((cl & 4) >> 2)) + 8;

        if ((y > 0) && (y < 232) && (xpos < 157))
        {
            for (j = 0; j < n; j++)
            {
                d1 = cset[(c + j) & 0x1FF];
                for (b = 0; b < 8; b++)
                {
                    if (d1 & 0x80)
                    {
                        if ((xpos - 8 + b < 160) && (y + j < 240))
                        {
                            mputvid(pnt, 2, cl, COL_CHAR);
                            mputvid(pnt + BMPW, 2, cl, COL_CHAR);
                        }
                    }
                    pnt += 2;
                    d1 = d1 << 1;
                }
                pnt += BMPW * 2 - 16;
            }
        }
    }
}

static void draw_quad(Byte ypos, Byte xpos, Byte cp0l, Byte cp0h, Byte cp1l, Byte cp1h,
                      Byte cp2l, Byte cp2h, Byte cp3l, Byte cp3h)
{
    int chp[4];
    Byte qcol[4];
    unsigned int pnt;
    unsigned int off;
    int i, j, lines;

    pnt = (ypos & 0xfe) * BMPW + ((xpos - 8) * 2) + 20;
    if (pnt > static_cast<unsigned long>(clip_high))
        return;

    chp[0] = cp0l | ((cp0h & 1) << 8);
    chp[1] = cp1l | ((cp1h & 1) << 8);
    chp[2] = cp2l | ((cp2h & 1) << 8);
    chp[3] = cp3l | ((cp3h & 1) << 8);

    for (i = 0; i < 4; i++)
        chp[i] = (chp[i] + (ypos >> 1)) & 0x1ff;

    lines = 8 - (chp[3] + 1) % 8;

    if (pnt + BMPW * 2 * lines < static_cast<unsigned long>(clip_low))
        return;

    qcol[0] = (cp0h & 0xe) >> 1;
    qcol[1] = (cp1h & 0xe) >> 1;
    qcol[2] = (cp2h & 0xe) >> 1;
    qcol[3] = (cp3h & 0xe) >> 1;

    for (i = 0; i < 4; i++)
        qcol[i] = ((qcol[i] & 2) | ((qcol[i] & 1) << 2) | ((qcol[i] & 4) >> 2)) + 8;

    while (lines-- > 0)
    {
        off = 0;
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 8; j++)
            {
                if ((cset[chp[i] & 0x1FF] & (1 << (7 - j))) && (off < BMPW))
                {
                    mputvid(pnt + off, 2, qcol[i], COL_CHAR);
                    mputvid(pnt + off + BMPW, 2, qcol[i], COL_CHAR);
                }
                off += 2;
            }
            off += 16;
        }

        for (i = 0; i < 4; i++)
            chp[i] = (chp[i] + 1) & 0x1ff;

        pnt += BMPW * 2;
    }
}

void draw_display(void)
{
    int i, j, x, sm, t;
    Byte y, b, d1, cl, c;
    unsigned int pnt, pnt2;

    for (i = clip_low / BMPW; i < clip_high / BMPW && i < BMPH; i++)
    {
        std::memset(
            vscreen + i * BMPW,
            ((ColorVector[i] & 0x38) >> 3) | (ColorVector[i] & 0x80 ? 0 : 8),
            BMPW);
    }

    if (VDCwrite[0xA0] & 0x08)
        draw_grid();

    if (useforen && (!(VDCwrite[0xA0] & 0x20)))
        return;

    for (i = 0x10; i < 0x40; i += 4)
        draw_char(VDCwrite[i], VDCwrite[i + 1], VDCwrite[i + 2], VDCwrite[i + 3]);

    for (i = 0x40; i < 0x80; i += 0x10)
        draw_quad(VDCwrite[i], VDCwrite[i + 1], VDCwrite[i + 2], VDCwrite[i + 3],
                  VDCwrite[i + 6], VDCwrite[i + 7], VDCwrite[i + 10], VDCwrite[i + 11],
                  VDCwrite[i + 14], VDCwrite[i + 15]);

    c = 8;
    for (i = 12; i >= 0; i -= 4)
    {
        pnt2 = 0x80 + (i * 2);
        y = VDCwrite[i];
        x = VDCwrite[i + 1] - 8;
        t = VDCwrite[i + 2];
        cl = ((t & 0x38) >> 3);
        cl = ((cl & 2) | ((cl & 1) << 2) | ((cl & 4) >> 2)) + 8;

        if ((x < 164) && (y > 0) && (y < 232))
        {
            pnt = y * BMPW + (x * 2) + 20 + sproff;
            if (t & 4)
            {
                if ((pnt + BMPW * 32 >= static_cast<unsigned long>(clip_low)) &&
                    (pnt <= static_cast<unsigned long>(clip_high)))
                {
                    for (j = 0; j < 8; j++)
                    {
                        sm = (((j % 2 == 0) && (((t >> 1) & 1) != (t & 1))) ||
                              ((j % 2 == 1) && (t & 1))) ? 1 : 0;
                        d1 = VDCwrite[pnt2++];
                        for (b = 0; b < 8; b++)
                        {
                            if (d1 & 0x01)
                            {
                                if ((x + b + sm < 159) && (y + j < 247))
                                {
                                    mputvid(sm + pnt, 4, cl, c);
                                    mputvid(sm + pnt + BMPW, 4, cl, c);
                                    mputvid(sm + pnt + 2 * BMPW, 4, cl, c);
                                    mputvid(sm + pnt + 3 * BMPW, 4, cl, c);
                                }
                            }
                            pnt += 4;
                            d1 = d1 >> 1;
                        }
                        pnt += BMPW * 4 - 32;
                    }
                }
            }
            else
            {
                if ((pnt + BMPW * 16 >= static_cast<unsigned long>(clip_low)) &&
                    (pnt <= static_cast<unsigned long>(clip_high)))
                {
                    for (j = 0; j < 8; j++)
                    {
                        sm = (((j % 2 == 0) && (((t >> 1) & 1) != (t & 1))) ||
                              ((j % 2 == 1) && (t & 1))) ? 1 : 0;
                        d1 = VDCwrite[pnt2++];
                        for (b = 0; b < 8; b++)
                        {
                            if (d1 & 0x01)
                            {
                                if ((x + b + sm < 160) && (y + j < 249))
                                {
                                    mputvid(sm + pnt, 2, cl, c);
                                    mputvid(sm + pnt + BMPW, 2, cl, c);
                                }
                            }
                            pnt += 2;
                            d1 = d1 >> 1;
                        }
                        pnt += BMPW * 2 - 16;
                    }
                }
            }
        }
        c = c >> 1;
    }
}

void draw_region(void)
{
    int i;

    if (regionoff == 0xffff)
        i = (master_clk / (LINECNT - 1) - 5);
    else
        i = (master_clk / 22 + regionoff);

    i = snapline(i, VDCwrite[0xA0], 0);

    if (app_data.crc == 0xA7344D1F)
    {
        i = (master_clk / 22 + regionoff) + 6;
        i = snapline(i, VDCwrite[0xA0], 0) + 6;
    }

    if (app_data.crc == 0xD0BC4EE6)
    {
        i = (master_clk / 24 + regionoff) - 6;
        i = snapline(i, VDCwrite[0xA0], 0) + 7;
    }

    if (app_data.crc == 0x26517E77)
    {
        i = (master_clk / 22 + regionoff);
        i = snapline(i, VDCwrite[0xA0], 0) - 5;
    }

    if (app_data.crc == 0xA57E1724)
    {
        i = (master_clk / (LINECNT - 1) - 5);
        i = snapline(i, VDCwrite[0xA0], 0) - 3;
    }

    if (i < 0)
        i = 0;
    if (i > BMPH)
        i = BMPH;

    clip_low = last_line * static_cast<long>(BMPW);
    clip_high = i * static_cast<long>(BMPW);

    if (clip_high > BMPW * BMPH)
        clip_high = BMPW * BMPH;
    if (clip_low < 0)
        clip_low = 0;

    if (clip_low < clip_high)
        draw_display();

    last_line = i;
}

void finish_display(void)
{
    if (!gRenderer || !gWindow)
        return;

    ensure_texture();

    if (!gTexture)
        return;

    for (int i = 0; i < BMPW * BMPH; i++)
        pixels[i] = palette_to_argb(vscreen[i]);

    SDL_UpdateTexture(
        gTexture,
        nullptr,
        pixels,
        BMPW * sizeof(uint32_t)
    );

    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);

    SDL_FRect src{
        7.0f,
        2.0f,
        static_cast<float>(WNDW),
        static_cast<float>(WNDH)
    };

    int windowW = 0;
    int windowH = 0;

    SDL_GetWindowSize(gWindow, &windowW, &windowH);

    float scaleX =
        static_cast<float>(windowW) /
        static_cast<float>(WNDW);

    float scaleY =
        static_cast<float>(windowH) /
        static_cast<float>(WNDH);

    float scale =
        (scaleX < scaleY) ? scaleX : scaleY;

    float outputW =
        static_cast<float>(WNDW) * scale;

    float outputH =
        static_cast<float>(WNDH) * scale;

    SDL_FRect dst{
        (static_cast<float>(windowW) - outputW) * 0.5f,
        (static_cast<float>(windowH) - outputH) * 0.5f,
        outputW,
        outputH
    };

    SDL_RenderTexture(
        gRenderer,
        gTexture,
        &src,
        &dst
    );

    if (app_data.scanlines)
    {
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 70);

        for (float y = dst.y; y < dst.y + dst.h; y += 2.0f)
        {
            SDL_RenderLine(gRenderer, dst.x, y, dst.x + dst.w, y);
        }
    }

    if (gOverlayMessage[0] != '\0' && SDL_GetTicks() < gOverlayUntil)
    {
        float textX = dst.x + 12.0f;
        float textY = dst.y + dst.h - 28.0f;

        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 190);
        SDL_FRect background{
            textX - 6.0f,
            textY - 6.0f,
            static_cast<float>(SDL_strlen(gOverlayMessage) * 8 + 12),
            20.0f
        };
        SDL_RenderFillRect(gRenderer, &background);

        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
        SDL_RenderDebugText(gRenderer, textX, textY, gOverlayMessage);
    }
    else if (gOverlayMessage[0] != '\0')
    {
        gOverlayMessage[0] = '\0';
    }

    SDL_RenderPresent(gRenderer);
}

void clear_collision(void)
{
    std::memset(col, 0, sizeof(col));
    coltab[0x01] = coltab[0x02] = 0;
    coltab[0x04] = coltab[0x08] = 0;
    coltab[0x10] = coltab[0x20] = 0;
    coltab[0x40] = coltab[0x80] = 0;
}

void close_display(void)
{
    if (gTexture)
    {
        SDL_DestroyTexture(gTexture);
        gTexture = nullptr;
    }

    if (gRenderer)
    {
        SDL_DestroyRenderer(gRenderer);
        gRenderer = nullptr;
    }
}

void grmode(void)
{
}

void set_textmode(void)
{
}

void clearscr(void)
{
    std::memset(vscreen, 0, sizeof(vscreen));
    std::memset(col, 0, sizeof(col));
}

void VDCStub_ShowTestPattern(void)
{
    if (!gRenderer)
        return;

    SDL_SetRenderDrawColor(gRenderer, 40, 40, 40, 255);
    SDL_RenderClear(gRenderer);

    for (int y = 0; y < 700; y += 40)
    {
        SDL_SetRenderDrawColor(gRenderer, 200, 200, 200, 255);
        SDL_FRect line{ 0.0f, static_cast<float>(y), 1000.0f, 20.0f };
        SDL_RenderFillRect(gRenderer, &line);
    }

    SDL_RenderPresent(gRenderer);
}
