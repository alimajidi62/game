//  main.cpp — Win32 window + DirectX 11 + Dear ImGui bootstrap for Snake
//  Entry point: WinMain (Windows subsystem — no console window)
//
//  Build requirements (already set in .vcxproj):
//    SubSystem  : Windows
//    Additional dependencies : d3d11.lib; d3dcompiler.lib; dxgi.lib
//    Additional include dirs : imgui\

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "Game.h"

// ---------------------------------------------------------------------------
//  DX11 globals (window-lifetime objects)
// ---------------------------------------------------------------------------
static ID3D11Device*            g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext    = nullptr;
static IDXGISwapChain*          g_pSwapChain           = nullptr;
static ID3D11RenderTargetView*  g_pMainRenderTargetView= nullptr;
static UINT                     g_ResizeWidth          = 0;
static UINT                     g_ResizeHeight         = 0;

// ---------------------------------------------------------------------------
//  Helper: create / release the render target (called on resize too)
// ---------------------------------------------------------------------------
static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr,
                                             &g_pMainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_pMainRenderTargetView) { g_pMainRenderTargetView->Release();
                                   g_pMainRenderTargetView = nullptr; }
}

// ---------------------------------------------------------------------------
//  DX11 device + swap chain creation
// ---------------------------------------------------------------------------
static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevelOut;
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags,
        featureLevels, (UINT)ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevelOut,
        &g_pd3dDeviceContext);

    if (hr == DXGI_ERROR_UNSUPPORTED)   // try WARP software renderer
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createDeviceFlags,
            featureLevels, (UINT)ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &sd, &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevelOut,
            &g_pd3dDeviceContext);

    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)          { g_pSwapChain->Release();          g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext)   { g_pd3dDeviceContext->Release();   g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)          { g_pd3dDevice->Release();          g_pd3dDevice = nullptr; }
}

// ---------------------------------------------------------------------------
//  Forward-declare ImGui's Win32 message handler
// ---------------------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
//  Win32 message procedure
// ---------------------------------------------------------------------------
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_ResizeWidth  = LOWORD(lParam);
            g_ResizeHeight = HIWORD(lParam);
        }
        return 0;

    case WM_SYSCOMMAND:
        // Suppress the beep on Alt-key
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
//  WinMain — entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPSTR     /*lpCmdLine*/,
    _In_     int       /*nCmdShow*/)
{
    // ---- Register window class ----
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SnakeImGuiClass";
    RegisterClassExW(&wc);

    // ---- Create window (900 x 700) centred on primary monitor ----
    const int WIN_W = 900, WIN_H = 700;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - WIN_W) / 2;
    int posY = (screenH - WIN_H) / 2;

    HWND hwnd = CreateWindowW(
        wc.lpszClassName, L"Snake",
        WS_OVERLAPPEDWINDOW,
        posX, posY, WIN_W, WIN_H,
        nullptr, nullptr, hInstance, nullptr);

    // ---- Initialise DX11 ----
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // ---- Initialise Dear ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // keyboard navigation

    // Style — dark theme base, we override most colours in Game::Render
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding  = 4.0f;
    style.ItemSpacing    = {8.0f, 6.0f};

    // Slightly larger font for the 900-wide window
    io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // ---- Game object ----
    Game game;

    // ---- Initialise 3-D renderer and load sprite textures ----
    // Must be called after CreateDeviceD3D() and ImGui_ImplDX11_Init()
    // so the D3D11 device and swap chain are fully initialised.
    game.LoadSprites(g_pd3dDevice, g_pd3dDeviceContext, WIN_W, WIN_H);

    // ---- Main loop ----
    bool done = false;
    while (!done)
    {
        // Process Win32 messages
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Handle window resize (must happen before NewFrame)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0,
                g_ResizeWidth, g_ResizeHeight,
                DXGI_FORMAT_UNKNOWN, 0);
            // Notify 3-D renderer so its depth buffer is recreated at the new size
            game.On3DResize(g_ResizeWidth, g_ResizeHeight);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // ---- Clear back buffer first so the 3-D pass can write into it ----
        // The 3-D render pass (inside game.Render) binds g_pMainRenderTargetView
        // together with its own depth/stencil buffer, draws the scene, then
        // unbinds the DSV.  ImGui then renders its draw lists on top.
        const float clearColor[4] = { 0.04f, 0.04f, 0.08f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1,
            &g_pMainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(
            g_pMainRenderTargetView, clearColor);

        // ---- Start ImGui frame ----
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ---- Game logic ----
        game.ProcessInput();
        game.Update();

        float winW = (float)io.DisplaySize.x;
        float winH = (float)io.DisplaySize.y;
        // Pass the RTV so the 3-D render pass can bind it internally.
        game.Render(winW, winH, g_pMainRenderTargetView);

        if (game.ShouldQuit())
            done = true;

        // ---- Finalise ImGui draw lists and flush to GPU ----
        ImGui::Render();

        // Ensure ImGui renders to the correct RTV (no DSV — 3-D pass unbound it)
        g_pd3dDeviceContext->OMSetRenderTargets(1,
            &g_pMainRenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);   // vsync on
    }

    // ---- Cleanup ----
    ReleaseTextures();          // free all sprite SRVs before device teardown
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);

    return 0;
}
