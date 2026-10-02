#include "frontend_collection.h"

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "src/library/game_info.h"
#include "src/library/game_library.h"
#include "src/database/game_database.h"
#include "frontend_boxart.h"
#include "frontend_panels.h"
#include "theme_win95.h"
#include "ui_font.h"

namespace
{
    // -------------------------------------------------------------------
    // Win95 drawing primitives (kept file-local so this module is fully
    // self-contained and does not disturb the other frontend pages).
    // -------------------------------------------------------------------
    void DrawText(SDL_Renderer* renderer, float x, float y, float scale,
        const std::string& text)
    {
        UiFont_DrawText(renderer, x, y, 16.5f * scale, text);
    }

    void DrawSunkenFrame(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }

    void DrawRaisedFrame(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }

    void DrawWin95ButtonBody(SDL_Renderer* renderer, const SDL_FRect& rect,
        const char* label, bool pressed)
    {
        if (!pressed)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            const SDL_FRect shadow{rect.x + 3.0f, rect.y + 3.0f, rect.w, rect.h};
            SDL_RenderFillRect(renderer, &shadow);
        }
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);
        if (pressed)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        }
        const float pressOffset = pressed ? 2.0f : 0.0f;
        const float textScale = 0.95f;
        const float textWidth = 8.0f * textScale * static_cast<float>(std::strlen(label));
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer,
            rect.x + (std::max)(2.0f, (rect.w - textWidth) * 0.5f) + pressOffset,
            rect.y + (rect.h - 16.0f) * 0.5f + pressOffset, textScale, label);
    }

    void DrawWin95VScrollbar(SDL_Renderer* renderer, const SDL_FRect& upArrow,
        const SDL_FRect& downArrow, const SDL_FRect& track, const SDL_FRect& thumb)
    {
        const auto drawArrowButton = [&](const SDL_FRect& rect, bool up)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
            SDL_RenderFillRect(renderer, &rect);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            const float cx = rect.x + rect.w * 0.5f;
            const float cy = up ? rect.y + rect.h * 0.5f - 1.5f
                                : rect.y + rect.h * 0.5f + 1.5f;
            for (int k = 0; k < 4; ++k)
            {
                const float half = static_cast<float>(k);
                const float yy = up ? cy - 1.5f + static_cast<float>(k)
                                    : cy + 1.5f - static_cast<float>(k);
                SDL_RenderLine(renderer, cx - half, yy, cx + half, yy);
            }
        };
        drawArrowButton(upArrow, true);
        drawArrowButton(downArrow, false);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &track);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, track.x, track.y, track.x, track.y + track.h - 1.0f);
        SDL_RenderLine(renderer, track.x, track.y, track.x + track.w - 1.0f, track.y);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, track.x + track.w - 1.0f, track.y,
            track.x + track.w - 1.0f, track.y + track.h - 1.0f);
        SDL_RenderLine(renderer, track.x, track.y + track.h - 1.0f,
            track.x + track.w - 1.0f, track.y + track.h - 1.0f);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &thumb);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, thumb.x, thumb.y, thumb.x + thumb.w - 1.0f, thumb.y);
        SDL_RenderLine(renderer, thumb.x, thumb.y, thumb.x, thumb.y + thumb.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, thumb.x, thumb.y + thumb.h - 1.0f,
            thumb.x + thumb.w - 1.0f, thumb.y + thumb.h - 1.0f);
        SDL_RenderLine(renderer, thumb.x + thumb.w - 1.0f, thumb.y,
            thumb.x + thumb.w - 1.0f, thumb.y + thumb.h - 1.0f);
    }

    void DrawCheckBox(SDL_Renderer* renderer, const SDL_FRect& rect, bool checked)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &rect);
        if (!checked)
            return;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        const float sx = rect.w / 14.0f;
        const float sy = rect.h / 14.0f;
        SDL_RenderLine(renderer, rect.x + 3.0f * sx, rect.y + 6.0f * sy,
            rect.x + 6.0f * sx, rect.y + 10.0f * sy);
        SDL_RenderLine(renderer, rect.x + 6.0f * sx, rect.y + 10.0f * sy,
            rect.x + 11.0f * sx, rect.y + 2.0f * sy);
        SDL_RenderLine(renderer, rect.x + 3.0f * sx, rect.y + 7.0f * sy,
            rect.x + 6.0f * sx, rect.y + 11.0f * sy);
        SDL_RenderLine(renderer, rect.x + 6.0f * sx, rect.y + 11.0f * sy,
            rect.x + 11.0f * sx, rect.y + 3.0f * sy);
    }

    void DrawSortArrow(SDL_Renderer* renderer, float cx, float cy)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        SDL_RenderLine(renderer, cx - 3.0f, cy - 2.0f, cx + 3.0f, cy - 2.0f);
        SDL_RenderLine(renderer, cx - 2.0f, cy - 1.0f, cx + 2.0f, cy - 1.0f);
        SDL_RenderLine(renderer, cx - 1.0f, cy, cx + 1.0f, cy);
        SDL_RenderLine(renderer, cx, cy + 1.0f, cx, cy + 1.0f);
    }

    // Small Win95 checkbox with a green tick, used by the Library front-page
    // My Collection summary (matches the approved overview panel).
    void DrawSummaryCheck(SDL_Renderer* renderer, const SDL_FRect& rect, bool checked)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &rect);
        if (!checked)
            return;
        Win95Theme::SetRenderColor(renderer, SDL_Color{0, 150, 0, 255});
        const float sx = rect.w / 14.0f;
        const float sy = rect.h / 14.0f;
        SDL_RenderLine(renderer, rect.x + 3.0f * sx, rect.y + 6.0f * sy,
            rect.x + 6.0f * sx, rect.y + 10.0f * sy);
        SDL_RenderLine(renderer, rect.x + 6.0f * sx, rect.y + 10.0f * sy,
            rect.x + 11.0f * sx, rect.y + 2.0f * sy);
        SDL_RenderLine(renderer, rect.x + 3.0f * sx, rect.y + 7.0f * sy,
            rect.x + 6.0f * sx, rect.y + 11.0f * sy);
        SDL_RenderLine(renderer, rect.x + 6.0f * sx, rect.y + 11.0f * sy,
            rect.x + 11.0f * sx, rect.y + 3.0f * sy);
    }

    void DrawMagnifier(SDL_Renderer* renderer, float cx, float cy)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        constexpr float r = 4.5f;
        SDL_FPoint previous{};
        for (int i = 0; i <= 8; ++i)
        {
            const float angle = static_cast<float>(i) * 3.14159265f / 4.0f;
            const SDL_FPoint point{cx + r * std::cos(angle), cy + r * std::sin(angle)};
            if (i > 0)
                SDL_RenderLine(renderer, previous.x, previous.y, point.x, point.y);
            previous = point;
        }
        SDL_RenderLine(renderer, cx + 3.0f, cy + 3.0f, cx + 7.0f, cy + 7.0f);
    }

    void DrawGroupPanel(SDL_Renderer* renderer, const SDL_FRect& rect,
        const char* title, float titleSize = 17.5f)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        UiFont_DrawText(renderer, rect.x + 12.0f, rect.y + 6.0f, titleSize, title);
    }

    // UTF-8 safe ellipsis (defined below; declared here for DrawField).
    std::string Utf8Ellipsize(const std::string& text, std::size_t maxChars);

    void DrawField(SDL_Renderer* renderer, const SDL_FRect& rect,
        const std::string& value, bool combo)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);

        const float textWidth = combo ? rect.w - 24.0f : rect.w - 10.0f;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        std::string text = value;
        const std::size_t maxChars = static_cast<std::size_t>(
            (std::max)(4.0f, textWidth / (8.0f * 0.9f)));
        text = Utf8Ellipsize(text, maxChars);
        UiFont_DrawText(renderer, rect.x + 5.0f, rect.y + 3.0f, 13.0f, text);

        if (combo)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
            const SDL_FRect arrow{rect.x + rect.w - 19.0f, rect.y + 1.0f,
                18.0f, rect.h - 2.0f};
            SDL_RenderFillRect(renderer, &arrow);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, arrow.x, arrow.y, arrow.x, arrow.y + arrow.h - 1.0f);
            DrawSortArrow(renderer, arrow.x + 9.0f, arrow.y + arrow.h * 0.5f + 1.0f);
        }
    }

    // -------------------------------------------------------------------
    // Small string helpers
    // -------------------------------------------------------------------
    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ContainsInsensitive(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty())
            return true;
        return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::string::npos;
    }

    std::string DisplayCatalogId(const GameInfo* game)
    {
        if (!game)
            return {};
        if (!game->catalogId.empty())
            return game->catalogId;
        if (game->videopacNumber > 0)
        {
            char buffer[16]{};
            std::snprintf(buffer, sizeof(buffer), "%02d", game->videopacNumber);
            return buffer;
        }
        return {};
    }

    std::string TodayDate()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char buffer[16]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
        return buffer;
    }

    // -------------------------------------------------------------------
    // UTF-8 aware text helpers for the Notes editor.
    //
    // Caret positions are byte indices into the UTF-8 string. Moving the
    // caret and deleting always operate on whole logical characters so a
    // multi-byte sequence (å, ä, ö, ø, —) is never split.
    // -------------------------------------------------------------------
    constexpr float kNotesLineHeight = 16.0f;
    constexpr float kNotesPad = 3.0f;

    bool Utf8Continuation(unsigned char c)
    {
        return (c & 0xC0) == 0x80;
    }

    std::size_t Utf8Prev(const std::string& text, std::size_t pos)
    {
        if (pos == 0)
            return 0;
        if (pos > text.size())
            pos = text.size();
        --pos;
        while (pos > 0 && Utf8Continuation(static_cast<unsigned char>(text[pos])))
            --pos;
        return pos;
    }

    std::size_t Utf8Next(const std::string& text, std::size_t pos)
    {
        if (pos >= text.size())
            return text.size();
        ++pos;
        while (pos < text.size() &&
            Utf8Continuation(static_cast<unsigned char>(text[pos])))
            ++pos;
        return pos;
    }

    int Utf8LineCount(const std::string& text)
    {
        int count = 1;
        for (char c : text)
            if (c == '\n')
                ++count;
        return count;
    }

    void Utf8LineRange(const std::string& text, int line,
        std::size_t& start, std::size_t& end)
    {
        int current = 0;
        start = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\n')
            {
                if (current == line) { end = i; return; }
                ++current;
                start = i + 1;
            }
        }
        if (current == line) { end = text.size(); return; }
        start = text.size();
        end = text.size();
    }

    int Utf8CaretLine(const std::string& text, std::size_t caret)
    {
        if (caret > text.size())
            caret = text.size();
        int line = 0;
        for (std::size_t i = 0; i < caret; ++i)
            if (text[i] == '\n')
                ++line;
        return line;
    }

    // Width of text exactly as UiFont_DrawText renders it at 13 px, with a
    // conservative estimate only when no system font is available.
    float UiWidth13(const std::string& text)
    {
        float width = 0.0f;
        float height = 0.0f;
        if (UiFont_MeasureText(13.0f, text, &width, &height))
            return width;
        return 8.0f * 0.9f * static_cast<float>(text.size());
    }

    // UTF-8 safe ellipsis: counts logical characters and never splits a
    // multi-byte sequence (which would render as a replacement box).
    std::string Utf8Ellipsize(const std::string& text, std::size_t maxChars)
    {
        if (maxChars < 4)
            maxChars = 4;
        std::size_t count = 0;
        std::size_t cut = text.size();
        for (std::size_t i = 0; i < text.size();)
        {
            if (count == maxChars - 3)
                cut = i;
            ++count;
            const std::size_t next = Utf8Next(text, i);
            if (next == i)
                break;
            i = next;
        }
        if (count <= maxChars)
            return text;
        return text.substr(0, cut) + "...";
    }

    // Normalizes pasted clipboard text: Notes keeps newlines (CRLF -> LF),
    // single-line fields never contain line breaks. Prevents stray '\r' boxes.
    std::string SanitizeClipboardText(std::string text, bool multiline)
    {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\r' || c == '\n')
            {
                if (multiline)
                {
                    out += '\n';
                    if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                        ++i;
                }
                else
                {
                    out += ' ';
                }
            }
            else
            {
                out += c;
            }
        }
        return out;
    }

    // Draws the multiline Notes field: white sunken box, lines clipped to the
    // box, smooth vertical scrolling that follows the caret, a thin caret
    // exactly one text line high aligned with the rendered glyphs, and a
    // lightweight selection highlight for clipboard feedback.
    void DrawMultilineNotes(SDL_Renderer* renderer, const SDL_FRect& box,
        const std::string& text, std::size_t caret, bool showCaret,
        int& scroll, std::size_t selA = 0, std::size_t selB = 0,
        bool hasSelection = false)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &box);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, box.x, box.y, box.x + box.w - 1.0f, box.y);
        SDL_RenderLine(renderer, box.x, box.y, box.x, box.y + box.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, box.x, box.y + box.h - 1.0f,
            box.x + box.w - 1.0f, box.y + box.h - 1.0f);
        SDL_RenderLine(renderer, box.x + box.w - 1.0f, box.y,
            box.x + box.w - 1.0f, box.y + box.h - 1.0f);

        const int totalLines = Utf8LineCount(text);
        const int visibleLines = (std::max)(1, static_cast<int>(
            (box.h - kNotesPad * 2.0f) / kNotesLineHeight));

        const int caretLine = Utf8CaretLine(text, caret);
        if (caretLine < scroll)
            scroll = caretLine;
        else if (caretLine >= scroll + visibleLines)
            scroll = caretLine - visibleLines + 1;
        scroll = (std::clamp)(scroll, 0,
            (std::max)(0, totalLines - visibleLines));

        const SDL_Rect clip{
            static_cast<int>(box.x) + 1, static_cast<int>(box.y) + 1,
            static_cast<int>(box.w) - 2, static_cast<int>(box.h) - 2};
        SDL_SetRenderClipRect(renderer, &clip);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        for (int line = scroll; line < totalLines && line < scroll + visibleLines;
            ++line)
        {
            std::size_t start = 0;
            std::size_t end = 0;
            Utf8LineRange(text, line, start, end);
            const std::string lineText = text.substr(start, end - start);
            if (hasSelection)
            {
                const std::size_t hs = (std::max)(selA, start);
                const std::size_t he = (std::min)(selB, end);
                if (he > hs)
                {
                    const float x1 = box.x + kNotesPad +
                        UiWidth13(text.substr(start, hs - start));
                    const float x2 = box.x + kNotesPad +
                        UiWidth13(text.substr(start, he - start));
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 128, 110);
                    const SDL_FRect highlight{x1,
                        box.y + kNotesPad + static_cast<float>(line - scroll) * kNotesLineHeight,
                        (std::max)(1.0f, x2 - x1), kNotesLineHeight};
                    SDL_RenderFillRect(renderer, &highlight);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }
            }
            UiFont_DrawText(renderer, box.x + kNotesPad,
                box.y + kNotesPad + static_cast<float>(line - scroll) * kNotesLineHeight,
                13.0f, lineText);
        }

        if (showCaret)
        {
            std::size_t start = 0;
            std::size_t end = 0;
            Utf8LineRange(text, caretLine, start, end);
            const std::size_t clampedCaret = (std::min)(caret, end);
            const std::string prefix = text.substr(start, clampedCaret - start);
            const float caretX = box.x + kNotesPad + UiWidth13(prefix);
            const float caretY = box.y + kNotesPad +
                static_cast<float>(caretLine - scroll) * kNotesLineHeight;
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            SDL_RenderLine(renderer, caretX, caretY,
                caretX, caretY + kNotesLineHeight - 3.0f);
        }
        SDL_SetRenderClipRect(renderer, nullptr);
    }

    // Maps a mouse point inside the Notes box to a caret byte index.
    std::size_t NotesCaretFromPoint(const std::string& text,
        const SDL_FRect& box, float x, float y, int scroll)
    {
        const int totalLines = Utf8LineCount(text);
        int line = static_cast<int>((y - (box.y + kNotesPad)) / kNotesLineHeight) + scroll;
        line = (std::clamp)(line, 0, totalLines - 1);

        std::size_t start = 0;
        std::size_t end = 0;
        Utf8LineRange(text, line, start, end);

        const float target = x - (box.x + kNotesPad);
        if (target <= 0.0f)
            return start;

        std::size_t pos = start;
        while (pos < end)
        {
            const std::size_t next = Utf8Next(text, pos);
            if (next == pos)
                break;
            if (UiWidth13(text.substr(start, next - start)) > target)
                break;
            pos = next;
        }
        return pos;
    }

    bool IsDialogTextField(int field);
    bool IsDialogComboField(int field);
    bool IsDialogCheckboxField(int field);

    // -------------------------------------------------------------------
    // Add / Edit dialog geometry
    // -------------------------------------------------------------------
    enum DialogField
    {
        DF_RefList = 0,
        DF_CatalogId, DF_Title,
        DF_Region, DF_CartridgeCondition, DF_BoxCondition, DF_ManualCondition,
        DF_Quantity, DF_PurchaseSource,
        DF_PurchaseDate, DF_DateAdded,
        DF_Shelf, DF_Notes,
        DF_Owned, DF_Cartridge, DF_Box, DF_Manual, DF_Wanted,
        DF_Ok, DF_Cancel,
        DF_Count
    };

    // The three component conditions behave identically; these small helpers
    // map a dialog field to its owning component flag and value.
    bool IsConditionField(int field)
    {
        return field == DF_CartridgeCondition || field == DF_BoxCondition ||
            field == DF_ManualCondition;
    }

    bool ConditionComponentPresent(const CollectionEntry& entry, int field)
    {
        if (field == DF_BoxCondition) return entry.box;
        if (field == DF_ManualCondition) return entry.manual;
        return entry.cartridge;
    }

    std::string& ConditionValueRef(CollectionEntry& entry, int field)
    {
        if (field == DF_BoxCondition) return entry.boxCondition;
        if (field == DF_ManualCondition) return entry.manualCondition;
        return entry.cartridgeCondition;
    }

    struct DialogLayout
    {
        SDL_FRect panel;
        SDL_FRect titleBar;
        SDL_FRect refList;
        SDL_FRect refManualRow;
        SDL_FRect refUp;
        SDL_FRect refDown;
        SDL_FRect refTrack;
        SDL_FRect gamesList;
        int refRows = 0;
        SDL_FRect label[DF_Count];
        SDL_FRect input[DF_Count];
        int comboPopupItems = 0;
    };

    DialogLayout LayoutCollectionDialog(const SDL_FRect& content)
    {
        DialogLayout layout{};

        const float panelW = (std::clamp)(content.w * 0.82f, 720.0f, 940.0f);
        const float panelH = (std::clamp)(content.h * 0.94f, 500.0f, 660.0f);
        layout.panel = {
            content.x + (content.w - panelW) * 0.5f,
            content.y + (content.h - panelH) * 0.5f,
            panelW, panelH
        };
        layout.titleBar = {layout.panel.x + 2.0f, layout.panel.y + 2.0f,
            layout.panel.w - 4.0f, 26.0f};

        const float pad = 12.0f;
        const float innerTop = layout.panel.y + 34.0f;
        const float bottomBarH = 42.0f;
        const float bodyH = layout.panel.y + layout.panel.h - pad - bottomBarH - innerTop;
        const float leftW = (std::clamp)(layout.panel.w * 0.30f, 220.0f, 280.0f);

        // Reference picker (left column).
        layout.label[DF_RefList] = {layout.panel.x + pad, innerTop, leftW, 15.0f};
        layout.refList = {layout.panel.x + pad, innerTop + 18.0f, leftW, bodyH - 18.0f};
        layout.refManualRow = {layout.refList.x + 2.0f, layout.refList.y + 2.0f,
            layout.refList.w - 4.0f, 20.0f};
        const float gamesTop = layout.refManualRow.y + layout.refManualRow.h + 2.0f;
        const float gamesBottom = layout.refList.y + layout.refList.h - 2.0f;
        layout.refTrack = {layout.refList.x + layout.refList.w - 15.0f, gamesTop + 15.0f,
            15.0f, (gamesBottom - gamesTop) - 30.0f};
        layout.refUp = {layout.refList.x + layout.refList.w - 15.0f, gamesTop, 15.0f, 15.0f};
        layout.refDown = {layout.refList.x + layout.refList.w - 15.0f,
            gamesBottom - 15.0f, 15.0f, 15.0f};
        layout.gamesList = {layout.refList.x + 1.0f, gamesTop,
            layout.refList.w - 18.0f, gamesBottom - gamesTop};
        layout.refRows = (std::max)(0,
            static_cast<int>(layout.gamesList.h / 18.0f));

        // Fields (right side, two columns).
        const float rightX = layout.panel.x + pad + leftW + 14.0f;
        const float rightW = layout.panel.x + layout.panel.w - pad - rightX;
        const float colGap = 12.0f;
        const float colW = (rightW - colGap) * 0.5f;
        const float x0 = rightX;
        const float x1 = rightX + colW + colGap;
        const float labelH = 15.0f;
        const float inputH = 22.0f;
        const float rowH = 47.0f;
        float y = innerTop;

        const auto place = [&](int field, float x, float w)
        {
            layout.label[field] = {x, y, w, labelH};
            layout.input[field] = {x, y + labelH, w, inputH};
        };
        place(DF_CatalogId, x0, colW);
        place(DF_Title, x1, colW);
        y += rowH;
        place(DF_Region, x0, colW);
        place(DF_CartridgeCondition, x1, colW);
        y += rowH;
        place(DF_BoxCondition, x0, colW);
        place(DF_ManualCondition, x1, colW);
        y += rowH;
        place(DF_Quantity, x0, colW);
        place(DF_PurchaseSource, x1, colW);
        y += rowH;
        place(DF_PurchaseDate, x0, colW);
        place(DF_DateAdded, x1, colW);
        y += rowH;

        const float checkRowY = innerTop + bodyH - 22.0f;
        // Shelf / storage (full width) then notes (remaining height).
        layout.label[DF_Shelf] = {rightX, y, rightW, labelH};
        layout.input[DF_Shelf] = {rightX, y + labelH, rightW, inputH};
        float notesY = y + labelH + inputH + 8.0f;
        layout.label[DF_Notes] = {rightX, notesY, rightW, labelH};
        const float notesH = (std::max)(40.0f, checkRowY - 8.0f - (notesY + labelH));
        layout.input[DF_Notes] = {rightX, notesY + labelH, rightW, notesH};

        const float checkW = rightW / 5.0f;
        const int checkFields[5] = {DF_Owned, DF_Cartridge, DF_Box, DF_Manual, DF_Wanted};
        for (int i = 0; i < 5; ++i)
        {
            const float cx = rightX + static_cast<float>(i) * checkW;
            layout.label[checkFields[i]] = {cx + 18.0f, checkRowY + 2.0f, checkW - 18.0f, 16.0f};
            layout.input[checkFields[i]] = {cx, checkRowY, 14.0f, 14.0f};
        }

        const float buttonW = 88.0f;
        const float buttonH = 26.0f;
        const float buttonY = layout.panel.y + layout.panel.h - pad - buttonH;
        layout.input[DF_Ok] = {layout.panel.x + layout.panel.w - pad - buttonW,
            buttonY, buttonW, buttonH};
        layout.input[DF_Cancel] = {layout.input[DF_Ok].x - buttonW - 8.0f,
            buttonY, buttonW, buttonH};

        return layout;
    }

    bool IsDialogTextField(int field)
    {
        switch (field)
        {
        case DF_CatalogId: case DF_Title: case DF_Quantity:
        case DF_PurchaseSource: case DF_PurchaseDate: case DF_DateAdded:
        case DF_Shelf: case DF_Notes:
            return true;
        default:
            return false;
        }
    }

    bool IsDialogComboField(int field)
    {
        return field == DF_Region || IsConditionField(field);
    }

    bool IsDialogCheckboxField(int field)
    {
        return field == DF_Owned || field == DF_Cartridge || field == DF_Box ||
            field == DF_Manual || field == DF_Wanted;
    }

    std::string CsvEscape(const std::string& value)
    {
        std::string out = "\"";
        for (char c : value)
        {
            if (c == '"')
                out += "\"\"";
            else
                out += c;
        }
        out += "\"";
        return out;
    }
}

