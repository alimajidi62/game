#pragma once
//  Game.h — Snake game state and logic (ImGui rendering, DX11 backend)
//  Rendering: 3-D pass (Renderer3D) runs first each frame, then ImGui HUD overlays.

#include "Snake.h"
#include "Renderer.h"
#include "TextureLoader.h"
#include "SpriteRenderer.h"
#include "Renderer3D.h"

#include <random>
#include <vector>
#include <chrono>
#include <deque>

// Forward-declare D3D11 device so Game.h does not need to pull in d3d11.h
// (main.cpp and Game.cpp both include d3d11.h before including Game.h via TextureLoader.h).

// ============================================================
//  Board constants
// ============================================================
constexpr int   BOARD_W    = 40;    // columns (cells)
constexpr int   BOARD_H    = 20;    // rows    (cells)
constexpr int   FOOD_COUNT =  5;    // live food items at once
constexpr int   TICK_MS    = 200;   // ms per game step (equalized H/V)

// ============================================================
//  Game phases
// ============================================================
enum class GamePhase { Title, Playing, GameOver };

// ============================================================
//  Game class
// ============================================================
class Game
{
public:
    Game();

    // Called once after the DX11 device is created.
    // Initialises the 3-D renderer and loads sprite textures.
    // bbWidth/bbHeight = initial back-buffer pixel dimensions.
    void LoadSprites(ID3D11Device* device,
                     ID3D11DeviceContext* ctx = nullptr,
                     UINT bbWidth = 900, UINT bbHeight = 700);

    // Notify the 3-D renderer that the swap-chain was resized.
    void On3DResize(UINT newW, UINT newH);

    // Expose the Renderer3D so main.cpp can call On3DResize on WM_SIZE.
    Renderer3D& GetRenderer3D() { return m_renderer3D; }

    // Called every frame from the Win32/DX11 main loop.
    void ProcessInput();                          // read ImGui keys -> update direction / phase
    void Update();                                // advance game state if tick has elapsed
    void Render(float winW, float winH,           // draw 3-D scene then ImGui overlays
                ID3D11RenderTargetView* rtv = nullptr);

    bool ShouldQuit() const { return m_quit; }

private:
    // ---- phase renderers ----------------------------------------------
    void RenderTitle        (ImDrawList* dl, float winW, float winH);
    void RenderPlaying      (ImDrawList* dl, float winW, float winH); // legacy 2-D (unused in 3-D mode)
    void RenderGameOver     (ImDrawList* dl, float winW, float winH);
    void Render3DOverlayHUD (float winW, float winH); // HUD overlay for 3-D mode

    // ---- drawing helpers ----------------------------------------------
    void DrawBoard    (ImDrawList* dl, float ox, float oy) const;
    void DrawSnake    (ImDrawList* dl, float ox, float oy) const;
    void DrawSnakeHead(ImDrawList* dl, float ox, float oy) const;
    void DrawFood     (ImDrawList* dl, float ox, float oy) const;
    void DrawHUD      (float ox, float oy, float boardPxW, float boardPxH) const;

    // ---- sprite helpers -----------------------------------------------
    // Pick the correct corner sprite given the direction FROM the previous
    // segment (in) and the direction TO the next segment (out).
    ID3D11ShaderResourceView* PickCornerSprite(Direction in, Direction out) const;

    // ---- game-state helpers -------------------------------------------
    void  ResetGame();
    void  SpawnAllFood();
    Point SpawnOneFood() const;
    bool  PointOccupied(const Point& p) const;

    // ---- state --------------------------------------------------------
    GamePhase          m_phase;
    Snake              m_snake;
    std::vector<Point> m_foods;
    int                m_score;
    Direction          m_pendingDir;
    bool               m_pendingGrow;   // true when snake should grow next tick
    bool               m_quit;

    // Board pixel origin (top-left of cell (0,0)), set each Render()
    float              m_boardOriginX;
    float              m_boardOriginY;

    // Timing
    using Clock = std::chrono::steady_clock;
    Clock::time_point  m_lastTick;

    // RNG
    mutable std::mt19937                       m_rng;
    mutable std::uniform_int_distribution<int> m_distX;
    mutable std::uniform_int_distribution<int> m_distY;

    // Sprites (kept for compilation; DrawSprite calls are skipped in 3-D mode)
    SpriteSet          m_sprites;
    bool               m_spritesLoaded = false;

    // 3-D renderer
    Renderer3D         m_renderer3D;
    ID3D11RenderTargetView* m_rtv = nullptr; // borrowed from main.cpp, not owned
};
