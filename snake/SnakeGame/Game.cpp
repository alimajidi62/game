//  Game.cpp — Snake game logic + ImGui DrawList rendering
//  Sprites loaded from assets\ via TextureLoader / stb_image.
//  Falls back to ImDrawList primitives if textures fail to load.

// Pull in D3D11 before anything else (TextureLoader.h needs it)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

// Define stb_image implementation here (exactly once in the project)
#define TEXTURE_LOADER_IMPLEMENTATION
#include "TextureLoader.h"

#include "Game.h"
#include "Renderer.h"
#include "SpriteRenderer.h"
#include "Renderer3D.h"
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
    , m_spritesLoaded(false)
{
    SpawnAllFood();
}

// ============================================================
//  LoadSprites — called once after DX11 device is ready
//  Also initialises the 3-D renderer (ctx and bbW/H required for that path).
// ============================================================
void Game::LoadSprites(ID3D11Device* device,
                       ID3D11DeviceContext* ctx,
                       UINT bbWidth, UINT bbHeight)
{
    if (!device) return;

    // ---- Initialise 3-D renderer ----
    if (ctx)
    {
        if (!m_renderer3D.Init(device, ctx, bbWidth, bbHeight))
            OutputDebugStringA("[Game] 3-D renderer failed to initialise — 3-D view disabled.\n");
    }

    // Build a helper lambda that prepends the assets path.
    // __FILE__ gives us the .cpp path; we derive assets\ relative to the
    // project directory at runtime using a hard-coded relative path that
    // is correct when the working directory is the project output folder.
    // We try two common working directories (Debug/x64 output, and the
    // project source directory itself).
    auto tryLoad = [&](const char* filename) -> ID3D11ShaderResourceView*
    {
        // Candidates in priority order
        const char* prefixes[] = {
            "assets\\",                    // CWD = project source dir (F5 without debugger or Ctrl+F5)
            "..\\assets\\",                // CWD = Debug\ or Release\ output subdirectory
            "..\\..\\assets\\",            // CWD = x64\Debug\ etc.
            "..\\..\\..\\assets\\",        // CWD = deeper build dirs
            "SnakeGame\\assets\\",         // CWD = solution root
        };
        for (const char* prefix : prefixes)
        {
            std::string fullPath = std::string(prefix) + filename;
            ID3D11ShaderResourceView* srv = LoadTextureFromFile(device, fullPath.c_str());
            if (srv) return srv;
        }
        return nullptr;
    };

    m_sprites.headUp    = tryLoad("head_up.png");
    m_sprites.headDown  = tryLoad("head_down.png");
    m_sprites.headLeft  = tryLoad("head_left.png");
    m_sprites.headRight = tryLoad("head_right.png");

    m_sprites.bodyH = tryLoad("body_horizontal.png");
    m_sprites.bodyV = tryLoad("body_vertical.png");

    // Corner sprites — named after the quadrant the curve fills in the cell.
    // body_topright   → curve in top-right corner  (connects L↔D or U↔R)
    // body_topleft    → curve in top-left corner   (connects R↔D or U↔L)
    // body_bottomright→ curve in bottom-right corner (connects L↔U or D↔R)
    // body_bottomleft → curve in bottom-left corner  (connects R↔U or D↔L)
    m_sprites.cornerTR = tryLoad("body_topright.png");
    m_sprites.cornerTL = tryLoad("body_topleft.png");
    m_sprites.cornerBR = tryLoad("body_bottomright.png");
    m_sprites.cornerBL = tryLoad("body_bottomleft.png");

    m_sprites.tailUp    = tryLoad("tail_up.png");
    m_sprites.tailDown  = tryLoad("tail_down.png");
    m_sprites.tailLeft  = tryLoad("tail_left.png");
    m_sprites.tailRight = tryLoad("tail_right.png");

    m_sprites.food = tryLoad("apple.png");

    m_spritesLoaded = m_sprites.IsLoaded();

    if (m_spritesLoaded)
        OutputDebugStringA("[Game] All snake sprites loaded successfully.\n");
    else
        OutputDebugStringA("[Game] One or more sprites failed to load — using primitive fallback.\n");
}

