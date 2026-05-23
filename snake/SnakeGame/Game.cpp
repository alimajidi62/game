//  Game.cpp — Snake game logic + ImGui DrawList rendering
//  No console API; all drawing goes through ImGui DrawList.

#include "Game.h"
#include "Renderer.h"
#include "imgui/imgui.h"

#include <string>
#include <cmath>

using namespace Renderer;
using namespace std::chrono;

// ============================================================
//  Constructor
// ============================================================
Game::Game()
    : m_phase(GamePhase::Title)
    , m_snake(BOARD_W / 2, BOARD_H / 2)
    , m_score(0)
    , m_pendingDir(Direction::RIGHT)
    , m_pendingGrow(false)
    , m_quit(false)
    , m_boardOriginX(0.f)
    , m_boardOriginY(0.f)
    , m_lastTick(Clock::now())
    , m_rng(std::random_device{}())
    , m_distX(0, BOARD_W - 1)
    , m_distY(0, BOARD_H - 1)
{
    SpawnAllFood();
}

// ============================================================
//  ProcessInput — read ImGui keyboard state, no blocking
// ============================================================
void Game::ProcessInput()
{
    // ESC always quits
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_quit = true;
        return;
    }

    // --- Title screen ---
    if (m_phase == GamePhase::Title)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Space, false))
        {
            ResetGame();
            m_phase = GamePhase::Playing;
        }
        return;
    }

    // --- Game over ---
    if (m_phase == GamePhase::GameOver)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_R, false))
        {
            ResetGame();
            m_phase = GamePhase::Playing;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            m_quit = true;
        return;
    }

    // --- Playing: direction input (IsKeyDown for held keys, one update per tick) ---
    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))
        m_pendingDir = Direction::UP;
    else if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))
        m_pendingDir = Direction::DOWN;
    else if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))
        m_pendingDir = Direction::LEFT;
    else if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))
        m_pendingDir = Direction::RIGHT;
}

// ============================================================
//  Update — advance game logic once per TICK_MS ms
// ============================================================
void Game::Update()
{
    if (m_phase != GamePhase::Playing)
        return;

    auto now     = Clock::now();
    auto elapsed = duration_cast<milliseconds>(now - m_lastTick).count();
    if (elapsed < TICK_MS)
        return;
    m_lastTick = now;

    // Apply buffered direction (ignores 180-degree reversals inside Snake)
    m_snake.SetDirection(m_pendingDir);

    // Move snake; if m_pendingGrow is set from last tick, pass grew=true
    // (Snake::Move(true) keeps the tail, making the snake one segment longer)
    m_snake.Move(m_pendingGrow);
    m_pendingGrow = false;

    const Point& head = m_snake.GetHead();

    // --- Wall collision ---
    if (head.x < 0 || head.x >= BOARD_W ||
        head.y < 0 || head.y >= BOARD_H)
    {
        m_phase = GamePhase::GameOver;
        return;
    }

    // --- Self collision ---
    if (m_snake.SelfCollision())
    {
        m_phase = GamePhase::GameOver;
        return;
    }

    // --- Food collision ---
    for (Point& food : m_foods)
    {
        if (head == food)
        {
            m_score      += 10;
            m_pendingGrow = true;            // grow on next tick
            food          = SpawnOneFood();  // replace this apple
            break;
        }
    }
}

// ============================================================
//  Render — full redraw each frame via ImGui DrawList
// ============================================================
void Game::Render(float winW, float winH)
{
    // Window background (covers the whole DX11 viewport)
    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
    bgDl->AddRectFilled({0.f, 0.f}, {winW, winH}, ToImU32(10, 10, 20));

    // Compute centred board origin; reserve 36 px at top for HUD
    constexpr float hudH   = 36.0f;
    float boardPxW = BOARD_W * CELL_PX;
    float boardPxH = BOARD_H * CELL_PX;
    m_boardOriginX = (winW - boardPxW) * 0.5f;
    m_boardOriginY = hudH + (winH - hudH - boardPxH) * 0.5f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    switch (m_phase)
    {
    case GamePhase::Title:
        RenderTitle(dl, winW, winH);
        break;

    case GamePhase::Playing:
        RenderPlaying(dl, winW, winH);
        break;

    case GamePhase::GameOver:
        RenderPlaying(dl, winW, winH);   // show frozen board behind overlay
        RenderGameOver(dl, winW, winH);
        break;
    }
}

