---
name: project-state
description: Snake game rewritten to use Dear ImGui + DirectX 11 backend (no console API)
metadata:
  type: project
---

Snake game fully rewritten to Dear ImGui + DX11 on 2026-05-23.
Original console-based version replaced; Snake logic (Snake.h/Snake.cpp) kept intact.

**Why:** User requested ImGui/DX11 frontend to replace the Windows Console API rendering.

**How to apply:** All future rendering work uses ImGui DrawList API. No conio.h, no SetConsoleCursorPosition, no ANSI escape codes. Entry point is WinMain (Windows subsystem).

## File layout
- C:\testcode\game\snake\SnakeGame.sln
- C:\testcode\game\snake\SnakeGame\main.cpp         — WinMain, Win32 window, DX11 device, ImGui init + main loop
- C:\testcode\game\snake\SnakeGame\Game.h / Game.cpp — Game logic + ImGui DrawList rendering
- C:\testcode\game\snake\SnakeGame\Snake.h / Snake.cpp — Unchanged game logic (deque<Point>)
- C:\testcode\game\snake\SnakeGame\Renderer.h        — DrawList helper wrappers (DrawCellRect, DrawCellCircle, DrawGrid, etc.)
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj.filters
- C:\testcode\game\snake\SnakeGame\imgui\            — 14 Dear ImGui source files (v1.92.9 WIP)

## ImGui source files in imgui\
imgui.h, imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp,
imgui_internal.h, imconfig.h, imstb_rectpack.h, imstb_textedit.h, imstb_truetype.h,
imgui_impl_win32.h, imgui_impl_win32.cpp, imgui_impl_dx11.h, imgui_impl_dx11.cpp

## Key design decisions
- Entry point: WinMain (SubSystem = Windows, not Console)
- Linked libs: d3d11.lib; d3dcompiler.lib; dxgi.lib (dwmapi/gdi32 via #pragma comment in backend)
- AdditionalIncludeDirectories: $(ProjectDir); $(ProjectDir)imgui
- Game::Render() called once per frame; full redraw (no incremental tricks needed)
- Background drawn via ImGui::GetBackgroundDrawList(), board+snake via GetForegroundDrawList()
- HUD drawn as a no-titlebar, no-background ImGui window positioned above the board
- Title + game-over shown as styled ImGui windows (no popups, just regular Begin/End with fixed pos)
- Snake grow: m_pendingGrow flag; Snake::Move(true) called on next tick keeps the tail
- Tick timer: std::chrono::steady_clock, TICK_MS = 200ms (equalized H/V — no console width compensation needed)
- ImGui version: 1.92.9 WIP — AddRect signature is (p_min, p_max, col, rounding, thickness, flags)
  NOT the old (p_min, p_max, col, rounding, flags, thickness)

## Visual constants (Renderer.h)
- CELL_PX = 18.0f pixels per board cell
- GAP_PX  = 2.0f pixel gap between snake body segments
- Board: 40 x 20 cells = 720 x 360 px, centred in 900 x 700 window with 36px top HUD

## Features implemented
- WASD + arrow key input via ImGui::IsKeyDown / IsKeyPressed
- Wall + self collision detection (unchanged logic)
- Score and length HUD (yellow/cyan text, transparent overlay window)
- 5 food items with coloured circles + glow rings
- Snake head: lime green rounded rect (#82FF3C) + yellow border
- Snake body: gradient green (neck #3CD218 to tail #0A4010) with 2px gap
- Game over overlay (red-bordered dark window, R/Q options)
- Title screen (blue-bordered dark window, controls list, food dots preview)
- ESC quits from any phase

## Input mapping (ImGui keys)
- W / UpArrow    -> UP
- S / DownArrow  -> DOWN
- A / LeftArrow  -> LEFT
- D / RightArrow -> RIGHT
- ESC            -> Quit (any phase)
- Enter / Space  -> Start game (title screen)
- R              -> Restart (game over screen)
- Q              -> Quit (game over screen)
