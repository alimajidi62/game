---
name: project-state
description: Snake game with full 3D DirectX 11 render pass (Renderer3D) + ImGui HUD overlays
metadata:
  type: project
---

Snake game fully rewritten to Dear ImGui + DX11 on 2026-05-23.
Full 3D render pass added 2026-05-23: board floor, snake segments (boxes), food (spheres) all drawn in 3D using D3D11 + DirectXMath.

**Why:** User requested ImGui/DX11 frontend (replacing Console API), then real sprite images for the snake, then full 3D rendering.

**How to apply:** All new rendering work goes in Renderer3D. ImGui overlays (HUD, title, game-over) remain on top of the 3D scene. Game logic (Snake.cpp) is never touched.

## File layout
- C:\testcode\game\snake\SnakeGame.sln
- C:\testcode\game\snake\SnakeGame\main.cpp         — WinMain, Win32 window, DX11 device, ImGui init + main loop
- C:\testcode\game\snake\SnakeGame\Game.h / Game.cpp — Game logic + phase dispatch; 3D pass runs before ImGui HUD
- C:\testcode\game\snake\SnakeGame\Snake.h / Snake.cpp — Unchanged game logic (deque<Point>)
- C:\testcode\game\snake\SnakeGame\Renderer.h        — DrawList helper wrappers (legacy 2D, kept for fallback)
- C:\testcode\game\snake\SnakeGame\Renderer3D.h      — Renderer3D class declaration; Vertex3D, CBPerDraw structs
- C:\testcode\game\snake\SnakeGame\Renderer3D.cpp    — Full 3D renderer: shaders, geometry, draw calls
- C:\testcode\game\snake\SnakeGame\Shader3D.h        — Embedded HLSL strings (kVS3D_HLSL, kPS3D_HLSL)
- C:\testcode\game\snake\SnakeGame\TextureLoader.h   — Header-only PNG loader (stb_image → D3D11 SRV)
- C:\testcode\game\snake\SnakeGame\SpriteRenderer.h  — SpriteSet + DrawSprite() helper (dormant in 3D mode)
- C:\testcode\game\snake\SnakeGame\SnakeGame.vcxproj
- C:\testcode\game\snake\SnakeGame\imgui\            — Dear ImGui v1.92.9 WIP + stb_image.h v2.30
- C:\testcode\game\snake\SnakeGame\assets\           — 16 CC0 PNG sprites (loaded but not drawn in 3D mode)
- C:\testcode\game\snake\x64\Debug\SnakeGame.exe     — Build output (~2.1 MB)

## 3D Render Architecture

### Coordinate system
- +X = board column (left→right), +Y = up, +Z = board row (top→bottom)
- Board footprint: X in [0,BOARD_W], Z in [0,BOARD_H], floor at Y=0

### Camera
- Eye: (boardW*0.5, boardH*1.3, -boardH*0.85)
- Target: (boardW*0.5, 0, boardH*0.5) — board centre
- FOV: 50°, near=0.1, far=500, aspect from window size

### Geometry (procedural, no .obj files)
- Floor: flat quad at Y=0 covering full board; dark greenish-grey
- Grid: thin flat boxes (scaleY=0.005) at each integer cell boundary
- Snake segments: box scaled to (0.88, 0.60, 0.88), sitting on floor (Y=segH/2)
  - Head: bright green (0.51, 1.0, 0.24)
  - Body: gradient from neck green to tail dark-green
  - Tail: drawn in same pass, darkest at back of body
- Food: UV sphere (radius=0.35, stacks=10, slices=14) at Y=0.5, 5 colours matching 2D palette

### Shaders (embedded in Shader3D.h, compiled at runtime with D3DCompile)
- VS: transforms pos by WorldViewProj; passes world-space normal to PS
- PS: Lambertian diffuse (one directional light from above-side) + ambient
- Light direction (world): (0.4, 0.9, -0.3) — from upper-right
- Ambient: (0.18, 0.18, 0.22)
- Constant buffer (b0): WorldViewProj, World, LightDir, BaseColor, AmbientColor

### Depth buffer
- Format: DXGI_FORMAT_D24_UNORM_S8_UINT
- Created in Renderer3D::Init(), recreated on OnResize()
- Cleared each frame before 3D draws; unbound before ImGui renders

### Frame order each frame
1. main.cpp: ClearRenderTargetView (dark navy)
2. main.cpp: ImGui::NewFrame()
3. game.Render() → Renderer3D::DrawScene() [real D3D11 draws to RTV+DSV]
4. DrawScene unbinds DSV, leaves RTV only
5. game.Render() → ImGui draw commands (HUD, overlays)
6. main.cpp: ImGui::Render() + ImGui_ImplDX11_RenderDrawData()
7. main.cpp: OMSetRenderTargets(RTV, nullptr) before ImGui flush (ensures no DSV)

### Key implementation notes
- Renderer3D.h includes Snake.h directly (needs Point for std::deque<Point> in DrawScene signature)
- Renderer3D.cpp does NOT include Game.h (avoids circular include); uses local kBoardW/kBoardH constants
- Sprite code (SpriteRenderer, TextureLoader, m_spritesLoaded) is preserved but DrawSprite is never called in Playing/GameOver phases
- RenderPlaying (2D) still compiles; it's never called in 3D mode
- Render3DOverlayHUD() draws the score/length bar + dark top strip over the 3D scene
- Title screen still uses 2D ImGui overlay (no 3D in Title phase)

## Linked libs (already in .vcxproj for all configs)
d3d11.lib; d3dcompiler.lib; dxgi.lib

## Board / window constants
- BOARD_W = 40, BOARD_H = 20, FOOD_COUNT = 5, TICK_MS = 200ms
- Window: 900 x 700, SubSystem = Windows (WinMain)
- CELL_PX = 18.0f (kept in Renderer.h; repurposed as HUD layout unit)

## ImGui version: 1.92.9 WIP (in imgui\)
## Build: VS2022, C++17, x64 Debug/Release — clean rebuild confirmed 2026-05-23
