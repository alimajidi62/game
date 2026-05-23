#pragma once
#include "Snake.h"
#include <random>
#include <vector>

// ============================================================
//  Game constants — adjust these to taste
// ============================================================
constexpr int BOARD_W     = 40;   // playfield columns (inner, excluding border)
constexpr int BOARD_H     = 20;   // playfield rows    (inner, excluding border)
constexpr int BORDER_X    = 1;    // left edge of the border on screen
constexpr int BORDER_Y    = 2;    // top  edge of the border on screen (row 0-1 = HUD)
constexpr int SPEED_MS       = 200;  // milliseconds per vertical tick
constexpr int HORIZ_SPEED_MS = 200;  // longer delay for horizontal — console chars are ~2x taller than wide
constexpr int FOOD_COUNT     = 5;    // how many apples are on the board at once

// Screen-space coords for the play area cells (0,0) -> (BOARD_W-1, BOARD_H-1)
inline int CellToScreenX(int cellX) { return BORDER_X + 1 + cellX; }
inline int CellToScreenY(int cellY) { return BORDER_Y + 1 + cellY; }

// ============================================================
//  Game — owns the game loop, state, and all subsystems
// ============================================================
class Game {
public:
    Game();

    // Entry point — returns when the player quits
    void Run();

private:
    // --- phases ---
    void ShowTitleScreen();
    void StartNewGame();
    void RunGameLoop();
    void ShowGameOver();

    // --- per-tick helpers ---
    void ProcessInput();
    void Update();
    void Render();

    // --- drawing helpers ---
    void DrawBorder() const;
    void DrawHUD()    const;
    void DrawSnake()  const;      // full redraw (used on game start)
    void DrawSnakeHead() const;   // just the new head
    void EraseTail(Point tail) const;
    void DrawFood()  const;
    void ClearCell(int screenX, int screenY) const;
    void DrawCenteredString(int row, const std::string& s) const;

    // --- food ---
    void SpawnAllFood();
    Point SpawnOneFood() const;
    bool  PointOccupied(const Point& p) const;

    // --- state ---
    Snake              m_snake;
    std::vector<Point> m_foods;   // all active apples (always FOOD_COUNT of them)
    Point              m_lastTail; // tail cell removed last tick (or {-1,-1} if grew)
    int            m_score;
    bool           m_running;     // false  => exit game loop
    bool           m_grew;        // did the snake eat food this tick?
    Direction      m_pendingDir;  // buffered input direction

    // --- RNG ---
    mutable std::mt19937                        m_rng;
    mutable std::uniform_int_distribution<int>  m_distX;
    mutable std::uniform_int_distribution<int>  m_distY;
};
