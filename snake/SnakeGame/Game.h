#pragma once
//  Game.h — Snake game state and logic (ImGui rendering, DX11 backend)
//  Rendering is done entirely via ImGui DrawList; no console API.

#include "Snake.h"
#include "Renderer.h"

#include <random>
#include <vector>
#include <chrono>

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

    // Called every frame from the Win32/DX11 main loop.
    // Reads ImGui keyboard state, ticks logic at TICK_MS intervals,
    // and issues all draw calls to ImGui DrawList.
    void ProcessInput();              // read ImGui keys -> update direction / phase
    void Update();                    // advance game state if tick has elapsed
    void Render(float winW, float winH); // draw everything via ImGui

    bool ShouldQuit() const { return m_quit; }

private:
    // ---- phase renderers ----------------------------------------------
    void RenderTitle   (ImDrawList* dl, float winW, float winH);
    void RenderPlaying (ImDrawList* dl, float winW, float winH);
    void RenderGameOver(ImDrawList* dl, float winW, float winH);

    // ---- drawing helpers ----------------------------------------------
    void DrawBoard    (ImDrawList* dl, float ox, float oy) const;
    void DrawSnake    (ImDrawList* dl, float ox, float oy) const;
    void DrawSnakeHead(ImDrawList* dl, float ox, float oy) const; // face with eyes + mouth
    void DrawFood     (ImDrawList* dl, float ox, float oy) const;
    void DrawHUD      (float ox, float oy, float boardPxW, float boardPxH) const;

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
};