// ===========================================================================
// Layout of the main page (unchanged from the approved Phase A runtime).
// ===========================================================================
namespace
{
    struct MyCollectionLayout
    {
        SDL_FRect inner;
        SDL_FRect search;
        SDL_FRect table;
        SDL_FRect header;
        SDL_FRect list;
        SDL_FRect scrollbar;
        SDL_FRect scrollUp;
        SDL_FRect scrollDown;
        SDL_FRect scrollTrack;
        SDL_FRect details;
        SDL_FRect stats;
        SDL_FRect buttons[5];
        float colOwned = 0.0f;
        float colNo = 0.0f;
        float colTitle = 0.0f;
        float colCartridge = 0.0f;
        float colBox = 0.0f;
        float colManual = 0.0f;
        float colWanted = 0.0f;
        int visibleRows = 0;
    };

    MyCollectionLayout LayoutMyCollection(const SDL_FRect& content)
    {
        MyCollectionLayout layout{};

        const float margin = 14.0f;
        const SDL_FRect frame{content.x + margin, content.y + margin,
            content.w - margin * 2.0f, content.h - margin * 2.0f};
        layout.inner = {frame.x + 4.0f, frame.y + 4.0f, frame.w - 8.0f, frame.h - 8.0f};

        const float pad = 14.0f;
        // Slightly more breathing room under the heading/subtitle (QA polish).
        const float tableTop = layout.inner.y + 86.0f;
        const float buttonH = 32.0f;
        const float buttonGap = 10.0f;
        const float bottom = layout.inner.y + layout.inner.h - pad;
        const float tableH = (std::max)(180.0f, bottom - buttonH - buttonGap - tableTop);

        const float searchW = (std::min)(360.0f, layout.inner.w * 0.32f);
        layout.search = {layout.inner.x + layout.inner.w - pad - searchW,
            layout.inner.y + 12.0f, searchW, 30.0f};

        // Game Details gets a wider share of the page; the table keeps enough
        // width to remain comfortable. Only a modest amount is moved.
        const float detailsW = (std::clamp)(layout.inner.w * 0.37f, 340.0f, 480.0f);
        const float gap = 14.0f;
        const float tableW = (std::max)(320.0f, layout.inner.w - detailsW - gap);
        layout.table = {layout.inner.x, tableTop, tableW, tableH};

        const float headerH = 30.0f;
        layout.header = {layout.table.x, layout.table.y, layout.table.w, headerH};
        layout.list = {layout.table.x, layout.table.y + headerH,
            layout.table.w, layout.table.h - headerH};

        layout.scrollbar = {layout.table.x + layout.table.w - 17.0f,
            layout.table.y, 17.0f, layout.table.h};
        layout.scrollUp = {layout.scrollbar.x, layout.scrollbar.y, 17.0f, 17.0f};
        layout.scrollDown = {layout.scrollbar.x,
            layout.scrollbar.y + layout.scrollbar.h - 17.0f, 17.0f, 17.0f};
        layout.scrollTrack = {layout.scrollbar.x, layout.scrollbar.y + 17.0f,
            17.0f, layout.scrollbar.h - 34.0f};
        layout.visibleRows = (std::max)(1, static_cast<int>(layout.list.h / 28.0f));

        const float gapStats = 12.0f;
        float detailsH = tableH * 0.66f;
        detailsH = (std::min)(detailsH, tableH - gapStats - 110.0f);
        detailsH = (std::max)(detailsH, (std::min)(tableH - gapStats, 120.0f));
        layout.details = {layout.table.x + layout.table.w + gap,
            layout.table.y, detailsW, detailsH};
        layout.stats = {layout.details.x, layout.details.y + detailsH + gapStats,
            detailsW, tableH - detailsH - gapStats};

        // QA polish: slightly smaller action buttons with tighter spacing,
        // grouped as a centred strip that still lines up with the table edges.
        const float stripW = (std::min)(tableW, 5.0f * 150.0f + 4.0f * 6.0f);
        const float btnGap = 6.0f;
        const float btnW = (stripW - btnGap * 4.0f) / 5.0f;
        const float btnY = layout.table.y + layout.table.h + buttonGap;
        const float stripX = layout.table.x + (tableW - stripW) * 0.5f;
        for (int i = 0; i < 5; ++i)
        {
            layout.buttons[i] = {stripX + i * (btnW + btnGap), btnY, btnW, buttonH};
        }

        const float usableW = tableW - 17.0f;
        layout.colOwned = 64.0f;
        layout.colNo = 58.0f;
        layout.colCartridge = 92.0f;
        layout.colBox = 64.0f;
        layout.colManual = 86.0f;
        layout.colWanted = 64.0f;
        // No condition column: component conditions live in Game Details. All
        // remaining width goes to the title.
        float remaining = usableW - (layout.colOwned + layout.colNo +
            layout.colCartridge + layout.colBox + layout.colManual +
            layout.colWanted);
        remaining = (std::max)(remaining, 120.0f);
        layout.colTitle = remaining;
        return layout;
    }
}

