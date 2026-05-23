---
name: project-state
description: Snake game rewritten to Dear ImGui + DirectX 11 backend; sprites integrated from OpenGameArt CC0 pack
metadata:
  type: project
---

Snake game fully rewritten to Dear ImGui + DX11 on 2026-05-23.
Sprite support added 2026-05-23: PNG sprites loaded via stb_image → D3D11 SRVs, rendered via ImDrawList::AddImage.

**Why:** User requested ImGui/DX11 frontend (replacing Console API), then real sprite images for the snake.

**How to apply:** All future rendering work uses ImGui DrawList API. No conio.h, no SetConsoleCursorPosition. Entry point is WinMain (Windows subsystem). Sprites loaded at startup; primitive DrawList fallback used if any texture fails.

## File layout
- C:\testcode\game\snake\SnakeGame.sln
- C:\testcode\game\snake\SnakeGame\main.cpp         — WinMain, Win32 window, DX11 device, ImGui init + main loop; calls game.LoadSprites(g_pd3dDevice) and ReleaseTextures() at shutdown
- C:\testcode\game\snake\SnakeGame\Game.h / Game.cpp — Game logic + ImGui DrawList rendering + sprite path
- C:\testcode\game\snake\SnakeGame\Snake.h / Snake.cpp — Unchanged game logic (deque<Point>)
- C:\testcode\game\snake\SnakeGame\Renderer.h        — DrawList helper wrappers (DrawCellRect, DrawCellCircle, DrawGrid, etc.)
- C:\testcode\game\snake\SnakeGame\TextureLoader.h   — Header-only PNG loader (stb_image → D3D11 SRV); define TEXTURE_LOADER_IMPLEMENTATION in Game.cpp
- C:\testcode\game\snake\SnakeGame\SpriteRenderer.h  — SpriteSet struct + DrawSprite() inline helper (AddImage wrapper)
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj.filters
- C:\testcode\game\snake\SnakeGame\imgui\            — 14 Dear ImGui source files (v1.92.9 WIP) + stb_image.h v2.30
- C:\testcode\game\snake\SnakeGame\assets\           — 16 CC0 PNG sprites (see below)

## Sprite assets (CC0, from OpenGameArt "Snake game assets" by Clear_code)
- head_up/down/left/right.png      — snake head (4 directions)
- body_horizontal.png              — straight body segment (H)
- body_vertical.png                — straight body segment (V)
- body_topright.png                — corner, curve in top-right quadrant
- body_topleft.png                 — corner, curve in top-left quadrant
- body_bottomright.png             — corner, curve in bottom-right quadrant
- body_bottomleft.png              — corner, curve in bottom-left quadrant
- tail_up/down/left/right.png      — tail tip (4 directions)
- apple.png                        — food sprite (tinted per-item at draw time)

## Corner sprite selection logic (PickCornerSprite)
- in/out = direction FROM previous segment (head side) INTO this / FROM this INTO next (tail side)
- body_topright   opens L & D: (in=L, out=D) or (in=D, out=L) → cornerTR
- body_topleft    opens R & D: (in=R, out=D) or (in=D, out=R) → cornerTL
- body_bottomright opens L & U: (in=L, out=U) or (in=U, out=L) → cornerBR
- body_bottomleft  opens R & U: (in=R, out=U) or (in=U, out=R) → cornerBL

## ImGui source files in imgui\
imgui.h, imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp,
imgui_internal.h, imconfig.h, imstb_rectpack.h, imstb_textedit.h, imstb_truetype.h,
imgui_impl_win32.h, imgui_impl_win32.cpp, imgui_impl_dx11.h, imgui_impl_dx11.cpp,
stb_image.h (v2.30)

## Key design decisions
- Entry point: WinMain (SubSystem = Windows, not Console)
- Linked libs: d3d11.lib; d3dcompiler.lib; dxgi.lib
- AdditionalIncludeDirectories: $(ProjectDir); $(ProjectDir)imgui
- Game::Render() called once per frame; full redraw (no incremental tricks needed)
- Background drawn via ImGui::GetBackgroundDrawList(), board+snake via GetForegroundDrawList()
- HUD drawn as a no-titlebar, no-background ImGui window positioned above the board
- Title + game-over shown as styled ImGui windows with fixed pos
- Snake grow: m_pendingGrow flag; Snake::Move(true) called on next tick keeps the tail
- Tick timer: std::chrono::steady_clock, TICK_MS = 200ms
- ImGui version: 1.92.9 WIP — AddRect signature is (p_min, p_max, col, rounding, thickness, flags)
- stb_image: TEXTURE_LOADER_IMPLEMENTATION defined in Game.cpp (only); header-only everywhere else
- Textures cast: (ImTextureID)(ID3D11ShaderResourceView*) — DX11 ImGui backend convention
- Asset path resolution: tries 5 prefixes (assets\, ..\assets\, ..\..\assets\, etc.) so EXE works from VS (CWD=project dir) and when double-clicked from x64\Debug\

## Visual constants (Renderer.h)
- CELL_PX = 18.0f pixels per board cell
- GAP_PX  = 2.0f pixel gap between snake body segments (fallback only)
- Board: 40 x 20 cells = 720 x 360 px, centred in 900 x 700 window with 36px top HUD

## Features implemented
- WASD + arrow key input via ImGui::IsKeyDown / IsKeyPressed
- Wall + self collision detection
- Score and length HUD (yellow/cyan text, transparent overlay window)
- 5 food items with apple sprites (tinted 5 different colours) or coloured circles fallback
- Snake head: sprite (4-directional) or lime-green rounded rect with eyes+mouth fallback
- Snake body: correct sprite per segment (straight H/V, 4 corners, tail) or gradient fallback
- Game over overlay (red-bordered dark window, R/Q options)
- Title screen (blue-bordered dark window, controls list)
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
