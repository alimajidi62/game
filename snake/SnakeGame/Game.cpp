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
    Renderer::Init();
    SetConsoleTitleA("Snake");
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
    using namespace Renderer;
    system("cls");

    // Outer box  (same footprint as the game border)
    const int L  = BORDER_X;
    const int R  = BORDER_X + BOARD_W + 1;
    const int T  = 0;
    const int B  = BORDER_Y + BOARD_H + 1;

    // Top / bottom
    DrawString(L, T, Chars::TL, Colors::Border);
    for (int x = L + 1; x < R; ++x) DrawString(x, T, Chars::HZ, Colors::Border);
    DrawString(R, T, Chars::TR, Colors::Border);
    DrawString(L, B, Chars::BL, Colors::Border);
    for (int x = L + 1; x < R; ++x) DrawString(x, B, Chars::HZ, Colors::Border);
    DrawString(R, B, Chars::BR, Colors::Border);
    // Side walls
    for (int y = T + 1; y < B; ++y) {
        DrawString(L, y, Chars::VT, Colors::Border);
        DrawString(R, y, Chars::VT, Colors::Border);
    }

    // ---- Content  (inner: cols 2..41, rows 1..B-1) ----

    // Big title
    DrawString(13, 3,  "\xe2\x97\x86\xe2\x97\x86  S N A K E  \xe2\x97\x86\xe2\x97\x86", Colors::TitleMain);
    DrawString(12, 4,  "~ ~ Classic Console Game ~ ~",   Colors::TitleSub);

    // Thin separator
    for (int x = L + 1; x < R; ++x) DrawString(x, 6, Chars::ThinHZ, Colors::Separator);

    // Controls
    DrawString(5,  8,  "\xe2\x96\xb2  W / Up Arrow        Move Up",    Colors::HintCol);
    DrawString(5,  9,  "\xe2\x96\xbc  S / Down Arrow      Move Down",   Colors::HintCol);
    DrawString(5, 10,  "\xe2\x97\x80  A / Left Arrow      Move Left",   Colors::HintCol);
    DrawString(5, 11,  "\xe2\x96\xb6  D / Right Arrow     Move Right",  Colors::HintCol);
    DrawString(5, 12,  "   ESC                   Quit",                  Colors::DimGray);

    // Thin separator
    for (int x = L + 1; x < R; ++x) DrawString(x, 14, Chars::ThinHZ, Colors::Separator);

    // Food info — draw five coloured dots
    DrawString(7, 16, "Collect:", Colors::HintCol);
    for (int i = 0; i < 5; ++i)
        DrawString(16 + i * 2, 16, Chars::Food, FOOD_COLORS[i]);
    DrawString(27, 16, "5 apples always on board", Colors::HintCol);

    // Press-any-key prompt
    DrawString(9, 19, "\xe2\x98\x85  Press any key to start  \xe2\x98\x85", Colors::PressKey);

    ResetColor();
    while (!_kbhit()) Sleep(50);
    _getch();
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
    using namespace Renderer;
    const int L  = BORDER_X;
    const int R  = BORDER_X + BOARD_W + 1;
    const int bot = BORDER_Y + BOARD_H + 1;

    // Row 0: top of the HUD box
    DrawString(L, 0, Chars::TL, Colors::Border);
    for (int x = L + 1; x < R; ++x) DrawString(x, 0, Chars::HZ, Colors::Border);
    DrawString(R, 0, Chars::TR, Colors::Border);

    // Row 1: HUD side walls (content filled by DrawHUD)
    DrawString(L, 1, Chars::VT, Colors::Border);
    DrawString(R, 1, Chars::VT, Colors::Border);

    // Row BORDER_Y (=2): separator between HUD and game area
    DrawString(L, BORDER_Y, Chars::ML, Colors::Border);
    for (int x = L + 1; x < R; ++x) DrawString(x, BORDER_Y, Chars::HZ, Colors::Border);
    DrawString(R, BORDER_Y, Chars::MR, Colors::Border);

    // Game-area side walls
    for (int y = 1; y <= BOARD_H; ++y) {
        DrawString(L, BORDER_Y + y, Chars::VT, Colors::Border);
        DrawString(R, BORDER_Y + y, Chars::VT, Colors::Border);
    }

    // Bottom row
    DrawString(L, bot, Chars::BL, Colors::Border);
    for (int x = L + 1; x < R; ++x) DrawString(x, bot, Chars::HZ, Colors::Border);
    DrawString(R, bot, Chars::BR, Colors::Border);
}

void Game::DrawHUD() const
{
    using namespace Renderer;
    // Clear HUD content row (between the two ║ walls)
    std::string blank(BOARD_W, ' ');
    DrawString(BORDER_X + 1, 1, blank, Colors::DimGray);

    std::string scoreStr  = " \xe2\x97\x86 SCORE: " + std::to_string(m_score);
    std::string lengthStr = "\xe2\x96\xa0 LENGTH: "
                            + std::to_string(m_snake.GetBody().size());
    std::string helpStr   = "ESC: Quit ";

    DrawString(BORDER_X + 1,          1, scoreStr,  Colors::ScoreVal);
    DrawString(BORDER_X + 17,         1, lengthStr, Colors::LengthCol);
    DrawString(BORDER_X + BOARD_W - 9, 1, helpStr,  Colors::HelpCol);
}