// ===========================================================================
// CollectionPage
// ===========================================================================

CollectionPage::CollectionPage() = default;

void CollectionPage::Initialize(const std::filesystem::path& basePath,
    const GameLibrary* library, const GameDatabase* gameDatabase,
    SDL_Window* window)
{
    basePath_ = basePath;
    library_ = library;
    gameDatabase_ = gameDatabase;
    window_ = window;
    database_.SetBasePath(basePath);

    std::string message;
    if (database_.Initialize(message))
        std::printf("O2EM-NG: %s\n", message.c_str());
    else
        std::printf("O2EM-NG: WARNING: %s\n", message.c_str());

    RefreshReferences();
    Refresh();
}

void CollectionPage::RefreshReferences()
{
    gameIdByFilename_.clear();
    filenameByGameId_.clear();

    // Read-only identity map from the main preservation database. Used to
    // store/resolve the optional reference_game_id; we never write there.
    if (gameDatabase_)
    {
        for (const GameIdentity& identity : gameDatabase_->LoadGameIdentityMap())
        {
            if (identity.romFilename.empty())
                continue;
            gameIdByFilename_[ToLowerCopy(identity.romFilename)] = identity.id;
            filenameByGameId_[identity.id] = identity.romFilename;
        }
    }

    refOrder_.clear();
    if (library_)
    {
        const std::vector<GameInfo>& games = library_->Games();
        refOrder_.reserve(games.size());
        for (int i = 0; i < static_cast<int>(games.size()); ++i)
            refOrder_.push_back(i);
        std::stable_sort(refOrder_.begin(), refOrder_.end(), [&](int a, int b)
        {
            const GameInfo& ga = games[a];
            const GameInfo& gb = games[b];
            const CatalogKey ka = MakeCatalogKey(ga.catalogId.empty()
                ? DisplayCatalogId(&ga) : ga.catalogId);
            const CatalogKey kb = MakeCatalogKey(gb.catalogId.empty()
                ? DisplayCatalogId(&gb) : gb.catalogId);
            if (ka.group != kb.group) return ka.group < kb.group;
            if (ka.number != kb.number) return ka.number < kb.number;
            if (ka.variant != kb.variant) return ka.variant < kb.variant;
            if (ka.text != kb.text) return ka.text < kb.text;
            return ga.title < gb.title;
        });
    }
}

CollectionPage::CatalogKey CollectionPage::MakeCatalogKey(
    const std::string& catalogId) const
{
    CatalogKey key;
    std::string id = catalogId;
    if (id.empty())
    {
        key.text = id;
        return key;
    }

    const std::string upper = ToLowerCopy(id);
    std::size_t pos = 0;
    while (pos < upper.size() && std::isdigit(static_cast<unsigned char>(upper[pos])))
        ++pos;
    if (pos > 0)
    {
        key.group = 0;
        key.number = std::stoi(upper.substr(0, pos));
        key.variant = upper.find('+', pos) != std::string::npos ? 1 : 0;
        key.text = upper;
        return key;
    }
    if (upper.size() > 1 && upper[0] == 'c')
    {
        std::size_t digit = 1;
        while (digit < upper.size() && std::isdigit(static_cast<unsigned char>(upper[digit])))
            ++digit;
        if (digit > 1)
        {
            key.group = 1;
            key.number = std::stoi(upper.substr(1, digit - 1));
            key.text = upper;
            return key;
        }
    }
    key.text = upper;
    return key;
}

const GameInfo* CollectionPage::FindReferenceGame(const CollectionEntry& entry) const
{
    if (!library_)
        return nullptr;

    // Resolution order: reference_game_id -> reference_rom_filename -> catalog.
    if (entry.referenceGameId > 0)
    {
        const auto byId = filenameByGameId_.find(entry.referenceGameId);
        if (byId != filenameByGameId_.end())
        {
            for (const GameInfo& game : library_->Games())
                if (game.filename == byId->second)
                    return &game;
        }
    }
    if (!entry.referenceRomFilename.empty())
    {
        for (const GameInfo& game : library_->Games())
            if (game.filename == entry.referenceRomFilename)
                return &game;
    }
    if (!entry.catalogId.empty())
    {
        for (const GameInfo& game : library_->Games())
            if (!game.catalogId.empty() &&
                ToLowerCopy(game.catalogId) == ToLowerCopy(entry.catalogId))
                return &game;
    }
    return nullptr;
}

std::string CollectionPage::ResolvedTitle(const CollectionEntry& entry) const
{
    if (!entry.title.empty())
        return entry.title;
    if (const GameInfo* reference = FindReferenceGame(entry))
        return reference->title;
    return {};
}

bool CollectionPage::MatchesSearch(const CollectionEntry& entry) const
{
    if (search_.empty())
        return true;
    if (ContainsInsensitive(entry.catalogId, search_) ||
        ContainsInsensitive(entry.title, search_) ||
        ContainsInsensitive(entry.region, search_) ||
        ContainsInsensitive(entry.purchaseSource, search_) ||
        ContainsInsensitive(entry.notes, search_) ||
        ContainsInsensitive(entry.storageLocation, search_) ||
        ContainsInsensitive(entry.cartridgeCondition, search_) ||
        ContainsInsensitive(entry.boxCondition, search_) ||
        ContainsInsensitive(entry.manualCondition, search_))
        return true;
    if (const GameInfo* reference = FindReferenceGame(entry))
        return ContainsInsensitive(reference->title, search_);
    return false;
}

void CollectionPage::Refresh()
{
    // Capture the selected entry's stable id before the entry vector is
    // replaced, so a reload (add/edit/remove) cannot leave stale view indices
    // pointing past the new, smaller vector.
    long long keepId = 0;
    const int previousIndex = SelectedEntryIndex();
    if (previousIndex >= 0)
        keepId = entries_[previousIndex].id;

    entries_ = database_.LoadEntries();
    stats_ = CollectionDatabase::ComputeStats(entries_);
    preserveSelectedId_ = keepId;
    RebuildView();
}

void CollectionPage::RebuildView()
{
    long long previousId = preserveSelectedId_;
    preserveSelectedId_ = 0;
    if (previousId == 0)
    {
        const int previousIndex = SelectedEntryIndex();
        if (previousIndex >= 0)
            previousId = entries_[previousIndex].id;
    }

    view_.clear();
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (MatchesSearch(entries_[i]))
            view_.push_back(i);

    const auto catalogLess = [&](const CollectionEntry& a, const CollectionEntry& b)
    {
        const CatalogKey ka = MakeCatalogKey(a.catalogId.empty()
            ? ResolvedTitle(a) : a.catalogId);
        const CatalogKey kb = MakeCatalogKey(b.catalogId.empty()
            ? ResolvedTitle(b) : b.catalogId);
        if (ka.group != kb.group) return ka.group < kb.group;
        if (ka.number != kb.number) return ka.number < kb.number;
        if (ka.variant != kb.variant) return ka.variant < kb.variant;
        if (ka.text != kb.text) return ka.text < kb.text;
        return ToLowerCopy(ResolvedTitle(a)) < ToLowerCopy(ResolvedTitle(b));
    };

    std::stable_sort(view_.begin(), view_.end(), [&](int ia, int ib)
    {
        const CollectionEntry& a = entries_[ia];
        const CollectionEntry& b = entries_[ib];
        int comparison = 0;
        switch (sortColumn_)
        {
        case 0: // Owned
            comparison = (a.owned ? 1 : 0) - (b.owned ? 1 : 0);
            break;
        case 1: // No.
            comparison = catalogLess(a, b) ? -1 : (catalogLess(b, a) ? 1 : 0);
            break;
        case 2: // Title
        {
            const std::string ta = ToLowerCopy(ResolvedTitle(a));
            const std::string tb = ToLowerCopy(ResolvedTitle(b));
            comparison = ta < tb ? -1 : (ta > tb ? 1 : 0);
            break;
        }
        case 3: // Cartridge
            comparison = (a.cartridge ? 1 : 0) - (b.cartridge ? 1 : 0);
            break;
        case 4: // Box
            comparison = (a.box ? 1 : 0) - (b.box ? 1 : 0);
            break;
        case 5: // Manual
            comparison = (a.manual ? 1 : 0) - (b.manual ? 1 : 0);
            break;
        case 6: // Wanted
            comparison = (a.wanted ? 1 : 0) - (b.wanted ? 1 : 0);
            break;
        default:
            comparison = 0;
            break;
        }
        if (comparison == 0)
            comparison = catalogLess(a, b) ? -1 : (catalogLess(b, a) ? 1 : 0);
        return sortAscending_ ? comparison < 0 : comparison > 0;
    });

    if (previousId != 0)
    {
        for (int i = 0; i < static_cast<int>(view_.size()); ++i)
        {
            if (entries_[view_[i]].id == previousId)
            {
                selected_ = i;
                break;
            }
        }
    }
    ClampSelection();
}

void CollectionPage::ClampSelection()
{
    const int count = static_cast<int>(view_.size());
    if (count == 0)
    {
        selected_ = -1;
        scroll_ = 0;
        return;
    }
    if (selected_ < 0 || selected_ >= count)
        selected_ = 0;
    scroll_ = (std::max)(0, scroll_);
}

int CollectionPage::SelectedEntryIndex() const
{
    if (selected_ < 0 || selected_ >= static_cast<int>(view_.size()))
        return -1;
    const int entryIndex = view_[selected_];
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries_.size()))
        return -1;
    return entryIndex;
}

CollectionEntry* CollectionPage::SelectedEntry()
{
    const int index = SelectedEntryIndex();
    if (index < 0)
        return nullptr;
    return &entries_[index];
}

void CollectionPage::SelectEntryById(long long id)
{
    for (int i = 0; i < static_cast<int>(view_.size()); ++i)
    {
        if (entries_[view_[i]].id == id)
        {
            selected_ = i;
            return;
        }
    }
}

void CollectionPage::SetStatus(std::string text)
{
    status_ = std::move(text);
    if (!status_.empty())
        std::printf("O2EM-NG: My Collection - %s\n", status_.c_str());
}

SDL_FRect CollectionPage::ContentRect() const
{
    if (!window_)
        return {0.0f, 0.0f, 0.0f, 0.0f};
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
    return panels.rightContent;
}

