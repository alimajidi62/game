#include "Game.h"
#include "Renderer.h"

#include <conio.h>      // _kbhit(), _getch()
#include <windows.h>    // Sleep()
#include <algorithm>
#include <string>
#include <sstream>

// ============================================================
//  Constructor
// ============================================================
Game::Game()
    : m_snake(BOARD_W / 2, BOARD_H / 2)
    , m_food{ 0, 0 }
    , m_score(0)
    , m_running(false)
    , m_grew(false)
    , m_pendingDir(Direction::RIGHT)
    , m_rng(std::random_device{}())
    , m_distX(0, BOARD_W - 1)
    , m_distY(0, BOARD_H - 1)
{
}

// ============================================================
//  Public entry point
// ============================================================
void Game::Run()
{
    // Set up the console once
    Renderer::SetConsoleSize(BOARD_W + 4, BOARD_H + 6);
    SetConsoleTitleA("Snake - Classic Console Game");
    Renderer::HideCursor();

    bool keepPlaying = true;
    while (keepPlaying) {
        ShowTitleScreen();
        StartNewGame();
        RunGameLoop();
        ShowGameOver();

        // Ask play again?
        // (ShowGameOver parks at 'Press R to restart or Q to quit')
        bool answered = false;
        while (!answered) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 'r' || ch == 'R') { answered = true; }
                if (ch == 'q' || ch == 'Q') { answered = true; keepPlaying = false; }
            }
            Sleep(50);
        }
    }

    // Restore console on exit
    system("cls");
    Renderer::ResetColor();
    Renderer::ShowCursor();
}

// ============================================================
//  Title screen
// ============================================================
void Game::ShowTitleScreen()
{
    system("cls");

    int totalW  = BOARD_W + 4;
    int midRow  = (BOARD_H + 4) / 2;

    auto centre = [&](int row, const std::string& s, Renderer::Color c) {
        int col = (totalW - static_cast<int>(s.size())) / 2;
        if (col < 0) col = 0;
        Renderer::DrawString(col, row, s, c);
    };

    centre(midRow - 3, "##  SNAKE  ##",             Renderer::Color::Green);
    centre(midRow - 1, "W / Up Arrow   - Move Up",  Renderer::Color::Cyan);
    centre(midRow,     "S / Down Arrow - Move Down", Renderer::Color::Cyan);
    centre(midRow + 1, "A / Left Arrow - Move Left", Renderer::Color::Cyan);
    centre(midRow + 2, "D / Right Arrow- Move Right",Renderer::Color::Cyan);
    centre(midRow + 4, "Press any key to start...", Renderer::Color::Yellow);

    Renderer::ResetColor();
    while (!_kbhit()) Sleep(50);
    _getch(); // consume the keypress
}

// ============================================================
//  Reset everything and draw the initial frame
// ============================================================
void Game::StartNewGame()
{
    // Re-create the snake at the centre
    m_snake      = Snake(BOARD_W / 2, BOARD_H / 2);
    m_score      = 0;
    m_grew       = false;
    m_pendingDir = Direction::RIGHT;
    m_running    = true;

    SpawnAllFood();

    system("cls");
    DrawBorder();
    DrawHUD();
    DrawSnake();   // full snake redraw
    DrawFood();
}

// ============================================================
//  Main game loop — runs until m_running becomes false
// ============================================================
void Game::RunGameLoop()
{
    while (m_running) {
        DWORD start = GetTickCount();

        ProcessInput();
        Update();
        Render();

        // Use a longer delay for horizontal movement — console chars are ~2x taller than wide,
        // so without compensation the snake visually races sideways vs. up/down.
        Direction cur = m_snake.GetDirection();
        DWORD tickMs = (cur == Direction::LEFT || cur == Direction::RIGHT)
                       ? HORIZ_SPEED_MS : SPEED_MS;

        DWORD elapsed = GetTickCount() - start;
        if (elapsed < tickMs) Sleep(tickMs - elapsed);
    }
}

