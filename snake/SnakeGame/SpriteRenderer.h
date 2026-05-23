#pragma once
//  SpriteRenderer.h — Sprite set definition and DrawSprite helper.
//
//  All sprite drawing goes through ImDrawList::AddImage so blending,
//  scissoring, and draw-order are all managed by ImGui — no separate
//  D3D render pass needed.
//
//  Corner naming matches the OpenGameArt CC0 "snake_graphics" pack:
//
//    body_topright.png   — curve occupies top-right quadrant of the cell
//                          (body comes from the LEFT and exits DOWNWARD,
//                           or equivalently comes from BELOW and exits RIGHT)
//    body_topleft.png    — curve occupies top-left quadrant
//                          (body comes from RIGHT and exits DOWNWARD,
//                           or comes from BELOW and exits LEFT)
//    body_bottomright.png — curve occupies bottom-right quadrant
//                          (body comes from LEFT and exits UPWARD,
//                           or comes from ABOVE and exits RIGHT)
//    body_bottomleft.png  — curve occupies bottom-left quadrant
//                          (body comes from RIGHT and exits UPWARD,
//                           or comes from ABOVE and exits LEFT)
//
//  Tail naming: tail_right.png — the blunt/rounded end is on the RIGHT side,
//  meaning the snake body extends to the LEFT of this cell.  In other words
//  the tail segment points in the direction it faces (same convention as head).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include "imgui/imgui.h"
#include "Renderer.h"   // for CELL_PX

// ---------------------------------------------------------------------------
//  SpriteSet — all texture pointers for one skin
// ---------------------------------------------------------------------------
struct SpriteSet
{
    // Head — direction the snake is facing (the mouth side)
    ID3D11ShaderResourceView* headUp    = nullptr;
    ID3D11ShaderResourceView* headDown  = nullptr;
    ID3D11ShaderResourceView* headLeft  = nullptr;
    ID3D11ShaderResourceView* headRight = nullptr;

    // Body — straight segments
    ID3D11ShaderResourceView* bodyH = nullptr;   // horizontal
    ID3D11ShaderResourceView* bodyV = nullptr;   // vertical

    // Body — corner segments (named after the quadrant the curve fills)
    ID3D11ShaderResourceView* cornerTR = nullptr;  // top-right  (from L→D or U→R)
    ID3D11ShaderResourceView* cornerTL = nullptr;  // top-left   (from R→D or U→L)
    ID3D11ShaderResourceView* cornerBR = nullptr;  // bottom-right (from L→U or D→R)
    ID3D11ShaderResourceView* cornerBL = nullptr;  // bottom-left  (from R→U or D→L)

    // Tail — direction the tail tip points (same convention as head)
    ID3D11ShaderResourceView* tailUp    = nullptr;
    ID3D11ShaderResourceView* tailDown  = nullptr;
    ID3D11ShaderResourceView* tailLeft  = nullptr;
    ID3D11ShaderResourceView* tailRight = nullptr;

    // Food / apple
    ID3D11ShaderResourceView* food = nullptr;

    // Returns true if the minimum set of textures needed to draw the snake was loaded.
    bool IsLoaded() const
    {
        return headUp && headDown && headLeft && headRight
            && bodyH && bodyV
            && cornerTR && cornerTL && cornerBR && cornerBL
            && tailUp && tailDown && tailLeft && tailRight;
    }
};

// ---------------------------------------------------------------------------
//  DrawSprite — draw one SRV at board cell (cellX, cellY), filling CELL_PX²
// ---------------------------------------------------------------------------
//  ox / oy  : board pixel origin (top-left corner of cell 0,0)
//  cellX/Y  : board grid coordinates
//  srv      : the texture to draw (nullptr is a no-op)
//  cellPx   : pixels per cell (from Renderer::CELL_PX)
//  tint     : optional RGBA tint (default = opaque white = no tint)
inline void DrawSprite(ImDrawList*               dl,
                       float                     ox,
                       float                     oy,
                       int                       cellX,
                       int                       cellY,
                       ID3D11ShaderResourceView* srv,
                       float                     cellPx,
                       ImU32                     tint = IM_COL32_WHITE)
{
    if (!srv || !dl) return;

    ImVec2 pMin{ ox + cellX * cellPx,          oy + cellY * cellPx          };
    ImVec2 pMax{ ox + cellX * cellPx + cellPx, oy + cellY * cellPx + cellPx };

    dl->AddImage(
        reinterpret_cast<ImTextureID>(srv),
        pMin, pMax,
        { 0.f, 0.f }, { 1.f, 1.f },
        tint);
}