// ---------------------------------------------------------------------------
// Drawing: main page
// ---------------------------------------------------------------------------
void CollectionPage::Draw(SDL_Renderer* renderer, const SDL_FRect& content)
{
    if (!renderer)
        return;

    const MyCollectionLayout layout = LayoutMyCollection(content);

    const float margin = 14.0f;
    const SDL_FRect frame{content.x + margin, content.y + margin,
        content.w - margin * 2.0f, content.h - margin * 2.0f};
    DrawSunkenFrame(renderer, frame);
    Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
    SDL_RenderFillRect(renderer, &layout.inner);

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    UiFont_DrawText(renderer, layout.inner.x + 22.0f, layout.inner.y + 11.0f,
        31.0f, "My Collection");
    UiFont_DrawText(renderer, layout.inner.x + 24.0f, layout.inner.y + 52.0f,
        15.0f,
        "Manage your own physical Videopac collection "
        "(separate from the main game database).");

    // Search field.
    {
        DrawField(renderer, layout.search, search_, false);
        if (search_.empty() && !searchFocused_)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            UiFont_DrawText(renderer, layout.search.x + 6.0f,
                layout.search.y + 5.0f, 13.0f, "Search my collection...");
        }
        DrawMagnifier(renderer, layout.search.x + layout.search.w - 16.0f,
            layout.search.y + layout.search.h * 0.5f - 1.0f);
        if (searchFocused_)
        {
            const std::size_t caret = (std::min)(searchCaret_, search_.size());
            const float caretX = layout.search.x + 5.0f +
                8.0f * 0.9f * static_cast<float>(caret);
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            SDL_RenderLine(renderer, caretX, layout.search.y + 3.0f,
                caretX, layout.search.y + layout.search.h - 4.0f);
        }
    }

    const float columns[7] = {
        layout.colOwned, layout.colNo, layout.colTitle,
        layout.colCartridge, layout.colBox, layout.colManual,
        layout.colWanted
    };
    float columnX[7] = {0.0f};
    {
        float cursor = layout.table.x;
        for (int i = 0; i < 7; ++i)
        {
            columnX[i] = cursor;
            cursor += columns[i];
        }
    }
    const float headerRight = layout.table.x + layout.table.w - 17.0f;
    static const char* const headerLabels[7] = {
        "Owned", "No.", "Title", "Cartridge", "Box", "Manual", "Wanted"
    };

    DrawSunkenFrame(renderer, layout.table);
    for (int i = 0; i < 7; ++i)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, columnX[i] + 6.0f, layout.header.y + 4.0f,
            0.95f, headerLabels[i]);
        DrawSortArrow(renderer, columnX[i] + columns[i] - 10.0f,
            layout.header.y + 11.0f);
        if (i == sortColumn_)
        {
            // Mark the active sort column with an up/down caret.
            Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
            const float cx = columnX[i] + columns[i] - 10.0f;
            const float cy = layout.header.y + 19.0f;
            SDL_RenderLine(renderer, cx - 2.0f, cy, cx + 2.0f, cy);
            if (sortAscending_)
                SDL_RenderLine(renderer, cx - 1.0f, cy + 1.0f, cx + 1.0f, cy + 1.0f);
            else
                SDL_RenderLine(renderer, cx - 2.0f, cy, cx, cy - 2.0f);
        }
        if (i < 6)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Light);
            SDL_RenderLine(renderer, columnX[i] + columns[i], layout.header.y,
                columnX[i] + columns[i], layout.header.y + layout.header.h - 1.0f);
        }
    }
    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(renderer, layout.table.x,
        layout.header.y + layout.header.h - 1.0f, headerRight,
        layout.header.y + layout.header.h - 1.0f);

    const int count = static_cast<int>(view_.size());
    const int capacity = layout.visibleRows;
    const int maxScroll = (std::max)(0, count - capacity);
    scroll_ = (std::clamp)(scroll_, 0, maxScroll);
    const int selected = selected_;

    for (int row = 0; row < capacity; ++row)
    {
        const float rowTop = layout.list.y + static_cast<float>(row) * 28.0f;
        const float rowH = (row == capacity - 1)
            ? layout.list.y + layout.list.h - rowTop : 28.0f;
        const SDL_FRect rowRect{layout.table.x, rowTop,
            headerRight - layout.table.x, rowH};
        const int index = scroll_ + row;
        const bool realRow = index < count;
        const bool selectedRow = realRow && index == selected;

        if (selectedRow)
            Win95Theme::SetRenderColor(renderer, Win95Theme::SelectedItem);
        else
            SDL_SetRenderDrawColor(renderer, index % 2 ? 245 : 255,
                index % 2 ? 245 : 255, index % 2 ? 245 : 255, 255);
        SDL_RenderFillRect(renderer, &rowRect);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Light);
        SDL_RenderLine(renderer, rowRect.x, rowTop + rowH - 1.0f,
            rowRect.x + rowRect.w, rowTop + rowH - 1.0f);
        for (int i = 1; i < 7; ++i)
            SDL_RenderLine(renderer, columnX[i], rowTop, columnX[i], rowTop + rowH);

        if (!realRow)
            continue;

        const CollectionEntry& entry = entries_[view_[index]];
        const SDL_Color& textColor = selectedRow
            ? Win95Theme::SelectedItemText : Win95Theme::WindowText;
        const float checkTop = rowTop + (rowH - 14.0f) * 0.5f;

        DrawCheckBox(renderer,
            {columnX[0] + (columns[0] - 14.0f) * 0.5f, checkTop, 14.0f, 14.0f},
            entry.owned);

        Win95Theme::SetRenderColor(renderer, textColor);
        DrawText(renderer, columnX[1] + 6.0f, rowTop + 4.0f, 0.95f, entry.catalogId);

        std::string title = ResolvedTitle(entry);
        const std::size_t titleMax = static_cast<std::size_t>(
            (std::max)(8.0f, columns[2] - 14.0f) / (8.0f * 0.95f));
        title = Utf8Ellipsize(title, titleMax);
        DrawText(renderer, columnX[2] + 6.0f, rowTop + 4.0f, 0.95f, title);

        DrawCheckBox(renderer,
            {columnX[3] + (columns[3] - 14.0f) * 0.5f, checkTop, 14.0f, 14.0f},
            entry.cartridge);
        DrawCheckBox(renderer,
            {columnX[4] + (columns[4] - 14.0f) * 0.5f, checkTop, 14.0f, 14.0f},
            entry.box);
        DrawCheckBox(renderer,
            {columnX[5] + (columns[5] - 14.0f) * 0.5f, checkTop, 14.0f, 14.0f},
            entry.manual);
        // Wanted: a clearly visible wishlist indicator on every row.
        DrawCheckBox(renderer,
            {columnX[6] + (columns[6] - 14.0f) * 0.5f, checkTop, 14.0f, 14.0f},
            entry.wanted);
    }

    if (count == 0)
    {
        // Empty state: subtle first-line hint, not a modern graphic.
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        const std::string message = entries_.empty()
            ? "No collection entries yet."
            : "No entries match the current search.";
        DrawText(renderer, layout.list.x + 12.0f, layout.list.y + 6.0f,
            0.95f, message);
    }

    {
        float thumbH = layout.scrollTrack.h;
        if (count > capacity && count > 0)
            thumbH = (std::max)(20.0f, layout.scrollTrack.h *
                static_cast<float>(capacity) / static_cast<float>(count));
        float thumbY = layout.scrollTrack.y;
        if (maxScroll > 0)
            thumbY += (layout.scrollTrack.h - thumbH) *
                static_cast<float>(scroll_) / static_cast<float>(maxScroll);
        const SDL_FRect thumb{layout.scrollTrack.x, thumbY,
            layout.scrollTrack.w, thumbH};
        DrawWin95VScrollbar(renderer, layout.scrollUp, layout.scrollDown,
            layout.scrollTrack, thumb);
    }

    // Game Details.
    DrawGroupPanel(renderer, layout.details, "Game Details");
    const CollectionEntry* selectedEntry =
        (selected >= 0 && selected < count) ? &entries_[view_[selected]] : nullptr;
    const GameInfo* reference = selectedEntry
        ? FindReferenceGame(*selectedEntry) : nullptr;

    const float dTop = layout.details.y + 30.0f;
    const float dPad = 16.0f;
    const float dLeft = layout.details.x + dPad;
    const float dRight = layout.details.x + layout.details.w - dPad;
    const float detailsWidth = dRight - dLeft;
    const float detailRowH = 24.0f;
    static const char* const detailFieldLabels[3] = {
        "Catalog ID:", "Title:", "Region:"
    };
    static const char* const detailPresenceLabels[3] = {
        "Cartridge:", "Box:", "Manual:"
    };
    static const char* const detailConditionLabels[3] = {
        "Cartridge condition:", "Box condition:", "Manual condition:"
    };
    float detailLabelW = 92.0f;
    {
        float widest = 0.0f;
        const auto measure = [&](const char* label)
        {
            float w = 0.0f, h = 0.0f;
            if (UiFont_MeasureText(16.5f * 0.9f, label, &w, &h))
                widest = (std::max)(widest, w);
        };
        for (const char* label : detailFieldLabels) measure(label);
        for (const char* label : detailPresenceLabels) measure(label);
        for (const char* label : detailConditionLabels) measure(label);
        if (widest > 0.0f)
            detailLabelW = widest + 8.0f;
    }
    // Bigger cover holder. It never squeezes the fields below a usable width.
    // FrontendBoxArt_DrawImage scales the EXISTING artwork proportionally
    // (aspect preserved, centred, letterboxed as needed) - no copies.
    const float minFieldW = 120.0f;
    const float maxCover = detailsWidth - 12.0f - detailLabelW - minFieldW;
    const float coverW = (std::min)({ 150.0f, detailsWidth * 0.42f,
        (std::max)(60.0f, maxCover) });
    const float coverH = coverW * 1.25f;
    const float rightColX = dLeft + coverW + 12.0f;
    const float rightFieldW = (std::max)(minFieldW,
        dRight - (rightColX + detailLabelW));

    const SDL_FRect coverFrame{dLeft, dTop, coverW, coverH};
    DrawSunkenFrame(renderer, coverFrame);
    if (reference && !reference->boxArt.empty())
    {
        FrontendBoxArt_DrawImage(renderer,
            {coverFrame.x + 2.0f, coverFrame.y + 2.0f,
             coverFrame.w - 4.0f, coverFrame.h - 4.0f},
            reference, true);
    }
    else
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, coverFrame.x + 8.0f,
            coverFrame.y + coverFrame.h * 0.5f - 8.0f, 0.8f, "NO COVER");
    }

    std::string titleValue = selectedEntry ? selectedEntry->title : std::string();
    if (titleValue.empty() && reference)
        titleValue = reference->title;
    const std::string fieldValues[3] = {
        selectedEntry ? selectedEntry->catalogId : std::string(),
        titleValue,
        selectedEntry ? selectedEntry->region : std::string()
    };
    const bool fieldCombos[3] = {false, false, true};
    for (int i = 0; i < 3; ++i)
    {
        const float y = dTop + static_cast<float>(i) * detailRowH;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, rightColX, y + 3.0f, 0.9f, detailFieldLabels[i]);
        DrawField(renderer, {rightColX + detailLabelW, y, rightFieldW, 20.0f},
            fieldValues[i], fieldCombos[i]);
    }

    const bool present[3] = {
        selectedEntry ? selectedEntry->cartridge : false,
        selectedEntry ? selectedEntry->box : false,
        selectedEntry ? selectedEntry->manual : false
    };
    const std::string conditions[3] = {
        selectedEntry ? selectedEntry->cartridgeCondition : std::string(),
        selectedEntry ? selectedEntry->boxCondition : std::string(),
        selectedEntry ? selectedEntry->manualCondition : std::string()
    };
    for (int i = 0; i < 3; ++i)
    {
        const float y = dTop + static_cast<float>(3 + i) * detailRowH;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, rightColX, y + 2.0f, 0.9f, detailPresenceLabels[i]);
        DrawCheckBox(renderer, {rightColX + detailLabelW, y + 2.0f, 14.0f, 14.0f},
            present[i]);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, rightColX + detailLabelW + 20.0f, y + 2.0f,
            0.9f, present[i] ? "Yes" : "No");
    }
    // Component conditions, shown separately and never as one ambiguous grade.
    // An absent component reads "Not present" instead of a misleading value.
    for (int i = 0; i < 3; ++i)
    {
        const float y = dTop + static_cast<float>(6 + i) * detailRowH;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, rightColX, y + 3.0f, 0.9f, detailConditionLabels[i]);
        std::string value;
        if (!present[i])
            value = "Not present";
        else
            value = conditions[i].empty() ? "-" : conditions[i];
        DrawField(renderer, {rightColX + detailLabelW, y, rightFieldW, 20.0f},
            value, false);
    }

    const float topSectionH = (std::max)(coverH, 9.0f * detailRowH);
    const float lowerTop = dTop + topSectionH + 8.0f;
    static const char* const lowerRowLabels[3] = {
        "Purchase source:", "Date added:", "Shelf / Storage:"
    };
    float lowerLabelW = 124.0f;
    {
        float widest = 0.0f;
        for (const char* label : lowerRowLabels)
        {
            float w = 0.0f, h = 0.0f;
            if (UiFont_MeasureText(16.5f * 0.92f, label, &w, &h))
                widest = (std::max)(widest, w);
        }
        if (widest > 0.0f)
            lowerLabelW = widest + 10.0f;
    }
    const float lowerFieldX = dLeft + lowerLabelW;
    const float lowerFieldW = dRight - lowerFieldX;

    const auto drawLowerRow = [&](int index, const char* label,
        const std::string& value) -> float
    {
        const float y = lowerTop + static_cast<float>(index) * 28.0f;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, dLeft, y + 3.0f, 0.92f, label);
        DrawField(renderer, {lowerFieldX, y, lowerFieldW, 21.0f}, value, false);
        return y;
    };

    drawLowerRow(0, "Purchase source:",
        selectedEntry ? selectedEntry->purchaseSource : std::string());
    const float dateY = drawLowerRow(1, "Date added:",
        selectedEntry ? selectedEntry->dateAdded : std::string());
    {
        const SDL_FRect calendar{lowerFieldX + lowerFieldW - 22.0f,
            dateY + 1.0f, 21.0f, 19.0f};
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &calendar);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &calendar);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        const SDL_FRect glyph{calendar.x + 5.0f, calendar.y + 6.0f, 11.0f, 9.0f};
        SDL_RenderRect(renderer, &glyph);
        SDL_RenderLine(renderer, glyph.x, glyph.y + 3.0f,
            glyph.x + glyph.w - 1.0f, glyph.y + 3.0f);
        SDL_RenderLine(renderer, glyph.x + 2.0f, glyph.y - 2.0f, glyph.x + 2.0f, glyph.y);
        SDL_RenderLine(renderer, glyph.x + glyph.w - 3.0f, glyph.y - 2.0f,
            glyph.x + glyph.w - 3.0f, glyph.y);
    }
    drawLowerRow(2, "Shelf / Storage:",
        selectedEntry ? selectedEntry->storageLocation : std::string());

    const float notesY = lowerTop + 3.0f * 28.0f;
    const float notesH = (std::max)(24.0f,
        layout.details.y + layout.details.h - 10.0f - notesY);
    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    DrawText(renderer, dLeft, notesY + 3.0f, 0.92f, "Notes:");
    const SDL_FRect notesBox{lowerFieldX, notesY, lowerFieldW, notesH};
    DrawField(renderer, notesBox, "", false);
    {
        const std::string notes = selectedEntry ? selectedEntry->notes : std::string();
        if (!notes.empty())
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            std::istringstream stream(notes);
            std::string line;
            float textY = notesBox.y + 3.0f;
            while (std::getline(stream, line) && textY < notesBox.y + notesBox.h - 11.0f)
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                DrawText(renderer, notesBox.x + 5.0f, textY, 0.8f, line);
                textY += 13.0f;
            }
        }
        const SDL_FRect track{notesBox.x + notesBox.w - 13.0f,
            notesBox.y + 2.0f, 11.0f, notesBox.h - 4.0f};
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &track);
        const SDL_FRect thumb{track.x, track.y, track.w, (std::min)(track.h, 36.0f)};
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderFillRect(renderer, &thumb);
    }

    // Collection Stats (6 real rows).
    DrawGroupPanel(renderer, layout.stats, "Collection Stats");
    const int totalReference = library_ ? static_cast<int>(library_->Count()) : 0;
    const auto percentText = [totalReference](int value) -> std::string
    {
        if (totalReference <= 0)
            return "(0.0%)";
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "(%.1f%%)",
            100.0 * static_cast<double>(value) / static_cast<double>(totalReference));
        return buffer;
    };
    const auto fractionText = [totalReference, &percentText](int value) -> std::string
    {
        return std::to_string(value) + " / " + std::to_string(totalReference) +
            "  " + percentText(value);
    };
    struct StatRow { const char* label; std::string value; };
    const StatRow statRows[6] = {
        {"Owned cartridges:", fractionText(stats_.ownedCartridges)},
        {"Boxed games:", fractionText(stats_.boxedGames)},
        {"Manuals:", fractionText(stats_.manuals)},
        {"Complete (C+B+M):", fractionText(stats_.complete)},
        {"Duplicates:", std::to_string(stats_.duplicates) + "  " +
            percentText(stats_.duplicates)},
        {"Wanted:", std::to_string(stats_.wanted) + "  " +
            percentText(stats_.wanted)}
    };
    const float statsLabelX = layout.stats.x + 14.0f;
    const float statsValueX = statsLabelX + 150.0f;
    const float statsTop = layout.stats.y + 32.0f;
    const float statsRowH = (std::max)(16.0f, (layout.stats.h - 38.0f) / 6.0f);
    for (int i = 0; i < 6; ++i)
    {
        const float y = statsTop + static_cast<float>(i) * statsRowH;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, statsLabelX, y, 0.9f, statRows[i].label);
        DrawText(renderer, statsValueX, y, 0.9f, statRows[i].value);
    }

    // Bottom action buttons. The Mark Wanted label reflects the selection.
    const bool selectedWanted = selectedEntry && selectedEntry->wanted;
    const char* buttonLabels[5] = {
        "Add Entry...", "Edit Entry...", "Remove",
        selectedWanted ? "Unmark Wanted" : "Mark Wanted", "Export List..."
    };
    for (int i = 0; i < 5; ++i)
        DrawWin95ButtonBody(renderer, layout.buttons[i], buttonLabels[i],
            buttonPressed_ == i);

    if (dialogOpen_)
        DrawDialog(renderer, content);
}