// ============================================================
//  Input — non-blocking; stores last direction for this tick
// ============================================================
void Game::ProcessInput()
{
    while (_kbhit()) {
        int ch = _getch();

        // Arrow keys come as two-byte sequences: 224 then the code
        if (ch == 224 || ch == 0) {
            ch = _getch();
            switch (ch) {
            case 72: m_pendingDir = Direction::UP;    break; // Up arrow
            case 80: m_pendingDir = Direction::DOWN;  break; // Down arrow
            case 75: m_pendingDir = Direction::LEFT;  break; // Left arrow
            case 77: m_pendingDir = Direction::RIGHT; break; // Right arrow
            }
        }
        else {
            switch (ch) {
            case 'w': case 'W': m_pendingDir = Direction::UP;    break;
            case 's': case 'S': m_pendingDir = Direction::DOWN;  break;
            case 'a': case 'A': m_pendingDir = Direction::LEFT;  break;
            case 'd': case 'D': m_pendingDir = Direction::RIGHT; break;
            case 27:            m_running = false; break; // ESC = quit
            }
        }
    }
}

// ============================================================
//  Update — move snake, check collisions
// ============================================================
void Game::Update()
{
    // Apply buffered direction
    m_snake.SetDirection(m_pendingDir);

    // Did the snake eat food on the PREVIOUS tick?  (m_grew carries over)
    Point removedTail = m_snake.Move(m_grew);
    m_grew = false;

    // Wall collision — head out of bounds?
    const Point& head = m_snake.GetHead();
    if (head.x < 0 || head.x >= BOARD_W ||
        head.y < 0 || head.y >= BOARD_H)
    {
        m_running = false;
        return;
    }

    // Self collision
    if (m_snake.SelfCollision()) {
        m_running = false;
        return;
    }

    // Did the snake eat any food this tick?
    for (Point& food : m_foods) {
        if (head == food) {
            m_grew  = true;
            m_score += 10;
            food = SpawnOneFood(); // replace just this apple
            break;
        }
    }

    // Store the removed tail so Render() can erase it
    // We smuggle it out via a member variable
    m_lastTail = removedTail;
}

// ============================================================
//  Render — incremental: only redraw what changed
// ============================================================
void Game::Render()
{
    // Erase old tail (if snake didn't grow)
    if (m_lastTail.x != -1) EraseTail(m_lastTail);

    // Draw new head
    DrawSnakeHead();

    // Redraw food (in case head was on food cell last frame)
    DrawFood();

    // HUD (score may have changed)
    DrawHUD();
}

// ============================================================
//  Drawing helpers
// ============================================================
void Game::DrawBorder() const
{
    // Top and bottom edges
    Renderer::DrawChar(BORDER_X, BORDER_Y, '+', Renderer::Color::DarkGreen);
    for (int x = 1; x <= BOARD_W; ++x)
        Renderer::DrawChar(BORDER_X + x, BORDER_Y, '-', Renderer::Color::DarkGreen);
    Renderer::DrawChar(BORDER_X + BOARD_W + 1, BORDER_Y, '+', Renderer::Color::DarkGreen);

    int bottomY = BORDER_Y + BOARD_H + 1;
    Renderer::DrawChar(BORDER_X, bottomY, '+', Renderer::Color::DarkGreen);
    for (int x = 1; x <= BOARD_W; ++x)
        Renderer::DrawChar(BORDER_X + x, bottomY, '-', Renderer::Color::DarkGreen);
    Renderer::DrawChar(BORDER_X + BOARD_W + 1, bottomY, '+', Renderer::Color::DarkGreen);

    // Left and right edges
    for (int y = 1; y <= BOARD_H; ++y) {
        Renderer::DrawChar(BORDER_X,              BORDER_Y + y, '|', Renderer::Color::DarkGreen);
        Renderer::DrawChar(BORDER_X + BOARD_W + 1, BORDER_Y + y, '|', Renderer::Color::DarkGreen);
    }
}

void Game::DrawHUD() const
{
    std::string scoreStr = "Score: " + std::to_string(m_score);
    std::string lengthStr = "Length: " + std::to_string(m_snake.GetBody().size());
    std::string helpStr  = "ESC: Quit";

    Renderer::DrawString(BORDER_X, 0, scoreStr,  Renderer::Color::Yellow);
    // Pad to overwrite stale digits
    Renderer::DrawString(BORDER_X + 16, 0, lengthStr, Renderer::Color::Cyan);
    Renderer::DrawString(BORDER_X + BOARD_W - 8, 0, helpStr, Renderer::Color::DarkGray);
}

