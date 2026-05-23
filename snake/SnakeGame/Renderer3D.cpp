//  Renderer3D.cpp — Full 3-D render pass for the Snake game.
//
//  Geometry is generated procedurally (no .obj files).
//  Shaders are compiled at runtime from embedded HLSL strings (Shader3D.h).
//
//  Build note: link d3d11.lib and d3dcompiler.lib (already in .vcxproj).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "Renderer3D.h"
#include "Shader3D.h"
// Snake.h is already pulled in by Renderer3D.h (Point, Direction)

#include <deque>
#include <vector>
#include <cmath>
#include <cassert>

// Board dimensions — must match Game.h constants.
// Defined here to avoid a circular include (Game.h -> Renderer3D.h -> Game.h).
static constexpr int kBoardW = 40;
static constexpr int kBoardH = 20;

using namespace DirectX;

// ---------------------------------------------------------------------------
//  Tiny RAII helper — releases a COM pointer on scope exit
// ---------------------------------------------------------------------------
template<typename T>
struct ComRelease { T* p; ~ComRelease() { if (p) p->Release(); } };

// ---------------------------------------------------------------------------
//  Init
// ---------------------------------------------------------------------------
bool Renderer3D::Init(ID3D11Device*        device,
                      ID3D11DeviceContext*  ctx,
                      UINT                 bbWidth,
                      UINT                 bbHeight)
{
    m_device = device;
    m_ctx    = ctx;
    m_bbW    = bbWidth;
    m_bbH    = bbHeight;

    if (!CompileShaders())                    return false;
    if (!BuildBoxGeometry())                  return false;
    if (!BuildSphereGeometry())               return false;
    if (!BuildFloorGeometry(kBoardW, kBoardH)) return false;

    // ---- Constant buffer ----
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth      = sizeof(CBPerDraw);
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&bd, nullptr, &m_cb)))
        {
            OutputDebugStringA("[R3D] Failed to create constant buffer.\n");
            return false;
        }
    }

    // ---- Depth/stencil buffer ----
    {
        D3D11_TEXTURE2D_DESC dd{};
        dd.Width            = m_bbW;
        dd.Height           = m_bbH;
        dd.MipLevels        = 1;
        dd.ArraySize        = 1;
        dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dd.SampleDesc.Count = 1;
        dd.Usage            = D3D11_USAGE_DEFAULT;
        dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        if (FAILED(m_device->CreateTexture2D(&dd, nullptr, &m_dsTex)))
        {
            OutputDebugStringA("[R3D] Failed to create depth texture.\n");
            return false;
        }
        if (FAILED(m_device->CreateDepthStencilView(m_dsTex, nullptr, &m_dsv)))
        {
            OutputDebugStringA("[R3D] Failed to create DSV.\n");
            return false;
        }
    }

    // ---- Depth-stencil state (depth test on, write on) ----
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable    = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc      = D3D11_COMPARISON_LESS;
        dsd.StencilEnable  = FALSE;
        m_device->CreateDepthStencilState(&dsd, &m_dss);
    }

    // ---- Rasteriser state (solid, back-face cull) ----
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode        = D3D11_FILL_SOLID;
        rd.CullMode        = D3D11_CULL_BACK;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        m_device->CreateRasterizerState(&rd, &m_rsState);
    }

    // ---- Blend state (opaque) ----
    {
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable           = FALSE;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        m_device->CreateBlendState(&bd, &m_blendState);
    }

    // ---- Camera ----
    RebuildViewProj(kBoardW, kBoardH);

    m_initialised = true;
    OutputDebugStringA("[R3D] 3D renderer initialised OK.\n");
    return true;
}

// ---------------------------------------------------------------------------
//  OnResize — recreate depth buffer at new dimensions
// ---------------------------------------------------------------------------
void Renderer3D::OnResize(UINT newWidth, UINT newHeight)
{
    if (!m_device) return;
    if (newWidth == m_bbW && newHeight == m_bbH) return;

    m_bbW = newWidth;
    m_bbH = newHeight;

    if (m_dsv) { m_dsv->Release(); m_dsv = nullptr; }
    if (m_dsTex) { m_dsTex->Release(); m_dsTex = nullptr; }

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width            = m_bbW;
    dd.Height           = m_bbH;
    dd.MipLevels        = 1;
    dd.ArraySize        = 1;
    dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage            = D3D11_USAGE_DEFAULT;
    dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    if (SUCCEEDED(m_device->CreateTexture2D(&dd, nullptr, &m_dsTex)))
        m_device->CreateDepthStencilView(m_dsTex, nullptr, &m_dsv);
}