// ============================================================
//  On3DResize — forward resize to the 3-D renderer
// ============================================================
void Game::On3DResize(UINT newW, UINT newH)
{
    m_renderer3D.OnResize(newW, newH);
}

// ============================================================
//  PickCornerSprite
//  'in'  = direction FROM the previous segment (closer to head) INTO this segment
//  'out' = direction FROM this segment INTO the next segment (closer to tail)
//
//  Corner sprite visual reference (the curve arc fills the named quadrant):
//
//    body_topright.png   — arc in top-right quadrant; pipe opens LEFT & DOWN
//                          → snake travels LEFT→DOWN or DOWN→LEFT through this cell
//
//    body_topleft.png    — arc in top-left quadrant;  pipe opens RIGHT & DOWN
//                          → snake travels RIGHT→DOWN or DOWN→RIGHT through this cell
//
//    body_bottomright.png — arc in bottom-right quadrant; pipe opens LEFT & UP
//                          → snake travels LEFT→UP or UP→LEFT through this cell
//
//    body_bottomleft.png  — arc in bottom-left quadrant;  pipe opens RIGHT & UP
//                          → snake travels RIGHT→UP or UP→RIGHT through this cell
// ============================================================
ID3D11ShaderResourceView* Game::PickCornerSprite(Direction in, Direction out) const
{
    using D = Direction;

    // 'in' is the travel direction INTO this cell, so the entry opening is on the OPPOSITE side.
    // 'out' is the travel direction OUT of this cell, so the exit opening is on that same side.
    //
    // body_topright  — arc in top-right; openings at BOTTOM & LEFT
    //   entered from LEFT (in=RIGHT) exiting BOTTOM (out=DOWN), or entered from BOTTOM (in=UP) exiting LEFT (out=LEFT)
    if ((in == D::RIGHT && out == D::DOWN) || (in == D::UP   && out == D::LEFT))  return m_sprites.cornerTR;
    // body_topleft   — arc in top-left;  openings at BOTTOM & RIGHT
    //   entered from RIGHT (in=LEFT) exiting BOTTOM (out=DOWN), or entered from BOTTOM (in=UP) exiting RIGHT (out=RIGHT)
    if ((in == D::LEFT  && out == D::DOWN) || (in == D::UP   && out == D::RIGHT)) return m_sprites.cornerTL;
    // body_bottomright — arc in bottom-right; openings at TOP & LEFT
    //   entered from LEFT (in=RIGHT) exiting TOP (out=UP), or entered from TOP (in=DOWN) exiting LEFT (out=LEFT)
    if ((in == D::RIGHT && out == D::UP)   || (in == D::DOWN && out == D::LEFT))  return m_sprites.cornerBR;
    // body_bottomleft  — arc in bottom-left;  openings at TOP & RIGHT
    //   entered from RIGHT (in=LEFT) exiting TOP (out=UP), or entered from TOP (in=DOWN) exiting RIGHT (out=RIGHT)
    if ((in == D::LEFT  && out == D::UP)   || (in == D::DOWN && out == D::RIGHT)) return m_sprites.cornerBL;

    // Straight segments — shouldn't reach here from DrawSnake, but safe fallback
    if (in == D::LEFT || in == D::RIGHT) return m_sprites.bodyH;
    return m_sprites.bodyV;
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

    // --- Playing: direction input (IsKeyDown for held keys) ---
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

    m_snake.SetDirection(m_pendingDir);
    m_snake.Move(m_pendingGrow);
    m_pendingGrow = false;

    // Wrap head to opposite edge instead of dying on wall
    m_snake.WrapHead(BOARD_W, BOARD_H);

    const Point& head = m_snake.GetHead();

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
            m_pendingGrow = true;
            food          = SpawnOneFood();
            break;
        }
    }
}

