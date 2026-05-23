#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// ============================================================
//  Renderer — thin wrapper around the Windows Console API.
//  All drawing is done by moving the cursor and writing chars;
//  there is no full-screen clear, so there is zero flicker.
// ============================================================
namespace Renderer
{
    // Console text-attribute colour codes
    enum class Color : WORD {
        Black       = 0,
        DarkBlue    = FOREGROUND_BLUE,
        DarkGreen   = FOREGROUND_GREEN,
        DarkCyan    = FOREGROUND_GREEN | FOREGROUND_BLUE,
        DarkRed     = FOREGROUND_RED,
        DarkMagenta = FOREGROUND_RED   | FOREGROUND_BLUE,
        DarkYellow  = FOREGROUND_RED   | FOREGROUND_GREEN,
        Gray        = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE,
        DarkGray    = FOREGROUND_INTENSITY,
        Blue        = FOREGROUND_BLUE  | FOREGROUND_INTENSITY,
        Green       = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        Cyan        = FOREGROUND_GREEN | FOREGROUND_BLUE  | FOREGROUND_INTENSITY,
        Red         = FOREGROUND_RED   | FOREGROUND_INTENSITY,
        Magenta     = FOREGROUND_RED   | FOREGROUND_BLUE  | FOREGROUND_INTENSITY,
        Yellow      = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        White       = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    };

    // Move the console cursor to (x, y) — (0,0) is top-left
    inline void GotoXY(int x, int y)
    {
        COORD coord{ static_cast<SHORT>(x), static_cast<SHORT>(y) };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
    }

    // Set foreground colour
    inline void SetColor(Color c)
    {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), static_cast<WORD>(c));
    }

    // Reset to default grey-on-black
    inline void ResetColor()
    {
        SetColor(Color::Gray);
    }

    // Draw a single character at (x, y)
    inline void DrawChar(int x, int y, char ch, Color c = Color::White)
    {
        GotoXY(x, y);
        SetColor(c);
        putchar(ch);
    }

    // Draw a string at (x, y)
    inline void DrawString(int x, int y, const std::string& s, Color c = Color::White)
    {
        GotoXY(x, y);
        SetColor(c);
        fputs(s.c_str(), stdout);
    }

    // Hide the blinking console cursor (looks cleaner during gameplay)
    inline void HideCursor()
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO ci{ 1, FALSE };
        SetConsoleCursorInfo(h, &ci);
    }

    // Restore the cursor for menus / game-over screen
    inline void ShowCursor()
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO ci{ 10, TRUE };
        SetConsoleCursorInfo(h, &ci);
    }

    // Resize the console window and its screen buffer
    inline void SetConsoleSize(int width, int height)
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

        // First shrink the window so the buffer can be resized safely
        SMALL_RECT tiny{ 0, 0, 1, 1 };
        SetConsoleWindowInfo(h, TRUE, &tiny);

        COORD bufSize{ static_cast<SHORT>(width), static_cast<SHORT>(height) };
        SetConsoleScreenBufferSize(h, bufSize);

        SMALL_RECT win{ 0, 0,
            static_cast<SHORT>(width  - 1),
            static_cast<SHORT>(height - 1) };
        SetConsoleWindowInfo(h, TRUE, &win);
    }
} // namespace Renderer