void Game::DrawSnake() const
{
    const auto& body = m_snake.GetBody();
    for (size_t i = 0; i < body.size(); ++i) {
        int sx = CellToScreenX(body[i].x);
        int sy = CellToScreenY(body[i].y);
        char ch = (i == 0) ? '@' : 'o';
        Renderer::Color col = (i == 0) ? Renderer::Color::Green : Renderer::Color::DarkGreen;
        Renderer::DrawChar(sx, sy, ch, col);
    }
}

void Game::DrawSnakeHead() const
{
    const Point& head = m_snake.GetHead();
    Renderer::DrawChar(CellToScreenX(head.x), CellToScreenY(head.y), '@', Renderer::Color::Green);

    // Also redraw the segment behind the head as a body segment (it was '@' last frame)
    const auto& body = m_snake.GetBody();
    if (body.size() > 1) {
        const Point& neck = body[1];
        Renderer::DrawChar(CellToScreenX(neck.x), CellToScreenY(neck.y), 'o', Renderer::Color::DarkGreen);
    }
}

void Game::EraseTail(Point tail) const
{
    ClearCell(CellToScreenX(tail.x), CellToScreenY(tail.y));
}

void Game::DrawFood() const
{
    for (const Point& food : m_foods)
        Renderer::DrawChar(CellToScreenX(food.x), CellToScreenY(food.y), '*', Renderer::Color::Red);
}

void Game::ClearCell(int screenX, int screenY) const
{
    Renderer::DrawChar(screenX, screenY, ' ', Renderer::Color::Black);
}

void Game::DrawCenteredString(int row, const std::string& s) const
{
    int totalWidth = BOARD_W + 4;
    int col = (totalWidth - static_cast<int>(s.size())) / 2;
    if (col < 0) col = 0;
    Renderer::DrawString(col, row, s, Renderer::Color::White);
}

// ============================================================
//  Food spawning — keeps trying until not on snake or other food
// ============================================================
void Game::SpawnAllFood()
{
    m_foods.clear();
    m_foods.reserve(FOOD_COUNT);
    for (int i = 0; i < FOOD_COUNT; ++i)
        m_foods.push_back(SpawnOneFood());
}

Point Game::SpawnOneFood() const
{
    Point p;
    do {
        p.x = m_distX(m_rng);
        p.y = m_distY(m_rng);
    } while (PointOccupied(p));
    return p;
}

bool Game::PointOccupied(const Point& p) const
{
    for (const Point& b : m_snake.GetBody())
        if (b == p) return true;
    for (const Point& f : m_foods)
        if (f == p) return true;
    return false;
}

// ============================================================
//  Game Over screen
// ============================================================
void Game::ShowGameOver()
{
    int centreRow = BORDER_Y + BOARD_H / 2 - 1;
    int totalW    = BOARD_W + 4;

    auto centre = [&](int row, const std::string& s, Renderer::Color c) {
        int col = (totalW - static_cast<int>(s.size())) / 2;
        if (col < 0) col = 0;
        Renderer::DrawString(col, row, s, c);
    };

    // Dim the playfield overlay
    centre(centreRow,     "+-----------------------+", Renderer::Color::DarkGray);
    centre(centreRow + 1, "|       GAME  OVER      |", Renderer::Color::Red);
    centre(centreRow + 2, "|                       |", Renderer::Color::DarkGray);

    std::string scoreLine = "|  Final Score: " + std::to_string(m_score) + "        |";
    // Trim/pad to fixed width of 25 chars inside the box
    centre(centreRow + 3, scoreLine,                   Renderer::Color::Yellow);
    centre(centreRow + 4, "|                       |", Renderer::Color::DarkGray);
    centre(centreRow + 5, "|  R = Restart           |", Renderer::Color::Cyan);
    centre(centreRow + 6, "|  Q = Quit              |", Renderer::Color::Cyan);
    centre(centreRow + 7, "+-----------------------+", Renderer::Color::DarkGray);

    Renderer::ResetColor();
}