// ---------------------------------------------------------------------------
//  Shutdown
// ---------------------------------------------------------------------------
void Renderer3D::Shutdown()
{
    auto SafeRelease = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };

    SafeRelease(m_vs);
    SafeRelease(m_ps);
    SafeRelease(m_layout);
    SafeRelease(m_cb);
    SafeRelease(m_boxVB);
    SafeRelease(m_boxIB);
    SafeRelease(m_sphereVB);
    SafeRelease(m_sphereIB);
    SafeRelease(m_floorVB);
    SafeRelease(m_floorIB);
    SafeRelease(m_dsTex);
    SafeRelease(m_dsv);
    SafeRelease(m_dss);
    SafeRelease(m_rsState);
    SafeRelease(m_blendState);
    m_initialised = false;
}

// ---------------------------------------------------------------------------
//  CompileShaders
// ---------------------------------------------------------------------------
bool Renderer3D::CompileShaders()
{
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errBlob = nullptr;

    // --- Vertex shader ---
    hr = D3DCompile(
        kVS3D_HLSL, strlen(kVS3D_HLSL),
        "VS3D",                          // source name (for error messages)
        nullptr, nullptr,                // no defines, no includes
        "main", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG,
        0,
        &vsBlob, &errBlob);

    if (FAILED(hr))
    {
        if (errBlob)
        {
            OutputDebugStringA("[R3D] VS compile error: ");
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("\n");
            errBlob->Release();
        }
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_vs);
    if (FAILED(hr)) { vsBlob->Release(); return false; }

    // --- Input layout (must be created against the VS blob) ---
    D3D11_INPUT_ELEMENT_DESC elems[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = m_device->CreateInputLayout(
        elems, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_layout);
    vsBlob->Release();
    if (FAILED(hr))
    {
        OutputDebugStringA("[R3D] Failed to create input layout.\n");
        return false;
    }

    // --- Pixel shader ---
    hr = D3DCompile(
        kPS3D_HLSL, strlen(kPS3D_HLSL),
        "PS3D",
        nullptr, nullptr,
        "main", "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG,
        0,
        &psBlob, &errBlob);

    if (FAILED(hr))
    {
        if (errBlob)
        {
            OutputDebugStringA("[R3D] PS compile error: ");
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("\n");
            errBlob->Release();
        }
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_ps);
    psBlob->Release();
    if (FAILED(hr)) return false;

    return true;
}

// ---------------------------------------------------------------------------
//  CreateMesh
// ---------------------------------------------------------------------------
bool Renderer3D::CreateMesh(const std::vector<Vertex3D>&  verts,
                             const std::vector<uint16_t>&  indices,
                             ID3D11Buffer**                ppVB,
                             ID3D11Buffer**                ppIB,
                             UINT&                         indexCount)
{
    indexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC bd{};
    D3D11_SUBRESOURCE_DATA sd{};

    // Vertex buffer
    bd.ByteWidth = (UINT)(verts.size() * sizeof(Vertex3D));
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    sd.pSysMem   = verts.data();
    if (FAILED(m_device->CreateBuffer(&bd, &sd, ppVB))) return false;

    // Index buffer
    bd.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem   = indices.data();
    if (FAILED(m_device->CreateBuffer(&bd, &sd, ppIB))) return false;

    return true;
}

// ---------------------------------------------------------------------------
//  BuildBoxGeometry — axis-aligned box from (-0.5,-0.5,-0.5) to (0.5,0.5,0.5)
//  6 faces x 4 verts x 1 normal each = 24 vertices, 36 indices
// ---------------------------------------------------------------------------
bool Renderer3D::BuildBoxGeometry()
{
    // Each face: 4 unique vertices sharing the face's outward normal.
    // Winding: counter-clockwise when viewed from outside (D3D11 default front-face = CW,
    // but we set FrontCounterClockwise=FALSE so we use CW winding as front-face).
    // We'll define CW from outside = correct for back-face culling.

    std::vector<Vertex3D> verts;
    std::vector<uint16_t> idx;
    verts.reserve(24);
    idx.reserve(36);

    // Helper: add one face as two triangles (CW winding viewed from normal direction)
    auto addFace = [&](
        float x0, float y0, float z0,
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        float x3, float y3, float z3,
        float nx, float ny, float nz)
    {
        uint16_t base = (uint16_t)verts.size();
        verts.push_back({x0, y0, z0, nx, ny, nz});
        verts.push_back({x1, y1, z1, nx, ny, nz});
        verts.push_back({x2, y2, z2, nx, ny, nz});
        verts.push_back({x3, y3, z3, nx, ny, nz});
        // Two triangles: 0-1-2 and 0-2-3  (CW)
        idx.insert(idx.end(), {base, (uint16_t)(base+1), (uint16_t)(base+2),
                                base, (uint16_t)(base+2), (uint16_t)(base+3)});
    };

    const float H = 0.5f;

    // +Y top
    addFace(-H,H,-H,  H,H,-H,  H,H,H,  -H,H,H,   0,1,0);
    // -Y bottom
    addFace(-H,-H,H,  H,-H,H,  H,-H,-H, -H,-H,-H, 0,-1,0);
    // +X right
    addFace(H,-H,-H,  H,-H,H,  H,H,H,  H,H,-H,   1,0,0);
    // -X left
    addFace(-H,-H,H,  -H,-H,-H, -H,H,-H, -H,H,H,  -1,0,0);
    // +Z front
    addFace(-H,-H,H,  H,-H,H,  H,H,H,  -H,H,H,   0,0,1);
    // -Z back
    addFace(H,-H,-H,  -H,-H,-H, -H,H,-H, H,H,-H,  0,0,-1);

    return CreateMesh(verts, idx, &m_boxVB, &m_boxIB, m_boxIdxCount);
}

// ---------------------------------------------------------------------------
//  BuildSphereGeometry — UV sphere, radius 0.35, centred at origin
//  stacks=10, slices=14  (~280 triangles)
// ---------------------------------------------------------------------------
bool Renderer3D::BuildSphereGeometry()
{
    const float R      = 0.35f;
    const int   stacks = 10;
    const int   slices = 14;
    const float PI     = 3.14159265358979f;

    std::vector<Vertex3D> verts;
    std::vector<uint16_t> idx;

    // Build vertices: stack angle phi in [0,PI], slice angle theta in [0,2PI]
    for (int st = 0; st <= stacks; ++st)
    {
        float phi  = PI * st / (float)stacks;
        float sinP = sinf(phi);
        float cosP = cosf(phi);
        for (int sl = 0; sl <= slices; ++sl)
        {
            float theta = 2.0f * PI * sl / (float)slices;
            float nx = sinP * cosf(theta);
            float ny = cosP;
            float nz = sinP * sinf(theta);
            verts.push_back({nx * R, ny * R, nz * R, nx, ny, nz});
        }
    }

    // Build index list (quads split into two CW triangles each)
    for (int st = 0; st < stacks; ++st)
    {
        for (int sl = 0; sl < slices; ++sl)
        {
            uint16_t a = (uint16_t)(st * (slices + 1) + sl);
            uint16_t b = (uint16_t)(a + 1);
            uint16_t c = (uint16_t)(a + (slices + 1));
            uint16_t d = (uint16_t)(c + 1);
            idx.insert(idx.end(), {a, b, d,  a, d, c});
        }
    }

    return CreateMesh(verts, idx, &m_sphereVB, &m_sphereIB, m_sphereIdxCount);
}

// ---------------------------------------------------------------------------
//  BuildFloorGeometry — flat quad at Y=0 covering [0,boardW] x [0,boardH] in XZ
//  We subdivide it into a grid so normals are well-defined for the full surface.
// ---------------------------------------------------------------------------
bool Renderer3D::BuildFloorGeometry(int boardW, int boardH)
{
    std::vector<Vertex3D> verts;
    std::vector<uint16_t> idx;

    // Two triangles spanning the entire board floor, normal pointing up.
    const float W = (float)boardW;
    const float D = (float)boardH;

    verts.push_back({ 0.f, 0.f, 0.f,  0,1,0 });
    verts.push_back({ W,   0.f, 0.f,  0,1,0 });
    verts.push_back({ W,   0.f, D,    0,1,0 });
    verts.push_back({ 0.f, 0.f, D,    0,1,0 });

    idx.insert(idx.end(), {0, 1, 2,  0, 2, 3});

    return CreateMesh(verts, idx, &m_floorVB, &m_floorIB, m_floorIdxCount);
}

// ---------------------------------------------------------------------------
//  RebuildViewProj
// ---------------------------------------------------------------------------
void Renderer3D::RebuildViewProj(int boardW, int boardH)
{
    float bW = (float)boardW;
    float bH = (float)boardH;
    float cx = bW * 0.5f;
    float cz = bH * 0.5f;

    // Camera position: above-and-to-the-side, looking down at board centre.
    // Y = board height * 1.3 gives a comfortable ~50° elevation angle.
    XMVECTOR eye    = XMVectorSet(cx, bH * 1.3f, -bH * 0.85f, 1.f);
    XMVECTOR target = XMVectorSet(cx, 0.f,        cz,          1.f);
    XMVECTOR up     = XMVectorSet(0.f, 1.f,       0.f,         0.f);

    m_view = XMMatrixLookAtLH(eye, target, up);

    float fovY     = XMConvertToRadians(50.0f);
    float aspect   = (m_bbH > 0) ? (float)m_bbW / (float)m_bbH : 1.0f;
    m_proj = XMMatrixPerspectiveFovLH(fovY, aspect, 0.1f, 500.f);
}

// ---------------------------------------------------------------------------
//  DrawMesh — bind buffers, update constant buffer, issue a draw call
// ---------------------------------------------------------------------------
void Renderer3D::DrawMesh(ID3D11Buffer*   vb,
                           ID3D11Buffer*   ib,
                           UINT            indexCount,
                           const XMMATRIX& world,
                           const XMFLOAT4& baseColor)
{
    // --- Update constant buffer ---
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_ctx->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    CBPerDraw* cb = reinterpret_cast<CBPerDraw*>(mapped.pData);

    XMMATRIX wvp = XMMatrixMultiply(XMMatrixMultiply(world, m_view), m_proj);
    XMStoreFloat4x4(&cb->WorldViewProj, XMMatrixTranspose(wvp));
    XMStoreFloat4x4(&cb->World,         XMMatrixTranspose(world));
    cb->LightDir    = m_lightDir;
    cb->BaseColor   = baseColor;
    cb->AmbientColor = m_ambient;

    m_ctx->Unmap(m_cb, 0);

    // --- Bind geometry ---
    UINT stride = sizeof(Vertex3D);
    UINT offset = 0;
    m_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- Draw ---
    m_ctx->DrawIndexed(indexCount, 0, 0);
}

// ---------------------------------------------------------------------------
//  DrawScene — the main per-frame 3-D draw function
// ---------------------------------------------------------------------------
void Renderer3D::DrawScene(ID3D11RenderTargetView*   rtv,
                            const std::deque<Point>&  snakeBody,
                            const std::vector<Point>& foods,
                            int                       boardW,
                            int                       boardH)
{
    if (!m_initialised || !rtv) return;

    // --- Set render targets (RTV + depth) ---
    m_ctx->OMSetRenderTargets(1, &rtv, m_dsv);
    m_ctx->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // --- Viewport covering the full window ---
    D3D11_VIEWPORT vp{};
    vp.Width    = (float)m_bbW;
    vp.Height   = (float)m_bbH;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    m_ctx->RSSetViewports(1, &vp);

    // --- Pipeline state ---
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->IASetInputLayout(m_layout);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cb);
    m_ctx->PSSetConstantBuffers(0, 1, &m_cb);
    m_ctx->OMSetDepthStencilState(m_dss, 0);
    m_ctx->RSSetState(m_rsState);

    float blendFactor[4] = {1,1,1,1};
    m_ctx->OMSetBlendState(m_blendState, blendFactor, 0xFFFFFFFF);

    // -----------------------------------------------------------------------
    //  1. Floor
    //     Dark greenish-grey, slightly reflective look via ambient tweak
    // -----------------------------------------------------------------------
    {
        XMMATRIX world = XMMatrixIdentity();
        XMFLOAT4 color = {0.08f, 0.12f, 0.09f, 1.0f};  // very dark green
        DrawMesh(m_floorVB, m_floorIB, m_floorIdxCount, world, color);
    }

    // Draw a thin grid overlay on the floor using slightly lighter quads.
    // Rather than extra geometry, we draw thin box strips at each grid line.
    // Grid lines run along X and Z at integer cell boundaries.
    // We use flat boxes (scaleY very small) in a dark accent colour.
    {
        const float lineH   = 0.005f;   // very flat
        const float lineW   = 0.04f;    // thin line width in world units
        const float bW      = (float)boardW;
        const float bH      = (float)boardH;
        XMFLOAT4 gridColor  = {0.13f, 0.20f, 0.14f, 1.0f};

        // Horizontal grid lines (along X axis, at each integer Z)
        for (int z = 0; z <= boardH; ++z)
        {
            float zf = (float)z;
            XMMATRIX scale = XMMatrixScaling(bW, lineH, lineW);
            XMMATRIX trans = XMMatrixTranslation(bW * 0.5f, lineH * 0.5f, zf);
            DrawMesh(m_boxVB, m_boxIB, m_boxIdxCount,
                     XMMatrixMultiply(scale, trans), gridColor);
        }
        // Vertical grid lines (along Z axis, at each integer X)
        for (int x = 0; x <= boardW; ++x)
        {
            float xf = (float)x;
            XMMATRIX scale = XMMatrixScaling(lineW, lineH, bH);
            XMMATRIX trans = XMMatrixTranslation(xf, lineH * 0.5f, bH * 0.5f);
            DrawMesh(m_boxVB, m_boxIB, m_boxIdxCount,
                     XMMatrixMultiply(scale, trans), gridColor);
        }
    }

    // -----------------------------------------------------------------------
    //  2. Food spheres
    //     Each food is a sphere elevated to Y=0.5 (sits on the floor).
    //     Alternating bright colours from the same palette as the 2-D version.
    // -----------------------------------------------------------------------
    static const XMFLOAT4 kFoodColors[5] =
    {
        {1.00f, 0.22f, 0.22f, 1.f},   // red
        {1.00f, 0.57f, 0.10f, 1.f},   // orange
        {0.94f, 0.88f, 0.08f, 1.f},   // yellow
        {0.14f, 0.88f, 0.88f, 1.f},   // cyan
        {1.00f, 0.29f, 0.82f, 1.f},   // pink
    };

    for (int i = 0; i < (int)foods.size(); ++i)
    {
        const Point& fp = foods[i];
        // Centre the sphere in the grid cell: cell (c,r) spans [c, c+1] x [r, r+1]
        float cx = (float)fp.x + 0.5f;
        float cz = (float)fp.y + 0.5f;  // board y = world z

        XMMATRIX world = XMMatrixTranslation(cx, 0.5f, cz);
        DrawMesh(m_sphereVB, m_sphereIB, m_sphereIdxCount,
                 world, kFoodColors[i % 5]);
    }

    // -----------------------------------------------------------------------
    //  3. Snake segments
    //     Box height = 0.6 world units (slightly shorter than a full cell).
    //     Head = bright green, body gradient, tail slightly darker.
    // -----------------------------------------------------------------------
    const int n = (int)snakeBody.size();

    // Colour constants
    const XMFLOAT4 headColor  = {0.51f, 1.00f, 0.24f, 1.f};  // bright green
    const XMFLOAT4 neckColor  = {0.24f, 0.82f, 0.09f, 1.f};  // mid green
    const XMFLOAT4 tailColor  = {0.04f, 0.25f, 0.06f, 1.f};  // very dark green

    auto LerpColor4 = [](const XMFLOAT4& a, const XMFLOAT4& b, float t) -> XMFLOAT4
    {
        t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            1.f
        };
    };

    // Box scale: width = 0.88 (small gap between adjacent segments), height = 0.6
    const float segW = 0.88f;
    const float segH = 0.60f;
    XMMATRIX segScale = XMMatrixScaling(segW, segH, segW);

    // Draw tail-to-head (back-to-front) so the head is drawn on top
    for (int i = n - 1; i >= 0; --i)
    {
        const Point& seg = snakeBody[i];
        float cx = (float)seg.x + 0.5f;
        float cz = (float)seg.y + 0.5f;

        XMFLOAT4 color;
        if (i == 0)
        {
            color = headColor;
        }
        else
        {
            // Gradient from neck to tail
            float t = (float)i / (float)(n > 1 ? n - 1 : 1);
            color = LerpColor4(neckColor, tailColor, t);
        }

        // Translate to cell centre, Y = segH/2 so it sits on the floor
        XMMATRIX trans = XMMatrixTranslation(cx, segH * 0.5f, cz);
        XMMATRIX world = XMMatrixMultiply(segScale, trans);

        DrawMesh(m_boxVB, m_boxIB, m_boxIdxCount, world, color);
    }

    // -----------------------------------------------------------------------
    //  After 3-D drawing, unbind DSV so ImGui can render to the RTV alone
    // -----------------------------------------------------------------------
    ID3D11RenderTargetView* rtvPtr = rtv;
    m_ctx->OMSetRenderTargets(1, &rtvPtr, nullptr);
}