// ============================================================
//  Render — 3-D scene first, then ImGui HUD / overlays on top
// ============================================================
void Game::Render(float winW, float winH, ID3D11RenderTargetView* rtv)
{
    // Store rtv for the 3-D pass (borrowed reference, not owned)
    m_rtv = rtv;

    // ---- 3-D render pass (runs before ImGui so it lands under the HUD) ----
    // Only draw the 3-D scene during Playing and GameOver phases.
    // During the Title screen show only the 2-D ImGui overlay.
    if (m_renderer3D.IsInitialised() && rtv &&
        (m_phase == GamePhase::Playing || m_phase == GamePhase::GameOver))
    {
        m_renderer3D.DrawScene(rtv,
                               m_snake.GetBody(),
                               m_foods,
                               BOARD_W, BOARD_H);
        // DrawScene leaves the RTV bound (without DSV) — ImGui takes over from here.
    }

    // ---- ImGui overlay pass ----
    // Background fill (dark navy) is only drawn when 3-D renderer is inactive
    // (title screen), so the 3-D rendered scene shows through otherwise.
    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
    if (!m_renderer3D.IsInitialised() || m_phase == GamePhase::Title)
        bgDl->AddRectFilled({0.f, 0.f}, {winW, winH}, ToImU32(10, 10, 20));

    // Board origin is still used for HUD positioning — keep the calculation
    // even though the 2-D board/snake/food are not drawn in 3-D mode.
    constexpr float hudH = 36.0f;
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
        Render3DOverlayHUD(winW, winH);   // HUD only — no 2-D board/snake/food
        break;
    case GamePhase::GameOver:
        Render3DOverlayHUD(winW, winH);
        RenderGameOver(dl, winW, winH);
        break;
    }
}

// ============================================================
//  Board drawing
// ============================================================
void Game::DrawBoard(ImDrawList* dl, float ox, float oy) const
{
    float boardPxW = BOARD_W * CELL_PX;
    float boardPxH = BOARD_H * CELL_PX;

    dl->AddRectFilled({ox, oy}, {ox + boardPxW, oy + boardPxH},
                      ToImU32(Colors::BoardBg));

    DrawGrid(dl, ox, oy, BOARD_W, BOARD_H);

    dl->AddRect({ox - 1.f, oy - 1.f},
                {ox + boardPxW + 1.f, oy + boardPxH + 1.f},
                ToImU32(Colors::Border), 0.0f, 2.0f);
}