// ============================================================
//  Board / snake / food drawing helpers
// ============================================================
void Game::DrawBoard(ImDrawList* dl, float ox, float oy) const
{
    float boardPxW = BOARD_W * CELL_PX;
    float boardPxH = BOARD_H * CELL_PX;

    // Board fill
    dl->AddRectFilled({ox, oy}, {ox + boardPxW, oy + boardPxH},
                      ToImU32(Colors::BoardBg));

    // Subtle grid lines
    DrawGrid(dl, ox, oy, BOARD_W, BOARD_H);

    // Bright border (1-px outside the board area)
    dl->AddRect({ox - 1.f, oy - 1.f},
                {ox + boardPxW + 1.f, oy + boardPxH + 1.f},
                ToImU32(Colors::Border), 0.0f, 2.0f);
}

void Game::DrawSnake(ImDrawList* dl, float ox, float oy) const
{
    const auto& body = m_snake.GetBody();

    // Draw back-to-front so head always renders on top
    for (int i = (int)body.size() - 1; i >= 0; --i)
    {
        if (i == 0)
        {
            DrawSnakeHead(dl, ox, oy);
        }
        else
        {
            // Body — gradient neck->tail with gap between segments
            RGB c = BodyColor(i);
            DrawCellRect(dl, ox, oy, body[i].x, body[i].y,
                         ToImU32(c, 230), 2.0f, GAP_PX);
        }
    }
}

