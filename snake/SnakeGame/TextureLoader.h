#pragma once
//  TextureLoader.h — Load PNG files into D3D11 shader resource views via stb_image.
//
//  USAGE — in exactly ONE .cpp file, before including this header:
//    #define TEXTURE_LOADER_IMPLEMENTATION
//    #include "TextureLoader.h"
//
//  In all other translation units just:
//    #include "TextureLoader.h"
//
//  API:
//    ID3D11ShaderResourceView* LoadTextureFromFile(device, "path/to/image.png");
//    void ReleaseTextures();   // call once at shutdown before device teardown

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
//  Public declarations (visible to all TUs)
// ---------------------------------------------------------------------------

/// Load a PNG from disk.  Returns nullptr on failure (missing file,
/// decode error, or D3D11 error).  The SRV is owned by this module
/// and released by ReleaseTextures().
ID3D11ShaderResourceView* LoadTextureFromFile(
    ID3D11Device* device, const char* path);

/// Release every SRV created by LoadTextureFromFile.
/// Call once at shutdown, BEFORE releasing the D3D11 device.
void ReleaseTextures();

// ---------------------------------------------------------------------------
//  Implementation — compiled only in the TU that defines
//  TEXTURE_LOADER_IMPLEMENTATION before including this header.
// ---------------------------------------------------------------------------
#ifdef TEXTURE_LOADER_IMPLEMENTATION

// stb_image — public-domain single-header image decoder.
// Defining STB_IMAGE_IMPLEMENTATION here compiles the full implementation
// exactly once (in Game.cpp, which is the only TU that sets the macro).
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4244)  // possible loss of data (int <-> size_t in stb)
#pragma warning(disable: 4100)  // unreferenced formal parameter
#pragma warning(disable: 4505)  // unreferenced local function removed
#include "imgui/stb_image.h"
#pragma warning(pop)

static std::vector<ID3D11ShaderResourceView*> g_loadedTextures;

ID3D11ShaderResourceView* LoadTextureFromFile(
    ID3D11Device* device, const char* path)
{
    if (!device || !path) return nullptr;

    // ---- Decode PNG to raw RGBA pixels with stb_image ----------------------
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels)
    {
        OutputDebugStringA("[TextureLoader] stbi_load failed: ");
        OutputDebugStringA(path);
        OutputDebugStringA("\n");
        return nullptr;
    }

    // ---- Upload to a D3D11 immutable Texture2D ------------------------------
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = (UINT)width;
    desc.Height           = (UINT)height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem     = pixels;
    initData.SysMemPitch = (UINT)(width * 4);  // 4 bytes per RGBA pixel

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &tex);
    stbi_image_free(pixels);   // GPU copy done — free CPU memory immediately

    if (FAILED(hr) || !tex)
    {
        OutputDebugStringA("[TextureLoader] CreateTexture2D failed: ");
        OutputDebugStringA(path);
        OutputDebugStringA("\n");
        return nullptr;
    }

    // ---- Create shader resource view ----------------------------------------
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, &srvDesc, &srv);
    tex->Release();   // SRV holds its own reference; release our Texture2D ref

    if (FAILED(hr) || !srv)
    {
        OutputDebugStringA("[TextureLoader] CreateShaderResourceView failed: ");
        OutputDebugStringA(path);
        OutputDebugStringA("\n");
        return nullptr;
    }

    g_loadedTextures.push_back(srv);
    return srv;
}

void ReleaseTextures()
{
    for (ID3D11ShaderResourceView* srv : g_loadedTextures)
        if (srv) srv->Release();
    g_loadedTextures.clear();
}

#endif // TEXTURE_LOADER_IMPLEMENTATION