// ============================================================
//  DrawSnake — sprite path + primitive fallback
// ============================================================
void Game::DrawSnake(ImDrawList* dl, float ox, float oy) const
{
    const auto& body = m_snake.GetBody();
    const int   n    = (int)body.size();

    if (n == 0) return;

    // -------------------------------------------------------
    //  SPRITE PATH
    // -------------------------------------------------------
    if (m_spritesLoaded)
    {
        // Helper: direction FROM cell 'a' TO cell 'b'
        auto dirAtoB = [](const Point& a, const Point& b) -> Direction
        {
            if (b.x > a.x) return Direction::RIGHT;
            if (b.x < a.x) return Direction::LEFT;
            if (b.y > a.y) return Direction::DOWN;
            return Direction::UP;
        };

        // Draw back-to-front so head is on top
        for (int i = n - 1; i >= 0; --i)
        {
            const Point& seg = body[i];

            if (i == 0)
            {
                // HEAD — pick sprite matching current movement direction
                ID3D11ShaderResourceView* srv = nullptr;
                switch (m_snake.GetDirection())
                {
                case Direction::UP:    srv = m_sprites.headUp;    break;
                case Direction::DOWN:  srv = m_sprites.headDown;  break;
                case Direction::LEFT:  srv = m_sprites.headLeft;  break;
                case Direction::RIGHT: srv = m_sprites.headRight; break;
                }
                DrawSprite(dl, ox, oy, seg.x, seg.y, srv, CELL_PX);
            }
            else if (i == n - 1)
            {
                // TAIL — direction the tail tip points equals the direction
                // from segment[n-2] to segment[n-1] (the tail cell).
                Direction tailDir = (n >= 2)
                    ? dirAtoB(body[n - 2], body[n - 1])
                    : m_snake.GetDirection();

                ID3D11ShaderResourceView* srv = nullptr;
                switch (tailDir)
                {
                case Direction::UP:    srv = m_sprites.tailUp;    break;
                case Direction::DOWN:  srv = m_sprites.tailDown;  break;
                case Direction::LEFT:  srv = m_sprites.tailLeft;  break;
                case Direction::RIGHT: srv = m_sprites.tailRight; break;
                }
                DrawSprite(dl, ox, oy, seg.x, seg.y, srv, CELL_PX);
            }
            else
            {
                // BODY — determine if straight or corner
                // 'in'  = direction FROM the previous segment (i-1) INTO this one (i)
                // 'out' = direction FROM this segment (i) INTO the next one (i+1)
                Direction in  = dirAtoB(body[i - 1], body[i]);
                Direction out = dirAtoB(body[i],     body[i + 1]);

                ID3D11ShaderResourceView* srv = nullptr;

                bool isStraight =
                    (in == Direction::LEFT  || in == Direction::RIGHT) &&
                    (out == Direction::LEFT || out == Direction::RIGHT);
                isStraight = isStraight ||
                    ((in == Direction::UP   || in == Direction::DOWN) &&
                     (out == Direction::UP  || out == Direction::DOWN));

                bool isCorner = !isStraight;
                if (isStraight)
                {
                    srv = (in == Direction::LEFT || in == Direction::RIGHT)
                        ? m_sprites.bodyH
                        : m_sprites.bodyV;
                }
                else
                {
                    srv = PickCornerSprite(in, out);
                }

                DrawSprite(dl, ox, oy, seg.x, seg.y, srv, CELL_PX, IM_COL32_WHITE, isCorner);
            }
        }
        return;
    }

    // -------------------------------------------------------
    //  PRIMITIVE FALLBACK (original ImDrawList drawing)
    // -------------------------------------------------------
    for (int i = n - 1; i >= 0; --i)
    {
        if (i == 0)
        {
            DrawSnakeHead(dl, ox, oy);
        }
        else
        {
            RGB c = BodyColor(i);
            DrawCellRect(dl, ox, oy, body[i].x, body[i].y,
                         ToImU32(c, 230), 2.0f, GAP_PX);
        }
    }
}