void Game::DrawSnake() const
{
    using namespace Renderer;
    const auto& body = m_snake.GetBody();

    Direction dir = m_snake.GetDirection();
    const char* head;
    switch (dir) {
    case Direction::RIGHT: head = Chars::Right; break;
    case Direction::LEFT:  head = Chars::Left;  break;
    case Direction::UP:    head = Chars::Up;    break;
    default:               head = Chars::Down;  break;
    }

    for (int i = 0; i < (int)body.size(); ++i) {
        int sx = CellToScreenX(body[i].x);
        int sy = CellToScreenY(body[i].y);
        if (i == 0)
            DrawString(sx, sy, head, Colors::SnakeHead);
        else
            DrawString(sx, sy, Chars::Body, BodyColor(i));
    }
}

void Game::DrawSnakeHead() const
{
    using namespace Renderer;
    const auto& body = m_snake.GetBody();

    Direction dir = m_snake.GetDirection();
    const char* head;
    switch (dir) {
    case Direction::RIGHT: head = Chars::Right; break;
    case Direction::LEFT:  head = Chars::Left;  break;
    case Direction::UP:    head = Chars::Up;    break;
    default:               head = Chars::Down;  break;
    }

    DrawString(CellToScreenX(body[0].x), CellToScreenY(body[0].y), head, Colors::SnakeHead);

    // Redraw neck — it displayed the head glyph last frame
    if (body.size() > 1)
        DrawString(CellToScreenX(body[1].x), CellToScreenY(body[1].y),
                   Chars::Body, BodyColor(1));
}

void Game::EraseTail(Point tail) const
{
    ClearCell(CellToScreenX(tail.x), CellToScreenY(tail.y));
}

void Game::DrawFood() const
{
    for (int i = 0; i < (int)m_foods.size(); ++i)
        Renderer::DrawString(CellToScreenX(m_foods[i].x), CellToScreenY(m_foods[i].y),
                             Renderer::Chars::Food, Renderer::FOOD_COLORS[i]);
}

void Game::ClearCell(int screenX, int screenY) const
{
    Renderer::ClearAt(screenX, screenY);
}

void Game::DrawCenteredString(int row, const std::string& s, Renderer::RGB col) const
{
    int totalWidth = BOARD_W + 4;
    int x = (totalWidth - static_cast<int>(s.size())) / 2;
    if (x < 0) x = 0;
    Renderer::DrawString(x, row, s, col);
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
    using namespace Renderer;

    // Overlay box — 28 wide, 10 tall, centred in game area
    const int boxW = 28;
    const int boxH = 10;
    const int cx   = BORDER_X + 1 + (BOARD_W - boxW) / 2;     // left col of box
    const int cy   = BORDER_Y + 1 + (BOARD_H - boxH) / 2;     // top row of box
    const int cxR  = cx + boxW - 1;
    const int cyB  = cy + boxH - 1;

    auto hline = [&](int y, const char* lc, const char* rc, const char* fill, RGB c) {
        DrawString(cx,  y, lc,   c);
        for (int x = cx + 1; x < cxR; ++x) DrawString(x, y, fill, c);
        DrawString(cxR, y, rc,   c);
    };
    auto row = [&](int y, const std::string& s, RGB c) {
        // Draw side walls + padded content
        DrawString(cx,  y, Chars::VT, Colors::BoxDim);
        // Centre the content inside the box
        int inner = boxW - 2;
        int pad   = (inner - (int)s.size()) / 2;
        std::string line(inner, ' ');
        if (pad >= 0 && pad + (int)s.size() <= inner)
            line.replace(pad, s.size(), s);
        DrawString(cx + 1, y, line, c);
        DrawString(cxR, y, Chars::VT, Colors::BoxDim);
    };

    hline(cy,  Chars::TL, Chars::TR, Chars::HZ, Colors::BoxDim);
    row(cy + 1, "",                          Colors::White);
    row(cy + 2, "G  A  M  E   O  V  E  R",  Colors::GameOverR);
    row(cy + 3, "",                          Colors::White);
    row(cy + 4, "Final Score:  " + std::to_string(m_score), Colors::ScoreVal);
    row(cy + 5, "",                          Colors::White);
    row(cy + 6, "R  =  Restart",             Colors::LengthCol);
    row(cy + 7, "Q  =  Quit",                Colors::LengthCol);
    row(cy + 8, "",                          Colors::White);
    hline(cyB, Chars::BL, Chars::BR, Chars::HZ, Colors::BoxDim);

    ResetColor();
}
