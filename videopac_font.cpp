#include "videopac_font.h"
#include "types.h"
#include "cset.h"

#include <cctype>

static int GlyphIndex(char ch)
{
    unsigned char c = static_cast<unsigned char>(ch);
    char u = static_cast<char>(std::toupper(c));

    if (u >= '0' && u <= '9')
        return u - '0';

    switch (u)
    {
    case ':': return 10;
    case '?': return 13;
    case 'L': return 14;
    case 'P': return 15;
    case '+': return 16;
    case 'W': return 17;
    case 'E': return 18;
    case 'R': return 19;
    case 'T': return 20;
    case 'U': return 21;
    case 'I': return 22;
    case 'O': return 23;
    case 'Q': return 24;
    case 'S': return 25;
    case 'D': return 26;
    case 'F': return 27;
    case 'G': return 28;
    case 'H': return 29;
    case 'J': return 30;
    case 'K': return 31;
    case 'A': return 32;
    case 'Z': return 33;
    case 'X': return 34;
    case 'C': return 35;
    case 'V': return 36;
    case 'B': return 37;
    case 'M': return 38;
    case '.': return 39;
    case '-': return 40;
    case '*': return 41;
    case '/': return 46;
    case 'Y': return 44;
    case 'N': return 45;
    case ' ': return 12;
    default:  return 13; // unsupported character -> question mark
    }
}

void VideopacFont_DrawText(
    SDL_Renderer* renderer,
    float x,
    float y,
    float scale,
    const std::string& text)
{
    if (!renderer || scale <= 0.0f)
        return;

    float cursorX = x;
    float cursorY = y;

    const float glyphWidth = 8.0f * scale;
    const float glyphHeight = 8.0f * scale;
    const float advanceX = 9.0f * scale;
    const float advanceY = 10.0f * scale;

    for (char ch : text)
    {
        if (ch == '\n')
        {
            cursorX = x;
            cursorY += advanceY;
            continue;
        }

        int glyph = GlyphIndex(ch);

        if (glyph != 12)
        {
            const int base = glyph * 8;

            for (int row = 0; row < 8; ++row)
            {
                unsigned char bits = cset[base + row];

                for (int col = 0; col < 8; ++col)
                {
                    if (bits & (0x80 >> col))
                    {
                        SDL_FRect pixel{
                            cursorX + static_cast<float>(col) * scale,
                            cursorY + static_cast<float>(row) * scale,
                            scale,
                            scale
                        };

                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }

        cursorX += advanceX;
    }
}