// ============================================================
//  DrawSnakeHead — sprite path + primitive fallback (face with eyes)
// ============================================================
void Game::DrawSnakeHead(ImDrawList* dl, float ox, float oy) const
{
    const Point& head = m_snake.GetHead();
    Direction    dir  = m_snake.GetDirection();

    // If sprites loaded, the head was already drawn in DrawSnake().
    // This function is only called from the primitive fallback path.

    // Check for open-mouth (food directly ahead)
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

    float x0 = ox + head.x * CELL_PX;
    float y0 = oy + head.y * CELL_PX;
    float x1 = x0 + CELL_PX;
    float y1 = y0 + CELL_PX;
    float cx = x0 + CELL_PX * 0.5f;
    float cy = y0 + CELL_PX * 0.5f;
    float h  = CELL_PX * 0.5f;

    // Base head rectangle
    dl->AddRectFilled({x0, y0}, {x1, y1}, IM_COL32(130, 255, 60, 255), 4.0f);
    dl->AddRect({x0, y0}, {x1, y1}, IM_COL32(255, 255, 100, 190), 4.0f, 1.5f);

    float fdx = 0.f, fdy = 0.f;
    float sdx = 0.f, sdy = 0.f;
    float mouthEdgeX = cx, mouthEdgeY = cy;
    switch (dir) {
    case Direction::RIGHT: fdx= 1.f; sdy= 1.f; mouthEdgeX=x1; mouthEdgeY=cy; break;
    case Direction::LEFT:  fdx=-1.f; sdy= 1.f; mouthEdgeX=x0; mouthEdgeY=cy; break;
    case Direction::UP:    fdy=-1.f; sdx= 1.f; mouthEdgeX=cx; mouthEdgeY=y0; break;
    case Direction::DOWN:  fdy= 1.f; sdx= 1.f; mouthEdgeX=cx; mouthEdgeY=y1; break;
    }

    float eyeBack = h * 0.28f;
    float eyeSide = h * 0.44f;
    float eyeR    = h * 0.28f;
    float pupR    = h * 0.14f;
    float pupFwd  = pupR * 0.6f;

    ImVec2 e1 = { cx - fdx*eyeBack + sdx*eyeSide, cy - fdy*eyeBack + sdy*eyeSide };
    ImVec2 e2 = { cx - fdx*eyeBack - sdx*eyeSide, cy - fdy*eyeBack - sdy*eyeSide };
    ImVec2 p1 = { e1.x + fdx*pupFwd, e1.y + fdy*pupFwd };
    ImVec2 p2 = { e2.x + fdx*pupFwd, e2.y + fdy*pupFwd };

    dl->AddCircleFilled(e1, eyeR, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(e2, eyeR, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(p1, pupR, IM_COL32(10,  20,  10,  255));
    dl->AddCircleFilled(p2, pupR, IM_COL32(10,  20,  10,  255));

    if (!mouthOpen)
    {
        float smileR = h * 0.38f;
        ImVec2 arcCtr = { mouthEdgeX - fdx * smileR * 0.6f,
                          mouthEdgeY - fdy * smileR * 0.6f };
        float fwdAngle = atan2f(fdy, fdx);
        dl->PathArcTo(arcCtr, smileR, fwdAngle - 0.7f, fwdAngle + 0.7f, 8);
        dl->PathStroke(IM_COL32(170, 30, 30, 210), false, 1.3f);
    }
    else
    {
        float openW = h * 0.52f;
        float openD = h * 0.55f;
        ImVec2 jaw1 = { mouthEdgeX - fdx*openD*0.4f + sdx*openW,
                        mouthEdgeY - fdy*openD*0.4f + sdy*openW };
        ImVec2 jaw2 = { mouthEdgeX - fdx*openD*0.4f - sdx*openW,
                        mouthEdgeY - fdy*openD*0.4f - sdy*openW };
        ImVec2 tip  = { mouthEdgeX + fdx*openD*0.35f,
                        mouthEdgeY + fdy*openD*0.35f };

        dl->AddTriangleFilled(jaw1, jaw2, tip, IM_COL32(200, 28, 28, 235));
        dl->AddTriangle(jaw1, jaw2, tip, IM_COL32(100, 8,  8,  200), 1.0f);

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

// ============================================================
//  DrawFood — sprite path (apple.png) + primitive fallback
// ============================================================
void Game::DrawFood(ImDrawList* dl, float ox, float oy) const
{
    for (int i = 0; i < (int)m_foods.size(); ++i)
    {
        if (m_spritesLoaded && m_sprites.food)
        {
            // Use apple sprite; tint each food item a different colour so
            // the 5 apples remain visually distinct (same as the fallback).
            // We encode the tint as a slight hue shift via IM_COL32 overlays.
            // The tints below map to the original FOOD_COLORS palette.
            static const ImU32 kFoodTints[5] = {
                IM_COL32(255, 120, 120, 255),   // reddish
                IM_COL32(255, 200, 100, 255),   // orange
                IM_COL32(240, 240,  80, 255),   // yellow
                IM_COL32( 80, 230, 230, 255),   // cyan
                IM_COL32(255, 120, 220, 255),   // pink
            };
            ImU32 tint = kFoodTints[i % 5];
            DrawSprite(dl, ox, oy, m_foods[i].x, m_foods[i].y,
                       m_sprites.food, CELL_PX, tint);
        }
        else
        {
            // Primitive fallback: coloured circle + glow ring
            const RGB& fc = FOOD_COLORS[i];
            DrawCellGlow(dl, ox, oy, m_foods[i].x, m_foods[i].y, ToImU32(fc, 55));
            DrawCellCircle(dl, ox, oy, m_foods[i].x, m_foods[i].y, ToImU32(fc), 0.32f);
        }
    }
}

// ============================================================
//  DrawHUD
// ============================================================
void Game::DrawHUD(float ox, float /*oy*/, float boardPxW, float /*boardPxH*/) const
{
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoScrollbar        |
        ImGuiWindowFlags_NoBackground       |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos ({ox,         4.0f},  ImGuiCond_Always);
    ImGui::SetNextWindowSize({boardPxW, 28.0f},   ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    if (ImGui::Begin("##hud", nullptr, flags))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.16f, 1.f));
        ImGui::Text("SCORE: %d", m_score);
        ImGui::PopStyleColor();

        ImGui::SameLine(boardPxW * 0.35f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.82f, 1.f, 1.f));
        ImGui::Text("LENGTH: %d", (int)m_snake.GetBody().size());
        ImGui::PopStyleColor();

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

// RenderPlaying — legacy 2-D path (kept for reference; not called in 3-D mode)
void Game::RenderPlaying(ImDrawList* dl, float /*winW*/, float /*winH*/)
{
    float ox = m_boardOriginX;
    float oy = m_boardOriginY;
    DrawBoard(dl, ox, oy);
    DrawFood (dl, ox, oy);
    DrawSnake(dl, ox, oy);
    DrawHUD  (ox, oy, BOARD_W * CELL_PX, BOARD_H * CELL_PX);
}

// Render3DOverlayHUD — HUD-only overlay for the 3-D scene view.
// Draws the score/length bar and a small "ESC: Quit" hint at the top of the
// window without touching the board area (the 3-D scene fills that).
void Game::Render3DOverlayHUD(float winW, float /*winH*/)
{
    DrawHUD(m_boardOriginX, m_boardOriginY,
            BOARD_W * CELL_PX, BOARD_H * CELL_PX);

    // Semi-transparent dark bar across the top so HUD text is legible over 3-D
    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
    bgDl->AddRectFilled({0.f, 0.f}, {winW, 36.f},
                        IM_COL32(8, 8, 16, 210));
}

void Game::RenderTitle(ImDrawList* /*dl*/, float winW, float winH)
{
    float popW = 480.0f, popH = 350.0f;
    ImGui::SetNextWindowPos ({(winW - popW) * 0.5f, (winH - popH) * 0.5f}, ImGuiCond_Always);
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

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 0.63f, 0.63f, 1.f));
        ImGui::Text("  W / Up Arrow        Move Up");
        ImGui::Text("  S / Down Arrow      Move Down");
        ImGui::Text("  A / Left Arrow      Move Left");
        ImGui::Text("  D / Right Arrow     Move Right");
        ImGui::Text("  ESC                 Quit");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("  Collect: ");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImDrawList* wdl   = ImGui::GetWindowDrawList();
        ImVec2      dotBase = ImGui::GetCursorScreenPos();
        dotBase.y += ImGui::GetTextLineHeight() * 0.45f;
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
    ImGui::SetNextWindowPos ({(winW - popW) * 0.5f, (winH - popH) * 0.5f}, ImGuiCond_Always);
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

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.20f, 0.20f, 1.f));
        const char* goStr = "G  A  M  E   O  V  E  R";
        float gw = ImGui::CalcTextSize(goStr).x;
        ImGui::SetCursorPosX((cw - gw) * 0.5f);
        ImGui::Text("%s", goStr);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        std::string scoreStr = "Final Score:  " + std::to_string(m_score);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.9f, 0.16f, 1.f));
        float fs = ImGui::CalcTextSize(scoreStr.c_str()).x;
        ImGui::SetCursorPosX((cw - fs) * 0.5f);
        ImGui::Text("%s", scoreStr.c_str());
        ImGui::PopStyleColor();

        std::string lenStr = "Length:  " + std::to_string((int)m_snake.GetBody().size());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.82f, 1.f, 1.f));
        float ls = ImGui::CalcTextSize(lenStr.c_str()).x;
        ImGui::SetCursorPosX((cw - ls) * 0.5f);
        ImGui::Text("%s", lenStr.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

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
