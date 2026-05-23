#pragma once
//  Renderer3D.h — 3-D render pass for the Snake game.
//
//  Draws the board floor, food spheres, and snake segments using a simple
//  Lambertian diffuse + ambient lighting model via D3D11.
//
//  Coordinate convention:
//    +X = board column direction (left → right)
//    +Y = up (out of the board surface)
//    +Z = board row direction (top → bottom in grid coordinates)
//
//  Board world footprint:
//    X: [0 .. BOARD_W]   (e.g. 0..20)
//    Z: [0 .. BOARD_H]   (e.g. 0..20)
//    Y: 0 (floor surface)
//
//  The camera looks from above-left toward the board centre at ~45° elevation.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <vector>
#include <deque>
#include <cstdint>

// Pull in Point / Direction definitions (Snake.h has no heavy dependencies)
#include "Snake.h"

using namespace DirectX;

// ---------------------------------------------------------------------------
//  Vertex type used by all 3-D geometry
// ---------------------------------------------------------------------------
struct Vertex3D
{
    float x, y, z;   // position
    float nx, ny, nz; // normal
};

// ---------------------------------------------------------------------------
//  Per-draw-call constant buffer layout (must match the HLSL cbuffer)
// ---------------------------------------------------------------------------
struct alignas(16) CBPerDraw
{
    XMFLOAT4X4 WorldViewProj;   // 64 bytes
    XMFLOAT4X4 World;           // 64 bytes
    XMFLOAT4   LightDir;        // 16 bytes  (direction TO light, world space)
    XMFLOAT4   BaseColor;       // 16 bytes
    XMFLOAT4   AmbientColor;    // 16 bytes
    // total = 176 bytes; constant buffers must be a multiple of 16 — OK
};

// ---------------------------------------------------------------------------
//  Renderer3D class
// ---------------------------------------------------------------------------
class Renderer3D
{
public:
    Renderer3D() = default;
    ~Renderer3D() { Shutdown(); }

    // Call once after the D3D11 device is ready.
    // Creates shaders, geometry buffers, and the depth/stencil buffer sized
    // to (bbWidth x bbHeight) — pass the initial back-buffer dimensions.
    bool Init(ID3D11Device*        device,
              ID3D11DeviceContext* ctx,
              UINT                 bbWidth,
              UINT                 bbHeight);

    // Call when the swap-chain is resized so the depth buffer matches the new
    // back-buffer dimensions.
    void OnResize(UINT newWidth, UINT newHeight);

    // ---------------------------------------------------------------------------
    //  Draw the full 3-D scene.
    //
    //  Parameters:
    //    rtv        — the current back-buffer RTV (set before calling ImGui render)
    //    snakeBody  — deque front=head, back=tail
    //    foods      — positions of all food items
    //    boardW/H   — board cell counts (BOARD_W, BOARD_H)
    // ---------------------------------------------------------------------------
    void DrawScene(ID3D11RenderTargetView*        rtv,
                   const std::deque<Point>&       snakeBody,
                   const std::vector<Point>&      foods,
                   int                            boardW,
                   int                            boardH);

    // Free all D3D resources.
    void Shutdown();

    bool IsInitialised() const { return m_initialised; }

private:
    // ---- Shader compilation helpers ----
    bool CompileShaders();

    // ---- Geometry builders ----
    //  Each function fills 'verts' and 'indices' with the geometry for one
    //  archetype and uploads them into a pair of immutable D3D11 buffers.
    bool BuildBoxGeometry();   // 1x1x1 box centred at origin, for snake segments
    bool BuildSphereGeometry();// UV-sphere radius 0.35, for food
    bool BuildFloorGeometry(int boardW, int boardH); // flat quad at Y=0

    // ---- D3D11 helper to create an immutable VB + IB from in-memory data ----
    bool CreateMesh(const std::vector<Vertex3D>&  verts,
                    const std::vector<uint16_t>&  indices,
                    ID3D11Buffer**                ppVB,
                    ID3D11Buffer**                ppIB,
                    UINT&                         indexCount);

    // ---- Per-draw helper ----
    void DrawMesh(ID3D11Buffer* vb, ID3D11Buffer* ib, UINT indexCount,
                  const XMMATRIX& world,
                  const XMFLOAT4& baseColor);

    // ---- Camera helpers ----
    void RebuildViewProj(int boardW, int boardH);

    // ---- D3D objects ----
    ID3D11Device*              m_device  = nullptr;
    ID3D11DeviceContext*       m_ctx     = nullptr;

    ID3D11VertexShader*        m_vs      = nullptr;
    ID3D11PixelShader*         m_ps      = nullptr;
    ID3D11InputLayout*         m_layout  = nullptr;
    ID3D11Buffer*              m_cb      = nullptr;   // constant buffer

    // Box (snake segments)
    ID3D11Buffer*              m_boxVB       = nullptr;
    ID3D11Buffer*              m_boxIB       = nullptr;
    UINT                       m_boxIdxCount = 0;

    // Sphere (food)
    ID3D11Buffer*              m_sphereVB       = nullptr;
    ID3D11Buffer*              m_sphereIB       = nullptr;
    UINT                       m_sphereIdxCount = 0;

    // Floor quad
    ID3D11Buffer*              m_floorVB       = nullptr;
    ID3D11Buffer*              m_floorIB       = nullptr;
    UINT                       m_floorIdxCount = 0;

    // Depth/stencil
    ID3D11Texture2D*           m_dsTex = nullptr;
    ID3D11DepthStencilView*    m_dsv   = nullptr;
    ID3D11DepthStencilState*   m_dss   = nullptr;

    // Rasteriser / blend states
    ID3D11RasterizerState*     m_rsState    = nullptr;
    ID3D11BlendState*          m_blendState = nullptr;

    // Camera matrices (rebuilt once at init; board dimensions don't change)
    XMMATRIX                   m_view;
    XMMATRIX                   m_proj;

    // Light direction (world space, pointing TO the light)
    XMFLOAT4                   m_lightDir = {0.4f, 0.9f, -0.3f, 0.0f};
    XMFLOAT4                   m_ambient  = {0.18f, 0.18f, 0.22f, 1.0f};

    UINT                       m_bbW = 0, m_bbH = 0;
    bool                       m_initialised = false;
};