void Game::DrawSnakeHead(ImDrawList* dl, float ox, float oy) const
{
    const Point& head = m_snake.GetHead();
    Direction    dir  = m_snake.GetDirection();

    // Open mouth when food is directly in the next cell we are moving into
    bool mouthOpen = false;
    {
        Point front = head;
        switch (dir) {
        case Direction::RIGHT: front.x++; break;
        case Direction::LEFT:  front.x--; break;
        case Direction::UP:    front.y--; break;
        case Direction::DOWN:  front.y++; break;
        }
        for (const Point& f : m_foods)
            if (f == front) { mouthOpen = true; break; }
    }

    // Cell bounds (head takes full cell, no gap)
    float x0 = ox + head.x * CELL_PX;
    float y0 = oy + head.y * CELL_PX;
    float x1 = x0 + CELL_PX;
    float y1 = y0 + CELL_PX;
    float cx = x0 + CELL_PX * 0.5f;
    float cy = y0 + CELL_PX * 0.5f;
    float h  = CELL_PX * 0.5f;   // half cell = 9 px

    // --- Base head rectangle ---
    dl->AddRectFilled({x0, y0}, {x1, y1}, IM_COL32(130, 255, 60, 255), 4.0f);
    dl->AddRect({x0, y0}, {x1, y1}, IM_COL32(255, 255, 100, 190), 4.0f, 1.5f);

    // --- Direction vectors (forward fd, sideways sd) ---
    float fdx = 0.f, fdy = 0.f;   // forward unit vector
    float sdx = 0.f, sdy = 0.f;   // sideways unit vector (perpendicular)
    float mouthEdgeX = cx, mouthEdgeY = cy; // centre of the front edge
    switch (dir) {
    case Direction::RIGHT: fdx= 1.f; sdy= 1.f; mouthEdgeX=x1; mouthEdgeY=cy; break;
    case Direction::LEFT:  fdx=-1.f; sdy= 1.f; mouthEdgeX=x0; mouthEdgeY=cy; break;
    case Direction::UP:    fdy=-1.f; sdx= 1.f; mouthEdgeX=cx; mouthEdgeY=y0; break;
    case Direction::DOWN:  fdy= 1.f; sdx= 1.f; mouthEdgeX=cx; mouthEdgeY=y1; break;
    }

    // --- Eyes (sit back from centre, spread sideways) ---
    float eyeBack = h * 0.28f;   // distance behind centre
    float eyeSide = h * 0.44f;   // lateral offset from centre
    float eyeR    = h * 0.28f;   // eye white radius (~2.5 px)
    float pupR    = h * 0.14f;   // pupil radius (~1.3 px)
    float pupFwd  = pupR * 0.6f; // pupils lean forward

    ImVec2 e1 = { cx - fdx*eyeBack + sdx*eyeSide,
                  cy - fdy*eyeBack + sdy*eyeSide };
    ImVec2 e2 = { cx - fdx*eyeBack - sdx*eyeSide,
                  cy - fdy*eyeBack - sdy*eyeSide };
    ImVec2 p1 = { e1.x + fdx*pupFwd, e1.y + fdy*pupFwd };
    ImVec2 p2 = { e2.x + fdx*pupFwd, e2.y + fdy*pupFwd };

    dl->AddCircleFilled(e1, eyeR, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(e2, eyeR, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(p1, pupR, IM_COL32(10,  20,  10,  255));
    dl->AddCircleFilled(p2, pupR, IM_COL32(10,  20,  10,  255));

    // --- Mouth ---
    if (!mouthOpen)
    {
        // Closed: small arc curving toward centre from the front edge
        float smileR = h * 0.38f;
        ImVec2 arcCtr = { mouthEdgeX - fdx * smileR * 0.6f,
                          mouthEdgeY - fdy * smileR * 0.6f };
        // Arc angles: spans ~80 degrees centred on the forward direction
        // atan2 of (fdy, fdx) gives angle pointing forward; we arc ±40 deg around it
        float fwdAngle = atan2f(fdy, fdx);
        dl->PathArcTo(arcCtr, smileR, fwdAngle - 0.7f, fwdAngle + 0.7f, 8);
        dl->PathStroke(IM_COL32(170, 30, 30, 210), false, 1.3f);
    }
    else
    {
        // Open mouth: filled triangle at the front edge, red inside
        float openW = h * 0.52f;  // half-width of jaw opening
        float openD = h * 0.55f;  // depth into the cell

        // Two jaw corners, one tip pointing outward
        ImVec2 jaw1 = { mouthEdgeX - fdx*openD*0.4f + sdx*openW,
                        mouthEdgeY - fdy*openD*0.4f + sdy*openW };
        ImVec2 jaw2 = { mouthEdgeX - fdx*openD*0.4f - sdx*openW,
                        mouthEdgeY - fdy*openD*0.4f - sdy*openW };
        ImVec2 tip  = { mouthEdgeX + fdx*openD*0.35f,
                        mouthEdgeY + fdy*openD*0.35f };

        dl->AddTriangleFilled(jaw1, jaw2, tip, IM_COL32(200, 28, 28, 235));
        dl->AddTriangle(jaw1, jaw2, tip, IM_COL32(100, 8,  8,  200), 1.0f);

        // Forked tongue extending forward from tip
        float tLen   = h * 0.60f;
        float tFork  = h * 0.20f;
        ImVec2 tMid  = { tip.x + fdx*tLen*0.6f, tip.y + fdy*tLen*0.6f };
        ImVec2 tEnd1 = { tMid.x + fdx*tLen*0.4f + sdx*tFork,
                         tMid.y + fdy*tLen*0.4f + sdy*tFork };
        ImVec2 tEnd2 = { tMid.x + fdx*tLen*0.4f - sdx*tFork,
                         tMid.y + fdy*tLen*0.4f - sdy*tFork };
        dl->AddLine(tip,  tMid,  IM_COL32(255, 80, 100, 230), 1.3f);
        dl->AddLine(tMid, tEnd1, IM_COL32(255, 80, 100, 210), 1.0f);
        dl->AddLine(tMid, tEnd2, IM_COL32(255, 80, 100, 210), 1.0f);
    }
}

void Game::DrawFood(ImDrawList* dl, float ox, float oy) const
{
    for (int i = 0; i < (int)m_foods.size(); ++i)
    {
        const RGB& fc = FOOD_COLORS[i];
        // Soft glow ring (large semi-transparent circle)
        DrawCellGlow(dl, ox, oy, m_foods[i].x, m_foods[i].y,
                     ToImU32(fc, 55));
        // Solid core circle
        DrawCellCircle(dl, ox, oy, m_foods[i].x, m_foods[i].y,
                       ToImU32(fc), 0.32f);
    }
}

void Game::DrawHUD(float ox, float /*oy*/, float boardPxW, float /*boardPxH*/) const
{
    // Transparent overlay window pinned above the board
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoScrollbar        |
        ImGuiWindowFlags_NoBackground       |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos ({ox,         4.0f},       ImGuiCond_Always);
    ImGui::SetNextWindowSize({boardPxW, 28.0f},        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    if (ImGui::Begin("##hud", nullptr, flags))
    {
        // Score (yellow)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.16f, 1.f));
        ImGui::Text("SCORE: %d", m_score);
        ImGui::PopStyleColor();

        // Length (cyan)
        ImGui::SameLine(boardPxW * 0.35f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.82f, 1.f, 1.f));
        ImGui::Text("LENGTH: %d", (int)m_snake.GetBody().size());
        ImGui::PopStyleColor();

        // Help hint (dim grey)
        ImGui::SameLine(boardPxW * 0.72f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.33f, 0.33f, 0.33f, 1.f));
        ImGui::Text("ESC: Quit");
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

// ============================================================
//  Phase renderers
// ============================================================
void Game::RenderPlaying(ImDrawList* dl, float /*winW*/, float /*winH*/)
{
    float ox = m_boardOriginX;
    float oy = m_boardOriginY;
    DrawBoard(dl, ox, oy);
    DrawFood (dl, ox, oy);
    DrawSnake(dl, ox, oy);
    DrawHUD  (ox, oy, BOARD_W * CELL_PX, BOARD_H * CELL_PX);
}

void Game::RenderTitle(ImDrawList* /*dl*/, float winW, float winH)
{
    float popW = 480.0f, popH = 350.0f;
    ImGui::SetNextWindowPos ({(winW - popW) * 0.5f, (winH - popH) * 0.5f},
                              ImGuiCond_Always);
    ImGui::SetNextWindowSize({popW, popH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.14f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.18f, 0.60f, 1.00f, 1.00f));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowRounding,   8.0f);

    if (ImGui::Begin("##title", nullptr, flags))
    {
        float cw = ImGui::GetContentRegionAvail().x;

        // Big title
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 1.00f, 0.31f, 1.f));
        const char* titleStr = "  S  N  A  K  E  ";
        float tw = ImGui::CalcTextSize(titleStr).x;
        ImGui::SetCursorPosX((cw - tw) * 0.5f);
        ImGui::Text("%s", titleStr);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.43f, 0.67f, 0.43f, 1.f));
        const char* subStr = "~ ~ Classic Arcade Game ~ ~";
        float sw = ImGui::CalcTextSize(subStr).x;
        ImGui::SetCursorPosX((cw - sw) * 0.5f);
        ImGui::Text("%s", subStr);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Controls
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 0.63f, 0.63f, 1.f));
        ImGui::Text("  W / Up Arrow        Move Up");
        ImGui::Text("  S / Down Arrow      Move Down");
        ImGui::Text("  A / Left Arrow      Move Left");
        ImGui::Text("  D / Right Arrow     Move Right");
        ImGui::Text("  ESC                 Quit");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Food dots row
        ImGui::Text("  Collect: ");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Draw 5 tiny food circles inline using the window draw list
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        ImVec2 dotBase  = ImGui::GetCursorScreenPos();
        dotBase.y      += ImGui::GetTextLineHeight() * 0.45f;
        for (int i = 0; i < 5; ++i)
        {
            RGB fc = FOOD_COLORS[i];
            wdl->AddCircleFilled(
                {dotBase.x + i * 22.0f + 8.0f, dotBase.y},
                7.0f, ToImU32(fc));
        }
        ImGui::Dummy({5 * 22.0f + 16.0f, ImGui::GetTextLineHeight()});

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 0.63f, 0.63f, 1.f));
        ImGui::Text("  5 apples always on the board");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Prompt
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.86f, 0.31f, 1.f));
        const char* promptStr = "Press ENTER or SPACE to start";
        float pw = ImGui::CalcTextSize(promptStr).x;
        ImGui::SetCursorPosX((cw - pw) * 0.5f);
        ImGui::Text("%s", promptStr);
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void Game::RenderGameOver(ImDrawList* /*dl*/, float winW, float winH)
{
    float popW = 320.0f, popH = 220.0f;
    ImGui::SetNextWindowPos ({(winW - popW) * 0.5f, (winH - popH) * 0.5f},
                              ImGuiCond_Always);
    ImGui::SetNextWindowSize({popW, popH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.04f, 0.04f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.80f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowRounding,   8.0f);

    if (ImGui::Begin("##gameover", nullptr, flags))
    {
        float cw = ImGui::GetContentRegionAvail().x;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);

        // "GAME OVER"
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.20f, 0.20f, 1.f));
        const char* goStr = "G  A  M  E   O  V  E  R";
        float gw = ImGui::CalcTextSize(goStr).x;
        ImGui::SetCursorPosX((cw - gw) * 0.5f);
        ImGui::Text("%s", goStr);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Final score
        std::string scoreStr = "Final Score:  " + std::to_string(m_score);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.16f, 1.f));
        float fs = ImGui::CalcTextSize(scoreStr.c_str()).x;
        ImGui::SetCursorPosX((cw - fs) * 0.5f);
        ImGui::Text("%s", scoreStr.c_str());
        ImGui::PopStyleColor();

        // Length
        std::string lenStr = "Length:  " + std::to_string((int)m_snake.GetBody().size());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.82f, 1.f, 1.f));
        float ls = ImGui::CalcTextSize(lenStr.c_str()).x;
        ImGui::SetCursorPosX((cw - ls) * 0.5f);
        ImGui::Text("%s", lenStr.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // R / Q options
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 0.63f, 0.63f, 1.f));
        const char* rStr = "R  =  Restart";
        float rw = ImGui::CalcTextSize(rStr).x;
        ImGui::SetCursorPosX((cw - rw) * 0.5f);
        ImGui::Text("%s", rStr);

        const char* qStr = "Q  =  Quit";
        float qw = ImGui::CalcTextSize(qStr).x;
        ImGui::SetCursorPosX((cw - qw) * 0.5f);
        ImGui::Text("%s", qStr);
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ============================================================
//  Internal helpers
// ============================================================
void Game::ResetGame()
{
    m_snake       = Snake(BOARD_W / 2, BOARD_H / 2);
    m_score       = 0;
    m_pendingDir  = Direction::RIGHT;
    m_pendingGrow = false;
    m_lastTick    = Clock::now();
    SpawnAllFood();
}

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
