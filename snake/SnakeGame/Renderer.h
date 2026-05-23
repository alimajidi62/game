#pragma once
//  Renderer.h — ImGui DrawList rendering utilities for the Snake game
//  All drawing goes through an ImDrawList* passed in from Game::Render().
//  No global state; no console API.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui/imgui.h"
#include <string>

namespace Renderer
{
    // ---- Colour helpers -------------------------------------------------------

    struct RGB { unsigned char r, g, b; };

    // Pack an RGB + alpha into ImU32 (ABGR byte order that ImGui uses).
    inline ImU32 ToImU32(RGB c, unsigned char a = 255)
    {
        return IM_COL32(c.r, c.g, c.b, a);
    }

    inline ImU32 ToImU32(int r, int g, int b, int a = 255)
    {
        return IM_COL32(r, g, b, a);
    }

    // Linear interpolation between two RGB values (t in [0,1]).
    inline RGB LerpColor(RGB a, RGB b, float t)
    {
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        return {
            (unsigned char)(a.r + (int)((b.r - a.r) * t)),
            (unsigned char)(a.g + (int)((b.g - a.g) * t)),
            (unsigned char)(a.b + (int)((b.b - a.b) * t))
        };
    }

    // ---- Colour palette -------------------------------------------------------

    namespace Colors
    {
        // Snake
        constexpr RGB SnakeHead  = {130, 255,  60};   // #82FF3C
        constexpr RGB SnakeNeck  = { 60, 210,  24};   // #3CD218
        constexpr RGB SnakeTail  = { 10,  64,  16};   // #0A4010
        // UI chrome
        constexpr RGB BoardBg    = { 12,  12,  22};   // very dark navy
        constexpr RGB GridLine   = { 25,  25,  40};   // barely-visible grid
        constexpr RGB Border     = { 45, 155, 255};   // blue border
        // HUD text
        constexpr RGB ScoreVal   = {255, 230,  40};
        constexpr RGB LengthCol  = { 80, 210, 255};
        constexpr RGB HelpCol    = { 75,  75,  75};
        // Title / game over
        constexpr RGB TitleMain  = { 80, 255,  80};
        constexpr RGB TitleSub   = {110, 170, 110};
        constexpr RGB HintCol    = {160, 160, 160};
        constexpr RGB PressKey   = {255, 220,  80};
        constexpr RGB GameOverR  = {255,  50,  50};
        constexpr RGB White      = {235, 235, 235};
        constexpr RGB DimGray    = { 85,  85,  85};
    }

    // Food colours indexed 0-4 (red, orange, yellow, cyan, pink)
    constexpr RGB FOOD_COLORS[5] = {
        {255,  55,  55},
        {255, 145,  25},
        {240, 225,  20},
        { 35, 225, 225},
        {255,  75, 210},
    };

    // Body gradient: segment 0 = neck (bright), 10+ = tail (dark)
    inline RGB BodyColor(int segIdx)
    {
        float t = (float)segIdx / 10.0f;
        return LerpColor(Colors::SnakeNeck, Colors::SnakeTail, t);
    }

    // ---- Cell size constant (pixels per board cell) ---------------------------
    //  Board is BOARD_W x BOARD_H = 40 x 20 cells.
    //  At CELL_PX = 18 that is 720 x 360 px for the play field.
    static constexpr float CELL_PX = 18.0f;
    static constexpr float GAP_PX  =  2.0f;  // gap between adjacent body rects

    // ---- DrawList wrappers ----------------------------------------------------

    // Filled rectangle covering one board cell (with optional inner gap for body)
    inline void DrawCellRect(ImDrawList* dl, float originX, float originY,
                              int cellX, int cellY,
                              ImU32 col,
                              float rounding = 2.0f,
                              float gap = 0.0f)
    {
        float x0 = originX + cellX * CELL_PX + gap;
        float y0 = originY + cellY * CELL_PX + gap;
        float x1 = x0 + CELL_PX - gap * 2.0f;
        float y1 = y0 + CELL_PX - gap * 2.0f;
        dl->AddRectFilled({x0, y0}, {x1, y1}, col, rounding);
    }

    // Outlined rectangle around one board cell
    inline void DrawCellBorder(ImDrawList* dl, float originX, float originY,
                                int cellX, int cellY,
                                ImU32 col, float thickness = 1.5f,
                                float rounding = 2.0f)
    {
        float x0 = originX + cellX * CELL_PX + 1.0f;
        float y0 = originY + cellY * CELL_PX + 1.0f;
        float x1 = x0 + CELL_PX - 2.0f;
        float y1 = y0 + CELL_PX - 2.0f;
        dl->AddRect({x0, y0}, {x1, y1}, col, rounding, thickness);
    }

    // Circle centred on a board cell (for food)
    inline void DrawCellCircle(ImDrawList* dl, float originX, float originY,
                                int cellX, int cellY,
                                ImU32 col, float radiusFrac = 0.35f)
    {
        float cx = originX + (cellX + 0.5f) * CELL_PX;
        float cy = originY + (cellY + 0.5f) * CELL_PX;
        float r  = CELL_PX * radiusFrac;
        dl->AddCircleFilled({cx, cy}, r, col);
    }

    // Glow ring (larger semi-transparent circle)
    inline void DrawCellGlow(ImDrawList* dl, float originX, float originY,
                              int cellX, int cellY,
                              ImU32 col)
    {
        float cx = originX + (cellX + 0.5f) * CELL_PX;
        float cy = originY + (cellY + 0.5f) * CELL_PX;
        float r  = CELL_PX * 0.48f;
        // col already has alpha baked in by caller
        dl->AddCircleFilled({cx, cy}, r, col);
    }

    // ---- Grid drawing ---------------------------------------------------------
    inline void DrawGrid(ImDrawList* dl,
                         float originX, float originY,
                         int boardW, int boardH)
    {
        ImU32 gc = ToImU32(Colors::GridLine, 180);
        // Vertical lines
        for (int x = 0; x <= boardW; ++x)
        {
            float px = originX + x * CELL_PX;
            dl->AddLine({px, originY},
                        {px, originY + boardH * CELL_PX},
                        gc, 0.5f);
        }
        // Horizontal lines
        for (int y = 0; y <= boardH; ++y)
        {
            float py = originY + y * CELL_PX;
            dl->AddLine({originX, py},
                        {originX + boardW * CELL_PX, py},
                        gc, 0.5f);
        }
    }

} // namespace Renderer
