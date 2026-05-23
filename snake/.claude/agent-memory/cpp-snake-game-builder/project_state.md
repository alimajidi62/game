---
name: project-state
description: Initial Snake game build — multi-file, VS2022, 0 warnings, exe produced
metadata:
  type: project
---

Initial Snake game delivered and compiled successfully on 2026-05-23.

**Why:** User requested a complete buildable Snake game for VS 2022, Windows console only.

**How to apply:** When making future changes, use the same file layout and build targets. Incremental renders (cursor repositioning, not cls) are the chosen anti-flicker strategy — preserve this.

## File layout
- C:\testcode\game\snake\SnakeGame.sln
- C:\testcode\game\snake\SnakeGame\main.cpp
- C:\testcode\game\snake\SnakeGame\Game.h / Game.cpp
- C:\testcode\game\snake\SnakeGame\Snake.h / Snake.cpp
- C:\testcode\game\snake\SnakeGame\Renderer.h
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj.filters

## Build output
- C:\testcode\game\snake\x64\Debug\SnakeGame.exe   (0 errors, 0 warnings)

## Key design decisions
- Snake body stored in std::deque<Point> — O(1) push_front / pop_back
- Incremental rendering: only head and erased tail are redrawn each tick (no full cls flicker)
- Fixed 130 ms tick via GetTickCount() delta + Sleep() remainder
- Arrow keys decoded via the 224+code two-byte sequence from _getch()
- m_lastTail member in Game carries the removed tail from Update() to Render()

## Features implemented
- WASD + arrow key input
- Wall + self collision detection
- Score and length HUD
- Food spawning (retry loop to avoid snake body)
- Game over overlay with R/Q prompt
- Title screen
- Coloured output via SetConsoleTextAttribute