// ---------------------------------------------------------------------------
// Drawing: dialog
// ---------------------------------------------------------------------------
void CollectionPage::DrawDialog(SDL_Renderer* renderer, const SDL_FRect& content)
{
    const DialogLayout dlg = LayoutCollectionDialog(content);

    // Drop shadow + panel.
    Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
    const SDL_FRect shadow{dlg.panel.x + 4.0f, dlg.panel.y + 4.0f,
        dlg.panel.w, dlg.panel.h};
    SDL_RenderFillRect(renderer, &shadow);
    DrawRaisedFrame(renderer, dlg.panel);

    Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
    SDL_RenderFillRect(renderer, &dlg.titleBar);
    Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitleText);
    UiFont_DrawText(renderer, dlg.titleBar.x + 8.0f, dlg.titleBar.y + 5.0f, 15.0f,
        dialogEditing_ ? "Edit Collection Entry" : "Add Collection Entry");

    // Reference picker.
    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    DrawText(renderer, dlg.label[DF_RefList].x, dlg.label[DF_RefList].y, 0.9f,
        "Reference (main library)");
    DrawSunkenFrame(renderer, dlg.refList);

    Win95Theme::SetRenderColor(renderer,
        dialogRefSelected_ == -1 ? Win95Theme::SelectedItem : Win95Theme::Window);
    SDL_RenderFillRect(renderer, &dlg.refManualRow);
    Win95Theme::SetRenderColor(renderer,
        dialogRefSelected_ == -1 ? Win95Theme::SelectedItemText : Win95Theme::WindowText);
    DrawText(renderer, dlg.refManualRow.x + 5.0f, dlg.refManualRow.y + 3.0f, 0.9f,
        "[ Manual entry - no reference ]");

    const std::vector<GameInfo>& games = library_ ? library_->Games()
        : std::vector<GameInfo>{};
    const int refCount = static_cast<int>(refOrder_.size());
    const int refMaxScroll = (std::max)(0, refCount - dlg.refRows);
    dialogRefScroll_ = (std::clamp)(dialogRefScroll_, 0, refMaxScroll);
    for (int row = 0; row < dlg.refRows; ++row)
    {
        const int index = dialogRefScroll_ + row;
        if (index >= refCount)
            break;
        const GameInfo& game = games[refOrder_[index]];
        const float rowTop = dlg.gamesList.y + static_cast<float>(row) * 18.0f;
        const SDL_FRect rowRect{dlg.gamesList.x, rowTop, dlg.gamesList.w, 18.0f};
        Win95Theme::SetRenderColor(renderer,
            dialogRefSelected_ == index ? Win95Theme::SelectedItem : Win95Theme::Window);
        SDL_RenderFillRect(renderer, &rowRect);
        Win95Theme::SetRenderColor(renderer,
            dialogRefSelected_ == index ? Win95Theme::SelectedItemText
                                        : Win95Theme::WindowText);
        std::string label = DisplayCatalogId(&game);
        if (!label.empty())
            label += "  ";
        label += game.title;
        const std::size_t maxChars = static_cast<std::size_t>(
            (std::max)(8.0f, dlg.gamesList.w - 10.0f) / (8.0f * 0.85f));
        label = Utf8Ellipsize(label, maxChars);
        DrawText(renderer, dlg.gamesList.x + 5.0f, rowTop + 2.0f, 0.85f, label);
    }

    {
        float thumbH = dlg.refTrack.h;
        if (refCount > dlg.refRows && refCount > 0)
            thumbH = (std::max)(16.0f, dlg.refTrack.h *
                static_cast<float>(dlg.refRows) / static_cast<float>(refCount));
        float thumbY = dlg.refTrack.y;
        if (refMaxScroll > 0)
            thumbY += (dlg.refTrack.h - thumbH) *
                static_cast<float>(dialogRefScroll_) / static_cast<float>(refMaxScroll);
        const SDL_FRect thumb{dlg.refTrack.x, thumbY, dlg.refTrack.w, thumbH};
        DrawWin95VScrollbar(renderer, dlg.refUp, dlg.refDown, dlg.refTrack, thumb);
    }

    // Fields.
    static const char* const fieldLabels[DF_Count] = {
        "", "Catalog ID:", "Title:", "Region:", "Cartridge Condition:",
        "Box Condition:", "Manual Condition:", "Quantity:", "Purchase source:",
        "Purchase date:", "Date added:", "Shelf / Storage:", "Notes:",
        "Owned", "Cartridge", "Box", "Manual", "Wanted", "", ""
    };
    const auto fieldString = [&](int field) -> std::string
    {
        switch (field)
        {
        case DF_CatalogId: return dialogEntry_.catalogId;
        case DF_Title: return dialogEntry_.title;
        case DF_Region: return dialogEntry_.region;
        case DF_CartridgeCondition: return dialogEntry_.cartridgeCondition;
        case DF_BoxCondition: return dialogEntry_.boxCondition;
        case DF_ManualCondition: return dialogEntry_.manualCondition;
        case DF_Quantity: return dialogQuantityText_;
        case DF_PurchaseSource: return dialogEntry_.purchaseSource;
        case DF_PurchaseDate: return dialogEntry_.purchaseDate;
        case DF_DateAdded: return dialogEntry_.dateAdded;
        case DF_Shelf: return dialogEntry_.storageLocation;
        case DF_Notes: return dialogEntry_.notes;
        default: return {};
        }
    };
    const auto fieldChecked = [&](int field) -> bool
    {
        switch (field)
        {
        case DF_Owned: return dialogEntry_.owned;
        case DF_Cartridge: return dialogEntry_.cartridge;
        case DF_Box: return dialogEntry_.box;
        case DF_Manual: return dialogEntry_.manual;
        case DF_Wanted: return dialogEntry_.wanted;
        default: return false;
        }
    };

    for (int field = 1; field < DF_Count; ++field)
    {
        if (field == DF_Ok || field == DF_Cancel)
            continue;
        if (IsDialogCheckboxField(field))
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            DrawCheckBox(renderer, dlg.input[field], fieldChecked(field));
            DrawText(renderer, dlg.label[field].x, dlg.label[field].y, 0.85f,
                fieldLabels[field]);
            continue;
        }
        if (!fieldLabels[field] || fieldLabels[field][0] == '\0')
            continue;
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, dlg.label[field].x, dlg.label[field].y, 0.85f,
            fieldLabels[field]);

        if (field == DF_Notes)
        {
            // Real multiline editor: wrapped-free lines, vertical scroll that
            // follows the caret, and a caret exactly one text line high.
            const bool notesSel = dialogSelActive_ && dialogFocus_ == DF_Notes;
            const std::size_t notesSelA = notesSel
                ? (std::min)(dialogSelAnchor_, dialogCaret_) : 0u;
            const std::size_t notesSelB = notesSel
                ? (std::max)(dialogSelAnchor_, dialogCaret_) : 0u;
            DrawMultilineNotes(renderer, dlg.input[field], dialogEntry_.notes,
                dialogCaret_,
                dialogTextActive_ && dialogFocus_ == DF_Notes,
                dialogNotesScroll_, notesSelA, notesSelB, notesSel);
            if (dialogFocus_ == DF_Notes && !dialogTextActive_)
            {
                Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
                SDL_RenderRect(renderer, &dlg.input[field]);
            }
            continue;
        }

        if (IsConditionField(field) &&
            !ConditionComponentPresent(dialogEntry_, field))
        {
            // Component not present -> condition is disabled and blank. This
            // never leaves stale hidden condition data visible.
            DrawSunkenFrame(renderer, dlg.input[field]);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            DrawText(renderer, dlg.input[field].x + 5.0f,
                dlg.input[field].y + 3.0f, 0.85f, "(not present)");
            if (field == dialogFocus_)
            {
                Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
                SDL_RenderRect(renderer, &dlg.input[field]);
            }
            continue;
        }

        DrawField(renderer, dlg.input[field], fieldString(field),
            IsDialogComboField(field));
        if (dialogSelActive_ && field == dialogFocus_ && IsDialogTextField(field))
        {
            // Lightweight selection feedback: a translucent bar under the text
            // so the black glyphs stay readable.
            const std::string value = fieldString(field);
            const std::size_t a = (std::min)(
                (std::min)(dialogSelAnchor_, dialogCaret_), value.size());
            const std::size_t b = (std::min)(
                (std::max)(dialogSelAnchor_, dialogCaret_), value.size());
            if (b > a)
            {
                const float x1 = dlg.input[field].x + 5.0f + UiWidth13(value.substr(0, a));
                const float x2 = dlg.input[field].x + 5.0f + UiWidth13(value.substr(0, b));
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 128, 110);
                const SDL_FRect highlight{x1, dlg.input[field].y + 3.0f,
                    (std::max)(1.0f, x2 - x1), dlg.input[field].h - 6.0f};
                SDL_RenderFillRect(renderer, &highlight);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        }
        if (dialogTextActive_ && field == dialogFocus_ && IsDialogTextField(field)
            && !dialogSelActive_)
        {
            const std::string value = fieldString(field);
            const std::size_t caret = (std::min)(dialogCaret_, value.size());
            const float caretX = dlg.input[field].x + 5.0f +
                UiWidth13(value.substr(0, caret));
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            const float caretH = (std::min)(14.0f, dlg.input[field].h - 6.0f);
            SDL_RenderLine(renderer, caretX, dlg.input[field].y + 4.0f,
                caretX, dlg.input[field].y + 4.0f + caretH);
        }
        else if (field == dialogFocus_ && !dialogTextActive_)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
            SDL_RenderRect(renderer, &dlg.input[field]);
        }
    }
    // The reference list gets a focus ring too.
    if (dialogFocus_ == DF_RefList)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
        SDL_RenderRect(renderer, &dlg.refList);
    }

    DrawWin95ButtonBody(renderer, dlg.input[DF_Ok], "OK", dialogButtonPressed_ == DF_Ok);
    DrawWin95ButtonBody(renderer, dlg.input[DF_Cancel], "Cancel",
        dialogButtonPressed_ == DF_Cancel);

    // Combo popup.
    if (dialogComboOpen_ >= 0)
    {
        const std::vector<std::string>& items =
            IsConditionField(dialogComboOpen_) ? CollectionDatabase::ConditionValues()
                                               : CollectionDatabase::RegionValues();
        const SDL_FRect box = dlg.input[dialogComboOpen_];
        const float itemH = 20.0f;
        const float popupH = itemH * static_cast<float>(items.size());
        SDL_FRect popup{box.x, box.y + box.h, box.w, popupH};
        if (popup.y + popup.h > dlg.panel.y + dlg.panel.h - 4.0f)
            popup.y = box.y - popupH;
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &popup);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &popup);
        const std::string current = fieldString(dialogComboOpen_);
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const SDL_FRect row{popup.x + 1.0f, popup.y + 1.0f + i * itemH,
                popup.w - 2.0f, itemH};
            const bool isCurrent = ToLowerCopy(items[i]) == ToLowerCopy(current);
            Win95Theme::SetRenderColor(renderer,
                isCurrent ? Win95Theme::SelectedItem : Win95Theme::Window);
            SDL_RenderFillRect(renderer, &row);
            Win95Theme::SetRenderColor(renderer,
                isCurrent ? Win95Theme::SelectedItemText : Win95Theme::WindowText);
            DrawText(renderer, row.x + 5.0f, row.y + 2.0f, 0.9f,
                items[i].empty() ? "(Unknown)" : items[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// Input: main page
// ---------------------------------------------------------------------------
bool CollectionPage::HandleMouseDown(float x, float y, int clicks)
{
    (void)clicks;
    const SDL_FRect content = ContentRect();
    if (content.w <= 0.0f)
        return false;
    if (dialogOpen_)
        return HandleDialogMouseDown(content, x, y);

    const MyCollectionLayout layout = LayoutMyCollection(content);
    const auto inside = [x, y](const SDL_FRect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };
    const auto setSearchFocus = [&](bool on)
    {
        searchFocused_ = on;
        if (window_)
        {
            if (on)
            {
                searchCaret_ = search_.size();
                SDL_StartTextInput(window_);
            }
            else
            {
                SDL_StopTextInput(window_);
            }
        }
    };

    if (inside(layout.search))
    {
        setSearchFocus(true);
        return true;
    }

    // Column header click: sort by that column (toggle direction).
    if (inside(layout.header))
    {
        const float columns[7] = {
            layout.colOwned, layout.colNo, layout.colTitle,
            layout.colCartridge, layout.colBox, layout.colManual,
            layout.colWanted
        };
        float cursor = layout.table.x;
        for (int i = 0; i < 7; ++i)
        {
            if (x < cursor + columns[i])
            {
                if (sortColumn_ == i)
                    sortAscending_ = !sortAscending_;
                else
                {
                    sortColumn_ = i;
                    sortAscending_ = true;
                }
                RebuildView();
                scroll_ = 0;
                break;
            }
            cursor += columns[i];
        }
        return true;
    }

    for (int i = 0; i < 5; ++i)
    {
        if (inside(layout.buttons[i]))
        {
            buttonPressed_ = i;
            status_.clear();
            return true;
        }
    }

    const int count = static_cast<int>(view_.size());
    const int maxScroll = (std::max)(0, count - layout.visibleRows);
    if (inside(layout.scrollUp))
    {
        scroll_ = (std::max)(0, scroll_ - 1);
        return true;
    }
    if (inside(layout.scrollDown))
    {
        scroll_ = (std::min)(maxScroll, scroll_ + 1);
        return true;
    }
    if (inside(layout.scrollTrack))
    {
        const bool pageUp = y < layout.scrollTrack.y + layout.scrollTrack.h * 0.5f;
        scroll_ = (std::clamp)(scroll_ + (pageUp ? -layout.visibleRows
                                                 : layout.visibleRows), 0, maxScroll);
        return true;
    }
    if (inside(layout.list))
    {
        const int row = static_cast<int>((y - layout.list.y) / 28.0f);
        const int index = scroll_ + row;
        if (index >= 0 && index < count)
        {
            selected_ = index;
            status_.clear();
        }
        setSearchFocus(false);
        return true;
    }

    if (inside(content))
    {
        setSearchFocus(false);
        return true;
    }
    return false;
}

bool CollectionPage::HandleMouseUp(float x, float y)
{
    if (dialogOpen_)
        return HandleDialogMouseUp(ContentRect(), x, y);
    if (buttonPressed_ < 0)
        return false;

    const int pressed = buttonPressed_;
    buttonPressed_ = -1;
    const MyCollectionLayout layout = LayoutMyCollection(ContentRect());
    const SDL_FRect& button = layout.buttons[pressed];
    if (!(x >= button.x && x < button.x + button.w &&
          y >= button.y && y < button.y + button.h))
        return true;

    switch (pressed)
    {
    case 0: OpenAddDialog(); break;
    case 1: OpenEditDialog(); break;
    case 2: RemoveSelected(); break;
    case 3: ToggleWanted(); break;
    case 4: ExportCsv(); break;
    default: break;
    }
    return true;
}

bool CollectionPage::HandleMouseWheel(float x, float y, float deltaY)
{
    if (dialogOpen_)
    {
        const DialogLayout dlg = LayoutCollectionDialog(ContentRect());
        const int refCount = static_cast<int>(refOrder_.size());
        const int refMaxScroll = (std::max)(0, refCount - dlg.refRows);
        if (x >= dlg.gamesList.x && x < dlg.gamesList.x + dlg.gamesList.w &&
            y >= dlg.gamesList.y && y < dlg.gamesList.y + dlg.gamesList.h)
        {
            dialogRefScroll_ = (std::clamp)(dialogRefScroll_ +
                (deltaY > 0.0f ? -1 : 1), 0, refMaxScroll);
        }
        return true;
    }

    const SDL_FRect content = ContentRect();
    const MyCollectionLayout layout = LayoutMyCollection(content);
    const auto inside = [x, y](const SDL_FRect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };
    if (inside(layout.list) || inside(layout.scrollTrack))
    {
        const int count = static_cast<int>(view_.size());
        const int maxScroll = (std::max)(0, count - layout.visibleRows);
        scroll_ = (std::clamp)(scroll_ + (deltaY > 0.0f ? -1 : 1), 0, maxScroll);
        return true;
    }
    if (inside(content))
        return true;
    return false;
}

bool CollectionPage::HandleKeyDown(const SDL_KeyboardEvent& event)
{
    if (dialogOpen_)
        return HandleDialogKeyDown(event);

    if (searchFocused_)
    {
        if (event.key == SDLK_BACKSPACE)
        {
            if (searchCaret_ > 0)
            {
                search_.erase(searchCaret_ - 1, 1);
                --searchCaret_;
                RebuildView();
            }
            return true;
        }
        if (event.key == SDLK_DELETE)
        {
            if (searchCaret_ < search_.size())
            {
                search_.erase(searchCaret_, 1);
                RebuildView();
            }
            return true;
        }
        if (event.key == SDLK_LEFT)
        {
            if (searchCaret_ > 0) --searchCaret_;
            return true;
        }
        if (event.key == SDLK_RIGHT)
        {
            if (searchCaret_ < search_.size()) ++searchCaret_;
            return true;
        }
        if (event.key == SDLK_HOME) { searchCaret_ = 0; return true; }
        if (event.key == SDLK_END) { searchCaret_ = search_.size(); return true; }
    }

    const MyCollectionLayout layout = LayoutMyCollection(ContentRect());
    const int count = static_cast<int>(view_.size());
    const int maxScroll = (std::max)(0, count - layout.visibleRows);

    switch (event.key)
    {
    case SDLK_ESCAPE:
        if (searchFocused_)
        {
            search_.clear();
            searchCaret_ = 0;
            searchFocused_ = false;
            if (window_) SDL_StopTextInput(window_);
            RebuildView();
            return true;
        }
        return false;   // let the app return to Library
    case SDLK_UP: MoveSelection(-1); return true;
    case SDLK_DOWN: MoveSelection(1); return true;
    case SDLK_RETURN: ActivateSelected(); return true;
    case SDLK_DELETE: RemoveSelected(); return true;
    case SDLK_PAGEUP: scroll_ = (std::max)(0, scroll_ - layout.visibleRows); return true;
    case SDLK_PAGEDOWN: scroll_ = (std::min)(maxScroll, scroll_ + layout.visibleRows); return true;
    default: break;
    }
    return false;
}

void CollectionPage::MoveSelection(int direction)
{
    const int count = static_cast<int>(view_.size());
    if (count == 0)
        return;
    if (selected_ < 0)
        selected_ = 0;
    else
        selected_ = (std::clamp)(selected_ + direction, 0, count - 1);

    const MyCollectionLayout layout = LayoutMyCollection(ContentRect());
    if (selected_ < scroll_)
        scroll_ = selected_;
    else if (selected_ >= scroll_ + layout.visibleRows)
        scroll_ = selected_ - layout.visibleRows + 1;
}

void CollectionPage::ActivateSelected()
{
    if (SelectedEntry())
        OpenEditDialog();
    else
        SetStatus("Select an entry first.");
}

bool CollectionPage::CancelOrBack()
{
    if (dialogOpen_)
    {
        CloseDialog(window_);
        return true;
    }
    if (searchFocused_)
    {
        search_.clear();
        searchCaret_ = 0;
        searchFocused_ = false;
        if (window_) SDL_StopTextInput(window_);
        RebuildView();
        return true;
    }
    return false;
}

void CollectionPage::Deactivate(SDL_Window* window)
{
    if (dialogOpen_)
        CloseDialog(window);
    if (searchFocused_)
    {
        searchFocused_ = false;
        if (window) SDL_StopTextInput(window);
    }
}

bool CollectionPage::HandleTextInput(const SDL_TextInputEvent& event)
{
    if (dialogOpen_)
        return HandleDialogTextInput(event);
    if (!searchFocused_ || !event.text)
        return false;

    search_.insert(searchCaret_, event.text);
    searchCaret_ += std::char_traits<char>::length(event.text);
    RebuildView();
    return true;
}

// ---------------------------------------------------------------------------
// Input: dialog
// ---------------------------------------------------------------------------
std::string* CollectionPage::DialogFieldString(int field)
{
    switch (field)
    {
    case DF_CatalogId: return &dialogEntry_.catalogId;
    case DF_Title: return &dialogEntry_.title;
    case DF_Quantity: return &dialogQuantityText_;
    case DF_PurchaseSource: return &dialogEntry_.purchaseSource;
    case DF_PurchaseDate: return &dialogEntry_.purchaseDate;
    case DF_DateAdded: return &dialogEntry_.dateAdded;
    case DF_Shelf: return &dialogEntry_.storageLocation;
    case DF_Notes: return &dialogEntry_.notes;
    default: return nullptr;
    }
}

bool CollectionPage::HandleDialogMouseDown(const SDL_FRect& content, float x, float y)
{
    const DialogLayout dlg = LayoutCollectionDialog(content);
    const auto inside = [x, y](const SDL_FRect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    // Combo popups take priority.
    if (dialogComboOpen_ >= 0)
    {
        const std::vector<std::string>& items =
            IsConditionField(dialogComboOpen_) ? CollectionDatabase::ConditionValues()
                                               : CollectionDatabase::RegionValues();
        const SDL_FRect box = dlg.input[dialogComboOpen_];
        const float itemH = 20.0f;
        const float popupH = itemH * static_cast<float>(items.size());
        SDL_FRect popup{box.x, box.y + box.h, box.w, popupH};
        if (popup.y + popup.h > dlg.panel.y + dlg.panel.h - 4.0f)
            popup.y = box.y - popupH;
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const SDL_FRect row{popup.x, popup.y + static_cast<float>(i) * itemH,
                popup.w, itemH};
            if (inside(row))
            {
                if (IsConditionField(dialogComboOpen_))
                    ConditionValueRef(dialogEntry_, dialogComboOpen_) = items[i];
                else
                    dialogEntry_.region = items[i];
                dialogComboOpen_ = -1;
                return true;
            }
        }
        dialogComboOpen_ = -1;
        return true;
    }

    if (inside(dlg.refManualRow))
    {
        dialogRefSelected_ = -1;
        dialogRefTouched_ = true;
        dialogTextActive_ = false;
        return true;
    }
    if (inside(dlg.gamesList))
    {
        const int row = static_cast<int>((y - dlg.gamesList.y) / 18.0f);
        const int index = dialogRefScroll_ + row;
        if (index >= 0 && index < static_cast<int>(refOrder_.size()))
        {
            dialogRefSelected_ = index;
            dialogRefTouched_ = true;
            if (library_)
            {
                const GameInfo* game = library_->Get(
                    static_cast<std::size_t>(refOrder_[index]));
                if (game)
                {
                    dialogEntry_.referenceRomFilename = game->filename;
                    const auto found = gameIdByFilename_.find(
                        ToLowerCopy(game->filename));
                    dialogEntry_.referenceGameId = found != gameIdByFilename_.end()
                        ? found->second : 0;
                    const std::string catalog = DisplayCatalogId(game);
                    if (!catalog.empty())
                        dialogEntry_.catalogId = catalog;
                    dialogEntry_.title = game->title;
                }
            }
        }
        return true;
    }
    if (inside(dlg.refUp))
    {
        dialogRefScroll_ = (std::max)(0, dialogRefScroll_ - 1);
        return true;
    }
    if (inside(dlg.refDown))
    {
        const int refCount = static_cast<int>(refOrder_.size());
        const int refMax = (std::max)(0, refCount - dlg.refRows);
        dialogRefScroll_ = (std::min)(refMax, dialogRefScroll_ + 1);
        return true;
    }
    if (inside(dlg.refTrack))
    {
        const int refCount = static_cast<int>(refOrder_.size());
        const int refMax = (std::max)(0, refCount - dlg.refRows);
        const bool up = y < dlg.refTrack.y + dlg.refTrack.h * 0.5f;
        dialogRefScroll_ = (std::clamp)(dialogRefScroll_ +
            (up ? -dlg.refRows : dlg.refRows), 0, refMax);
        return true;
    }

    if (inside(dlg.input[DF_Ok])) { dialogButtonPressed_ = DF_Ok; return true; }
    if (inside(dlg.input[DF_Cancel])) { dialogButtonPressed_ = DF_Cancel; return true; }

    for (int field = DF_Owned; field <= DF_Wanted; ++field)
    {
        if (inside(dlg.input[field]))
        {
            bool* value = nullptr;
            switch (field)
            {
            case DF_Owned: value = &dialogEntry_.owned; break;
            case DF_Cartridge: value = &dialogEntry_.cartridge; break;
            case DF_Box: value = &dialogEntry_.box; break;
            case DF_Manual: value = &dialogEntry_.manual; break;
            case DF_Wanted: value = &dialogEntry_.wanted; break;
            default: break;
            }
            if (value) *value = !*value;
            dialogFocus_ = field;
            dialogTextActive_ = false;
            dialogComboOpen_ = -1;
            return true;
        }
    }

    if (inside(dlg.input[DF_Region]))
    {
        dialogFocus_ = DF_Region;
        dialogComboOpen_ = DF_Region;
        dialogTextActive_ = false;
        return true;
    }
    for (const int conditionField :
        {DF_CartridgeCondition, DF_BoxCondition, DF_ManualCondition})
    {
        if (inside(dlg.input[conditionField]))
        {
            dialogFocus_ = conditionField;
            // Only an existing component can have a condition; disabling keeps
            // stale hidden data impossible.
            if (ConditionComponentPresent(dialogEntry_, conditionField))
                dialogComboOpen_ = conditionField;
            else
                dialogComboOpen_ = -1;
            dialogTextActive_ = false;
            return true;
        }
    }

    for (int field = 1; field < DF_Count; ++field)
    {
        if (!IsDialogTextField(field))
            continue;
        if (inside(dlg.input[field]))
        {
            dialogFocus_ = field;
            dialogTextActive_ = true;
            std::string* target = DialogFieldString(field);
            if (target && field == DF_Notes)
            {
                // Place the caret at the clicked position inside the notes.
                dialogCaret_ = NotesCaretFromPoint(*target, dlg.input[field],
                    x, y, dialogNotesScroll_);
            }
            else if (target)
            {
                // Place the caret at the clicked character position.
                const float targetX = x - (dlg.input[field].x + 5.0f);
                if (targetX <= 0.0f)
                {
                    dialogCaret_ = 0;
                }
                else if (targetX >= UiWidth13(*target))
                {
                    dialogCaret_ = target->size();
                }
                else
                {
                    std::size_t pos = 0;
                    while (pos < target->size())
                    {
                        const std::size_t next = Utf8Next(*target, pos);
                        if (next == pos)
                            break;
                        if (UiWidth13(target->substr(0, next)) > targetX)
                            break;
                        pos = next;
                    }
                    dialogCaret_ = pos;
                }
            }
            else
            {
                dialogCaret_ = 0;
            }
            dialogSelAnchor_ = dialogCaret_;
            dialogSelActive_ = false;
            dialogComboOpen_ = -1;
            if (window_) SDL_StartTextInput(window_);
            return true;
        }
    }

    if (inside(dlg.panel))
    {
        dialogTextActive_ = false;
        if (window_) SDL_StopTextInput(window_);
        return true;
    }
    return true;   // modal: swallow everything
}

bool CollectionPage::HandleDialogMouseUp(const SDL_FRect& content, float x, float y)
{
    if (dialogButtonPressed_ < 0)
        return true;
    const int pressed = dialogButtonPressed_;
    dialogButtonPressed_ = -1;
    const DialogLayout dlg = LayoutCollectionDialog(content);
    const SDL_FRect& button = dlg.input[pressed];
    if (!(x >= button.x && x < button.x + button.w &&
          y >= button.y && y < button.y + button.h))
        return true;
    if (pressed == DF_Ok)
        SaveDialog();
    else
        CloseDialog(window_);
    return true;
}

bool CollectionPage::HandleDialogKeyDown(const SDL_KeyboardEvent& event)
{
    const bool shift = (event.mod & SDL_KMOD_SHIFT) != 0;
    const bool ctrl = (event.mod & SDL_KMOD_CTRL) != 0;
    const auto selMin = [this]()
    {
        return (std::min)(dialogSelAnchor_, dialogCaret_);
    };
    const auto selMax = [this]()
    {
        return (std::max)(dialogSelAnchor_, dialogCaret_);
    };

    // Standard editing shortcuts for EVERY editable field in the dialog.
    // SDL text input does not deliver these and the global frontend shortcuts
    // must not steal them while a collection field is focused.
    if (dialogTextActive_ && IsDialogTextField(dialogFocus_))
    {
        std::string* text = DialogFieldString(dialogFocus_);
        if (text)
        {
            dialogCaret_ = (std::min)(dialogCaret_, text->size());
            if (ctrl && event.key == SDLK_A)
            {
                dialogSelAnchor_ = 0;
                dialogCaret_ = text->size();
                dialogSelActive_ = true;
                return true;
            }
            if (ctrl && (event.key == SDLK_C || event.key == SDLK_X))
            {
                const std::size_t first = dialogSelActive_ ? selMin() : 0u;
                const std::size_t last = dialogSelActive_ ? selMax() : text->size();
                if (last > first)
                    SDL_SetClipboardText(text->substr(first, last - first).c_str());
                if (event.key == SDLK_X && last > first)
                {
                    text->erase(first, last - first);
                    dialogCaret_ = first;
                    dialogSelActive_ = false;
                }
                return true;
            }
            if ((ctrl && event.key == SDLK_V) || (shift && event.key == SDLK_INSERT))
            {
                if (SDL_HasClipboardText())
                {
                    char* clipboardText = SDL_GetClipboardText();
                    if (clipboardText)
                    {
                        if (dialogSelActive_)
                        {
                            const std::size_t first = selMin();
                            const std::size_t last = selMax();
                            text->erase(first, last - first);
                            dialogCaret_ = first;
                            dialogSelActive_ = false;
                        }
                        std::string pasted = SanitizeClipboardText(clipboardText,
                            dialogFocus_ == DF_Notes);
                        SDL_free(clipboardText);
                        const std::size_t insertAt = (std::min)(dialogCaret_, text->size());
                        text->insert(insertAt, pasted);
                        dialogCaret_ = insertAt + pasted.size();
                    }
                }
                return true;
            }
        }
    }

    switch (event.key)
    {
    case SDLK_ESCAPE:
        if (dialogComboOpen_ >= 0)
        {
            dialogComboOpen_ = -1;
            return true;
        }
        if (dialogTextActive_)
        {
            dialogTextActive_ = false;
            dialogSelActive_ = false;
            if (window_) SDL_StopTextInput(window_);
            return true;
        }
        CloseDialog(window_);
        return true;

    case SDLK_TAB:
    {
        dialogTextActive_ = false;
        dialogSelActive_ = false;
        dialogComboOpen_ = -1;
        if (window_) SDL_StopTextInput(window_);
        dialogFocus_ = (dialogFocus_ + (shift ? DF_Count - 1 : 1)) % DF_Count;
        return true;
    }

    case SDLK_UP:
    case SDLK_DOWN:
    {
        const int direction = (event.key == SDLK_DOWN) ? 1 : -1;
        if (dialogComboOpen_ >= 0)
        {
            dialogComboOpen_ = -1;
            return true;
        }
        // Notes is a real multiline editor: Up/Down move between lines and
        // keep the visual column where possible.
        if (dialogFocus_ == DF_Notes && dialogTextActive_)
        {
            std::string* text = DialogFieldString(DF_Notes);
            if (text)
            {
                dialogCaret_ = (std::min)(dialogCaret_, text->size());
                dialogSelActive_ = false;
                const int line = Utf8CaretLine(*text, dialogCaret_);
                const int totalLines = Utf8LineCount(*text);
                const int targetLine = line + direction;
                if (targetLine < 0)
                {
                    dialogCaret_ = 0;
                }
                else if (targetLine >= totalLines)
                {
                    dialogCaret_ = text->size();
                }
                else
                {
                    std::size_t lineStart = 0, lineEnd = 0;
                    std::size_t targetStart = 0, targetEnd = 0;
                    Utf8LineRange(*text, line, lineStart, lineEnd);
                    Utf8LineRange(*text, targetLine, targetStart, targetEnd);
                    std::size_t column = dialogCaret_ - lineStart;
                    if (column > lineEnd - lineStart)
                        column = lineEnd - lineStart;
                    std::size_t pos = targetStart + column;
                    while (pos > targetStart && pos < targetEnd &&
                        Utf8Continuation(static_cast<unsigned char>((*text)[pos])))
                        --pos;
                    dialogCaret_ = pos;
                }
            }
            return true;
        }
        if (dialogFocus_ == DF_RefList)
        {
            const int refCount = static_cast<int>(refOrder_.size());
            const int current = dialogRefSelected_;
            int next = current + direction;
            if (next < -1) next = -1;
            if (next >= refCount) next = refCount - 1;
            dialogRefSelected_ = next;
            dialogRefTouched_ = true;
            if (current == -1 && next == 0 && next < refCount && library_)
            {
                const GameInfo* game = library_->Get(
                    static_cast<std::size_t>(refOrder_[next]));
                if (game)
                {
                    dialogEntry_.referenceRomFilename = game->filename;
                    dialogEntry_.catalogId = DisplayCatalogId(game);
                    dialogEntry_.title = game->title;
                }
            }
            return true;
        }
        dialogTextActive_ = false;
        dialogSelActive_ = false;
        if (window_) SDL_StopTextInput(window_);
        dialogFocus_ = (dialogFocus_ + direction + DF_Count) % DF_Count;
        return true;
    }

    case SDLK_LEFT:
    case SDLK_RIGHT:
    {
        if (dialogTextActive_ && IsDialogTextField(dialogFocus_))
        {
            std::string* target = DialogFieldString(dialogFocus_);
            if (target)
            {
                dialogCaret_ = (std::min)(dialogCaret_, target->size());
                const bool left = (event.key == SDLK_LEFT);
                if (shift)
                {
                    if (!dialogSelActive_)
                    {
                        dialogSelAnchor_ = dialogCaret_;
                        dialogSelActive_ = true;
                    }
                    dialogCaret_ = left ? Utf8Prev(*target, dialogCaret_)
                                        : Utf8Next(*target, dialogCaret_);
                }
                else if (dialogSelActive_)
                {
                    dialogCaret_ = left ? selMin() : selMax();
                    dialogSelActive_ = false;
                }
                else
                {
                    dialogCaret_ = left ? Utf8Prev(*target, dialogCaret_)
                                        : Utf8Next(*target, dialogCaret_);
                }
            }
            return true;
        }
        dialogSelActive_ = false;
        dialogFocus_ = (dialogFocus_ + (event.key == SDLK_RIGHT ? 1 : DF_Count - 1)) % DF_Count;
        return true;
    }

    case SDLK_HOME:
    case SDLK_END:
        if (dialogTextActive_ && IsDialogTextField(dialogFocus_))
        {
            std::string* target = DialogFieldString(dialogFocus_);
            if (target)
            {
                dialogCaret_ = (std::min)(dialogCaret_, target->size());
                std::size_t newCaret = 0;
                if (dialogFocus_ == DF_Notes)
                {
                    std::size_t start = 0, end = 0;
                    Utf8LineRange(*target, Utf8CaretLine(*target, dialogCaret_),
                        start, end);
                    newCaret = (event.key == SDLK_HOME) ? start : end;
                }
                else
                {
                    newCaret = (event.key == SDLK_HOME) ? 0 : target->size();
                }
                if (shift)
                {
                    if (!dialogSelActive_)
                    {
                        dialogSelAnchor_ = dialogCaret_;
                        dialogSelActive_ = true;
                    }
                    dialogCaret_ = newCaret;
                }
                else
                {
                    dialogCaret_ = newCaret;
                    dialogSelActive_ = false;
                }
            }
        }
        return true;

    case SDLK_BACKSPACE:
    case SDLK_DELETE:
        if (dialogTextActive_ && IsDialogTextField(dialogFocus_))
        {
            std::string* target = DialogFieldString(dialogFocus_);
            if (target)
            {
                dialogCaret_ = (std::min)(dialogCaret_, target->size());
                if (dialogSelActive_)
                {
                    const std::size_t first = selMin();
                    const std::size_t last = selMax();
                    if (last > first)
                        target->erase(first, last - first);
                    dialogCaret_ = (std::min)(first, target->size());
                    dialogSelActive_ = false;
                }
                else if (event.key == SDLK_BACKSPACE)
                {
                    if (dialogCaret_ > 0)
                    {
                        const std::size_t previous = Utf8Prev(*target, dialogCaret_);
                        target->erase(previous, dialogCaret_ - previous);
                        dialogCaret_ = previous;
                    }
                }
                else if (dialogCaret_ < target->size())
                {
                    const std::size_t next = Utf8Next(*target, dialogCaret_);
                    target->erase(dialogCaret_, next - dialogCaret_);
                }
            }
        }
        return true;

    case SDLK_SPACE:
        if (IsDialogCheckboxField(dialogFocus_))
        {
            bool* value = nullptr;
            switch (dialogFocus_)
            {
            case DF_Owned: value = &dialogEntry_.owned; break;
            case DF_Cartridge: value = &dialogEntry_.cartridge; break;
            case DF_Box: value = &dialogEntry_.box; break;
            case DF_Manual: value = &dialogEntry_.manual; break;
            case DF_Wanted: value = &dialogEntry_.wanted; break;
            default: break;
            }
            if (value) *value = !*value;
        }
        return true;

    case SDLK_RETURN:
        if (dialogFocus_ == DF_Ok) { SaveDialog(); return true; }
        if (dialogFocus_ == DF_Cancel) { CloseDialog(window_); return true; }
        if (IsDialogCheckboxField(dialogFocus_))
        {
            bool* value = nullptr;
            switch (dialogFocus_)
            {
            case DF_Owned: value = &dialogEntry_.owned; break;
            case DF_Cartridge: value = &dialogEntry_.cartridge; break;
            case DF_Box: value = &dialogEntry_.box; break;
            case DF_Manual: value = &dialogEntry_.manual; break;
            case DF_Wanted: value = &dialogEntry_.wanted; break;
            default: break;
            }
            if (value) *value = !*value;
            return true;
        }
        if (IsDialogComboField(dialogFocus_))
        {
            dialogComboOpen_ = dialogFocus_;
            return true;
        }
        if (IsDialogTextField(dialogFocus_))
        {
            if (!dialogTextActive_)
            {
                dialogTextActive_ = true;
                dialogSelActive_ = false;
                std::string* target = DialogFieldString(dialogFocus_);
                dialogCaret_ = target ? target->size() : 0;
                if (window_) SDL_StartTextInput(window_);
            }
            else if (dialogFocus_ == DF_Notes)
            {
                std::string* target = DialogFieldString(dialogFocus_);
                if (target)
                {
                    if (dialogSelActive_)
                    {
                        const std::size_t first = (std::min)(dialogSelAnchor_, dialogCaret_);
                        const std::size_t last = (std::max)(dialogSelAnchor_, dialogCaret_);
                        target->erase(first, last - first);
                        dialogCaret_ = first;
                        dialogSelActive_ = false;
                    }
                    dialogCaret_ = (std::min)(dialogCaret_, target->size());
                    target->insert(dialogCaret_, "\n");
                    ++dialogCaret_;
                }
            }
            else
            {
                dialogTextActive_ = false;
                if (window_) SDL_StopTextInput(window_);
            }
            return true;
        }
        return true;

    default:
        return true;
    }
}

bool CollectionPage::HandleDialogTextInput(const SDL_TextInputEvent& event)
{
    if (!dialogOpen_ || !dialogTextActive_ || !event.text)
        return false;
    if (!IsDialogTextField(dialogFocus_))
        return false;
    std::string* target = DialogFieldString(dialogFocus_);
    if (!target)
        return false;

    std::string text = event.text;
    if (dialogFocus_ == DF_Quantity)
    {
        std::string digits;
        for (char c : text)
            if (std::isdigit(static_cast<unsigned char>(c)))
                digits += c;
        text = digits;
        if (text.empty())
            return true;
    }
    text = SanitizeClipboardText(text, dialogFocus_ == DF_Notes);
    if (dialogSelActive_)
    {
        const std::size_t first = (std::min)(dialogSelAnchor_, dialogCaret_);
        const std::size_t last = (std::max)(dialogSelAnchor_, dialogCaret_);
        const std::size_t a = (std::min)(first, target->size());
        const std::size_t b = (std::min)(last, target->size());
        target->erase(a, b - a);
        dialogCaret_ = a;
        dialogSelActive_ = false;
    }
    dialogCaret_ = (std::min)(dialogCaret_, target->size());
    target->insert(dialogCaret_, text);
    dialogCaret_ += text.size();
    return true;
}

// ---------------------------------------------------------------------------
// Dialog open/save/close and collection actions
// ---------------------------------------------------------------------------
void CollectionPage::OpenAddDialog()
{
    dialogEntry_ = CollectionEntry{};
    dialogEntry_.owned = true;
    dialogEntry_.cartridge = true;
    dialogEntry_.box = false;
    dialogEntry_.manual = false;
    dialogEntry_.wanted = false;
    dialogEntry_.quantity = 1;
    dialogEntry_.dateAdded = TodayDate();
    dialogQuantityText_ = "1";
    dialogEntryId_ = 0;
    dialogEditing_ = false;
    dialogRefSelected_ = -1;
    dialogRefTouched_ = false;
    dialogRefScroll_ = 0;
    dialogFocus_ = DF_RefList;
    dialogTextActive_ = false;
    dialogSelActive_ = false;
    dialogNotesScroll_ = 0;
    dialogComboOpen_ = -1;
    dialogButtonPressed_ = -1;
    status_.clear();
    dialogOpen_ = true;
}

void CollectionPage::OpenEditDialog()
{
    const CollectionEntry* selected = SelectedEntry();
    if (!selected)
    {
        SetStatus("Select an entry first.");
        return;
    }
    if (library_)
        RefreshReferences();

    dialogEntry_ = *selected;
    dialogEntryId_ = selected->id;
    dialogQuantityText_ = std::to_string(dialogEntry_.quantity);
    dialogEditing_ = true;
    dialogRefTouched_ = false;
    dialogRefScroll_ = 0;
    dialogFocus_ = DF_Title;
    dialogTextActive_ = false;
    dialogSelActive_ = false;
    dialogNotesScroll_ = 0;
    dialogComboOpen_ = -1;
    dialogButtonPressed_ = -1;
    status_.clear();

    // Locate the existing link in the reference order.
    dialogRefSelected_ = -1;
    if (!dialogEntry_.referenceRomFilename.empty() ||
        dialogEntry_.referenceGameId > 0)
    {
        for (int i = 0; i < static_cast<int>(refOrder_.size()); ++i)
        {
            const GameInfo* game = library_
                ? library_->Get(static_cast<std::size_t>(refOrder_[i])) : nullptr;
            if (!game)
                continue;
            if ((!dialogEntry_.referenceRomFilename.empty() &&
                 game->filename == dialogEntry_.referenceRomFilename) ||
                (dialogEntry_.referenceGameId > 0 &&
                 gameIdByFilename_.find(ToLowerCopy(game->filename)) !=
                     gameIdByFilename_.end() &&
                 gameIdByFilename_[ToLowerCopy(game->filename)] == dialogEntry_.referenceGameId))
            {
                dialogRefSelected_ = i;
                break;
            }
        }
    }
    dialogOpen_ = true;
}

void CollectionPage::SaveDialog()
{
    const bool wasEditing = dialogEditing_;
    CollectionEntry entry = dialogEntry_;

    // Reference resolution / linkage.
    if (dialogRefTouched_)
    {
        if (dialogRefSelected_ >= 0 && library_ &&
            dialogRefSelected_ < static_cast<int>(refOrder_.size()))
        {
            const GameInfo* game = library_->Get(
                static_cast<std::size_t>(refOrder_[dialogRefSelected_]));
            if (game)
            {
                entry.referenceRomFilename = game->filename;
                const auto found = gameIdByFilename_.find(ToLowerCopy(game->filename));
                entry.referenceGameId = found != gameIdByFilename_.end()
                    ? found->second : 0;
                if (entry.catalogId.empty())
                    entry.catalogId = DisplayCatalogId(game);
                if (entry.title.empty())
                    entry.title = game->title;
            }
        }
        else
        {
            entry.referenceRomFilename.clear();
            entry.referenceGameId = 0;
        }
    }

    if (entry.title.empty())
    {
        SetStatus("A title is required.");
        return;
    }

    // Never persist a condition for a component that is not present: unchecking
    // Cartridge/Box/Manual clears its condition on save.
    if (!entry.cartridge)
        entry.cartridgeCondition.clear();
    if (!entry.box)
        entry.boxCondition.clear();
    if (!entry.manual)
        entry.manualCondition.clear();

    entry.quantity = std::atoi(dialogQuantityText_.c_str());
    if (entry.quantity < 0)
        entry.quantity = 0;
    if (entry.owned && entry.quantity == 0)
        entry.quantity = 1;

    long long savedId = entry.id;
    if (wasEditing)
    {
        if (!database_.UpdateEntry(entry))
        {
            SetStatus("Could not save changes.");
            return;
        }
    }
    else
    {
        savedId = database_.InsertEntry(entry);
        if (savedId <= 0)
        {
            SetStatus("Could not add entry.");
            return;
        }
    }

    CloseDialog(window_);
    Refresh();
    SelectEntryById(savedId);
    // Keep the selected row visible.
    const MyCollectionLayout layout = LayoutMyCollection(ContentRect());
    if (selected_ >= 0 && selected_ >= scroll_ + layout.visibleRows)
        scroll_ = selected_ - layout.visibleRows + 1;
    if (selected_ >= 0 && selected_ < scroll_)
        scroll_ = selected_;
    SetStatus(std::string(wasEditing ? "Updated " : "Added ") +
        ResolvedTitle(entry) + ".");
}

void CollectionPage::CloseDialog(SDL_Window* window)
{
    dialogOpen_ = false;
    dialogTextActive_ = false;
    dialogComboOpen_ = -1;
    dialogButtonPressed_ = -1;
    if (window)
        SDL_StopTextInput(window);
}

void CollectionPage::ToggleWanted()
{
    CollectionEntry* selected = SelectedEntry();
    if (!selected)
    {
        SetStatus("Select an entry first.");
        return;
    }
    const bool wanted = !selected->wanted;
    const std::string title = ResolvedTitle(*selected);
    selected->wanted = wanted;
    if (!database_.UpdateEntry(*selected))
    {
        SetStatus("Could not update the entry.");
        return;
    }
    Refresh();
    SetStatus(std::string(wanted ? "Marked " : "Removed ") + title +
        (wanted ? " as Wanted." : " from Wanted."));
}

void CollectionPage::RemoveSelected()
{
    CollectionEntry* selected = SelectedEntry();
    if (!selected)
    {
        SetStatus("Select an entry first.");
        return;
    }
    const long long id = selected->id;
    const std::string title = ResolvedTitle(*selected);
    const std::string catalog = selected->catalogId;

    std::string message = "Remove this item from My Collection?\n\n";
    if (!catalog.empty())
        message += "No. " + catalog + "  ";
    message += title;
    message += "\n\nOnly the personal collection entry is removed. ROMs, box art, "
        "manuals, screenshots and game metadata are not affected.";

    HWND owner = nullptr;
    if (window_)
        owner = static_cast<HWND>(SDL_GetPointerProperty(
            SDL_GetWindowProperties(window_),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    if (MessageBoxA(owner, message.c_str(),
            "O2EM-NG - Remove from My Collection",
            MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        SetStatus("Remove cancelled.");
        return;
    }

    if (!database_.DeleteEntry(id))
    {
        SetStatus("Could not remove the entry.");
        return;
    }
    Refresh();
    SetStatus("Removed " + title + ".");
}

void CollectionPage::ExportCsv()
{
    if (entries_.empty())
    {
        SetStatus("Nothing to export.");
        return;
    }

    wchar_t filename[32768] = {};
    const std::filesystem::path suggested =
        basePath_ / "GAMEDATA" / "mycollection.csv";
    wcsncpy_s(filename, suggested.wstring().c_str(), _TRUNCATE);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFile = filename;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrDefExt = L"csv";
    dialog.lpstrTitle = L"Export My Collection";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog))
    {
        SetStatus("Export cancelled.");
        return;
    }

    std::ofstream output(std::filesystem::path(filename), std::ios::binary);
    if (!output)
    {
        SetStatus("Could not write the export file.");
        return;
    }

    output << "\xEF\xBB\xBF";   // UTF-8 BOM for spreadsheet compatibility.
    output << "Catalog ID,Title,Region,Owned,Quantity,Cartridge,"
              "Cartridge Condition,Box,Box Condition,Manual,Manual Condition,"
              "Wanted,Purchase Source,Purchase Date,Date Added,"
              "Storage Location,Notes\r\n";

    // Export the current filtered/sorted view. A component that is not present
    // exports a blank condition rather than a misleading value.
    for (int viewIndex : view_)
    {
        const CollectionEntry& entry = entries_[viewIndex];
        output << CsvEscape(entry.catalogId) << ','
               << CsvEscape(ResolvedTitle(entry)) << ','
               << CsvEscape(entry.region) << ','
               << (entry.owned ? 1 : 0) << ','
               << entry.quantity << ','
               << (entry.cartridge ? 1 : 0) << ','
               << CsvEscape(entry.cartridge ? entry.cartridgeCondition : std::string()) << ','
               << (entry.box ? 1 : 0) << ','
               << CsvEscape(entry.box ? entry.boxCondition : std::string()) << ','
               << (entry.manual ? 1 : 0) << ','
               << CsvEscape(entry.manual ? entry.manualCondition : std::string()) << ','
               << (entry.wanted ? 1 : 0) << ','
               << CsvEscape(entry.purchaseSource) << ','
               << CsvEscape(entry.purchaseDate) << ','
               << CsvEscape(entry.dateAdded) << ','
               << CsvEscape(entry.storageLocation) << ','
               << CsvEscape(entry.notes) << "\r\n";
    }
    output.close();

    SetStatus("Exported " + std::to_string(view_.size()) + " entries to " +
        std::filesystem::path(filename).filename().string() + ".");
}

// ---------------------------------------------------------------------------
// Status text and Library summary panel
// ---------------------------------------------------------------------------
std::string CollectionPage::StatusText(const GameLibrary& library) const
{
    std::string status = "  |  View: All  |  Filter: None  |  ";
    status += std::to_string(library.Count());
    status += " games in database  |  ";
    status += std::to_string(stats_.ownedCartridges);
    status += " owned";

    if (!search_.empty())
    {
        status += "  |  Search: ";
        status += search_;
        status += " (";
        status += std::to_string(view_.size());
        status += " shown)";
    }

    if (selected_ >= 0 && selected_ < static_cast<int>(view_.size()))
    {
        const CollectionEntry& entry = entries_[view_[selected_]];
        status += "  |  1 selected";
        if (!entry.catalogId.empty())
            status += " (No. " + entry.catalogId + ")";
        if (entry.wanted)
            status += "  |  Wanted";
    }

    if (!status_.empty())
    {
        status += "  |  ";
        status += status_;
    }
    return status;
}

void CollectionPage::DrawLibrarySummary(SDL_Renderer* renderer,
    const SDL_FRect& panel) const
{
    if (!renderer)
        return;

    // The Library dashboard draws the surrounding Win95 group frame and its
    // "My Collection" caption; this method renders only the panel contents.
    //
    // Column geometry is defined ONCE here and shared by the header and the
    // data rows so the status boxes line up exactly under Cart / Box / Manual / W.
    const float innerX = panel.x + 8.0f;
    const float right = panel.x + panel.w - 14.0f;   // clear of the scrollbar
    const float noW = 30.0f;
    const float checkColW = 48.0f;                    // equal, fixed status columns
    const float wantedCX = right - checkColW * 0.5f;
    const float manualCX = wantedCX - checkColW;
    const float boxCX = manualCX - checkColW;
    const float cartCX = boxCX - checkColW;
    const float titleX = innerX + noW;
    const float titleW = (std::max)(20.0f,
        (cartCX - checkColW * 0.5f - 10.0f) - titleX);

    const float headerScale = 0.80f;
    const float rowScale = 0.82f;
    float rowTextH = 0.0f;
    {
        float w = 0.0f, h = 0.0f;
        if (UiFont_MeasureText(16.5f * rowScale, "Ag", &w, &h))
            rowTextH = h;
    }
    if (rowTextH <= 0.0f)
        rowTextH = 13.0f;

    // Header: one shared baseline, one scale, status labels truly centred.
    const float separatorY = panel.y + 34.0f;
    const float headerY = panel.y + 15.0f;
    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    const auto drawHeaderLeft = [&](float x, const char* label)
    {
        DrawText(renderer, x, headerY, headerScale, label);
    };
    const auto drawHeaderCentered = [&](float cx, const char* label)
    {
        float w = 0.0f, h = 0.0f;
        if (!UiFont_MeasureText(16.5f * headerScale, label, &w, &h))
            w = 8.0f * headerScale *
                static_cast<float>(std::char_traits<char>::length(label));
        DrawText(renderer, cx - w * 0.5f, headerY, headerScale, label);
    };
    drawHeaderLeft(innerX, "#");
    drawHeaderLeft(titleX, "Title");
    drawHeaderCentered(cartCX, "Cart");
    drawHeaderCentered(boxCX, "Box");
    drawHeaderCentered(manualCX, "Manual");
    drawHeaderCentered(wantedCX, "W");
    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(renderer, innerX, separatorY, right, separatorY);

    const float listTop = separatorY + 3.0f;
    const float listBottom = panel.y + panel.h - 30.0f;
    const float rowH = 18.0f;
    const int rows = (std::max)(1, static_cast<int>((listBottom - listTop) / rowH));

    std::vector<int> order;
    order.reserve(entries_.size());
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b)
    {
        const CollectionEntry& ea = entries_[a];
        const CollectionEntry& eb = entries_[b];
        const CatalogKey ka = MakeCatalogKey(ea.catalogId.empty()
            ? ResolvedTitle(ea) : ea.catalogId);
        const CatalogKey kb = MakeCatalogKey(eb.catalogId.empty()
            ? ResolvedTitle(eb) : eb.catalogId);
        if (ka.group != kb.group) return ka.group < kb.group;
        if (ka.number != kb.number) return ka.number < kb.number;
        if (ka.variant != kb.variant) return ka.variant < kb.variant;
        if (ka.text != kb.text) return ka.text < kb.text;
        return ToLowerCopy(ResolvedTitle(ea)) < ToLowerCopy(ResolvedTitle(eb));
    });

    const int count = static_cast<int>(order.size());
    const int maxScroll = (std::max)(0, count - rows);
    summaryScroll_ = (std::clamp)(summaryScroll_, 0, maxScroll);

    // Alternating row backgrounds fill the whole list area so the panel reads
    // as a proper multi-row list rather than a half-empty rectangle.
    for (int row = 0; row < rows; ++row)
    {
        const float rowTop = listTop + static_cast<float>(row) * rowH;
        const SDL_FRect rowRect{innerX, rowTop, right - innerX, rowH};
        SDL_SetRenderDrawColor(renderer, row % 2 ? 245 : 255, row % 2 ? 245 : 255,
            row % 2 ? 245 : 255, 255);
        SDL_RenderFillRect(renderer, &rowRect);
    }

    if (count == 0)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        DrawText(renderer, innerX, listTop + (rowH - rowTextH) * 0.5f, rowScale,
            "No entries yet - open My Collection to add.");
    }
    else
    {
        for (int row = 0; row < rows; ++row)
        {
            const int index = summaryScroll_ + row;
            if (index >= count)
                break;
            const CollectionEntry& entry = entries_[order[index]];
            const float rowTop = listTop + static_cast<float>(row) * rowH;
            const float rowTextY = rowTop + (rowH - rowTextH) * 0.5f;
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            DrawText(renderer, innerX, rowTextY, rowScale, entry.catalogId);
            std::string title = ResolvedTitle(entry);
            const std::size_t maxChars = static_cast<std::size_t>(
                (std::max)(6.0f, titleW) / (8.0f * rowScale));
            title = Utf8Ellipsize(title, maxChars);
            DrawText(renderer, titleX, rowTextY, rowScale, title);
            const float checkY = rowTop + (rowH - 15.0f) * 0.5f;
            DrawSummaryCheck(renderer, {cartCX - 7.5f, checkY, 15.0f, 15.0f},
                entry.cartridge);
            DrawSummaryCheck(renderer, {boxCX - 7.5f, checkY, 15.0f, 15.0f},
                entry.box);
            DrawSummaryCheck(renderer, {manualCX - 7.5f, checkY, 15.0f, 15.0f},
                entry.manual);
            DrawSummaryCheck(renderer, {wantedCX - 7.5f, checkY, 15.0f, 15.0f},
                entry.wanted);
        }
    }

    // Win95 vertical scrollbar is always present; it fills the track when the
    // list is not scrollable, matching the main My Collection table.
    {
        const SDL_FRect track{panel.x + panel.w - 6.0f, listTop, 5.0f,
            listBottom - listTop};
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &track);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderRect(renderer, &track);
        float thumbH = track.h - 2.0f;
        if (count > rows && count > 0)
            thumbH = (std::max)(12.0f, (track.h - 2.0f) *
                static_cast<float>(rows) / static_cast<float>(count));
        float thumbY = track.y + 1.0f;
        if (maxScroll > 0)
            thumbY += (track.h - thumbH - 2.0f) *
                static_cast<float>(summaryScroll_) / static_cast<float>(maxScroll);
        const SDL_FRect thumb{track.x + 1.0f, thumbY, track.w - 2.0f, thumbH};
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderFillRect(renderer, &thumb);
    }

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    std::string summary = "Owned: " + std::to_string(stats_.ownedCartridges) +
        " | Boxed: " + std::to_string(stats_.boxedGames) +
        " | Manuals: " + std::to_string(stats_.manuals);
    DrawText(renderer, innerX, panel.y + panel.h - 22.0f, 0.84f, summary);
}

bool CollectionPage::HandleLibrarySummaryWheel(const SDL_FRect& panel,
    float x, float y, float deltaY)
{
    if (x < panel.x || x >= panel.x + panel.w ||
        y < panel.y || y >= panel.y + panel.h)
        return false;

    const float listTop = panel.y + 37.0f;
    const float listBottom = panel.y + panel.h - 30.0f;
    const int rows = (std::max)(1, static_cast<int>((listBottom - listTop) / 18.0f));
    const int maxScroll = (std::max)(0, static_cast<int>(entries_.size()) - rows);
    summaryScroll_ = (std::clamp)(summaryScroll_ + (deltaY > 0.0f ? -1 : 1),
        0, maxScroll);
    return true;
}
