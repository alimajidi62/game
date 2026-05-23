#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include <algorithm>

// ============================================================
//  Renderer — ANSI 24-bit colour + UTF-8 Unicode console drawing
// ============================================================
namespace Renderer
{
    struct RGB { int r, g, b; };

    // ---- Colour palette ----
    namespace Colors {
        constexpr RGB SnakeHead  = {130, 255,  60};
        constexpr RGB SnakeNeck  = { 60, 210,  40};
        constexpr RGB SnakeTail  = {  0,  65,  20};
        constexpr RGB Border     = { 45, 155, 255};
        constexpr RGB ScoreVal   = {255, 230,  40};
        constexpr RGB LengthCol  = { 80, 210, 255};
        constexpr RGB HelpCol    = { 75,  75,  75};
        constexpr RGB TitleMain  = { 80, 255,  80};
        constexpr RGB TitleSub   = {110, 170, 110};
        constexpr RGB HintCol    = {160, 160, 160};
        constexpr RGB KeyCol     = {255, 210,  50};
        constexpr RGB PressKey   = {255, 220,  80};
        constexpr RGB GameOverR  = {255,  50,  50};
        constexpr RGB BoxDim     = { 70,  70,  95};
        constexpr RGB White      = {235, 235, 235};
        constexpr RGB DimGray    = { 85,  85,  85};
        constexpr RGB Separator  = { 50, 100, 160};
    }

    // One colour per food slot (indices 0-4)
    constexpr RGB FOOD_COLORS[5] = {
        {255,  55,  55},   // red
        {255, 145,  25},   // orange
        {240, 225,  20},   // yellow
        { 35, 225, 225},   // cyan
        {255,  75, 210},   // pink
    };

    // ---- UTF-8 glyph constants ----
    namespace Chars {
        // Double-line box drawing
        static const char* TL      = "\xe2\x95\x94"; // ╔
        static const char* TR      = "\xe2\x95\x97"; // ╗
        static const char* BL      = "\xe2\x95\x9a"; // ╚
        static const char* BR      = "\xe2\x95\x9d"; // ╝
        static const char* HZ      = "\xe2\x95\x90"; // ═
        static const char* VT      = "\xe2\x95\x91"; // ║
        static const char* ML      = "\xe2\x95\xa0"; // ╠
        static const char* MR      = "\xe2\x95\xa3"; // ╣
        // Snake
        static const char* Body    = "\xe2\x96\xa0"; // ■
        static const char* Right   = "\xe2\x96\xb6"; // ▶
        static const char* Left    = "\xe2\x97\x80"; // ◀
        static const char* Up      = "\xe2\x96\xb2"; // ▲
        static const char* Down    = "\xe2\x96\xbc"; // ▼
        // Food / decorative
        static const char* Food    = "\xe2\x97\x8f"; // ●
        static const char* Diamond = "\xe2\x97\x86"; // ◆
        static const char* Star    = "\xe2\x98\x85"; // ★
        static const char* ThinHZ  = "\xe2\x94\x80"; // ─  (thin separator)
    }

    // ---- Internal helpers ----
    inline HANDLE Out() { return GetStdHandle(STD_OUTPUT_HANDLE); }

    inline void GotoXY(int x, int y)
    {
        COORD c{ static_cast<SHORT>(x), static_cast<SHORT>(y) };
        SetConsoleCursorPosition(Out(), c);
    }

    // ---- Initialise once at startup ----
    inline void Init()
    {
        HANDLE h = Out();
        // Enable ANSI virtual-terminal processing
        DWORD mode = 0;
        GetConsoleMode(h, &mode);
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // Set UTF-8 code page
        SetConsoleOutputCP(CP_UTF8);
        // Unbuffer stdout so ANSI codes flush immediately
        setvbuf(stdout, nullptr, _IONBF, 0);
        // Set a Unicode-capable font
        CONSOLE_FONT_INFOEX fi = { sizeof(CONSOLE_FONT_INFOEX) };
        GetCurrentConsoleFontEx(h, FALSE, &fi);
        fi.dwFontSize = { 0, 18 };
        wcscpy_s(fi.FaceName, L"Consolas");
        SetCurrentConsoleFontEx(h, FALSE, &fi);
    }

    inline void HideCursor()
    {
        CONSOLE_CURSOR_INFO ci{ 1, FALSE };
        SetConsoleCursorInfo(Out(), &ci);
    }

    inline void ShowCursor()
    {
        CONSOLE_CURSOR_INFO ci{ 10, TRUE };
        SetConsoleCursorInfo(Out(), &ci);
    }

    inline void ResetColor() { printf("\033[0m"); }

    inline void SetConsoleSize(int w, int h)
    {
        HANDLE hnd = Out();
        SMALL_RECT tiny{ 0, 0, 1, 1 };
        SetConsoleWindowInfo(hnd, TRUE, &tiny);
        COORD buf{ static_cast<SHORT>(w), static_cast<SHORT>(h) };
        SetConsoleScreenBufferSize(hnd, buf);
        SMALL_RECT win{ 0, 0, static_cast<SHORT>(w - 1), static_cast<SHORT>(h - 1) };
        SetConsoleWindowInfo(hnd, TRUE, &win);
    }

    // ---- Core drawing functions (RGB colour) ----
    inline void DrawString(int x, int y, const char* s, RGB fg)
    {
        GotoXY(x, y);
        printf("\033[38;2;%d;%d;%dm%s\033[0m", fg.r, fg.g, fg.b, s);
    }

    inline void DrawString(int x, int y, const std::string& s, RGB fg)
    {
        DrawString(x, y, s.c_str(), fg);
    }

    // Erase a single character cell
    inline void ClearAt(int x, int y)
    {
        GotoXY(x, y);
        printf("\033[0m ");
    }

    // ---- Colour math ----
    inline RGB LerpColor(RGB a, RGB b, float t)
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return {
            a.r + (int)((b.r - a.r) * t),
            a.g + (int)((b.g - a.g) * t),
            a.b + (int)((b.b - a.b) * t)
        };
    }

    // Body gradient: index 0 = neck (bright), 10+ = tail (dark)
    inline RGB BodyColor(int segIdx)
    {
        float t = (float)segIdx / 10.0f;
        return LerpColor(Colors::SnakeNeck, Colors::SnakeTail, t);
    }

} // namespace Renderer
