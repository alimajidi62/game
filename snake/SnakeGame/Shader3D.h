#pragma once
//  Shader3D.h — Embedded HLSL shaders for the Snake 3D render pass.
//
//  Both shaders are stored as string literals and compiled at runtime via
//  D3DCompile so no separate .hlsl files or FXC pre-compilation step is needed.
//
//  Vertex layout expected by the VS:
//    float3 Position  : POSITION
//    float3 Normal    : NORMAL
//
//  Constant buffer slot b0 (updated per draw call):
//    float4x4 WorldViewProj  — combined transform for clip space
//    float4x4 World          — world matrix for normal transform
//    float4   LightDir       — normalised direction TO the light (world space)
//    float4   BaseColor      — RGBA base tint for this draw call
//    float4   AmbientColor   — ambient light colour * intensity

// --------------------------------------------------------------------------
//  Vertex shader source
// --------------------------------------------------------------------------
static const char* kVS3D_HLSL = R"HLSL(
cbuffer PerDraw : register(b0)
{
    float4x4 WorldViewProj;
    float4x4 World;
    float4   LightDir;       // direction TO the light, world space (normalised)
    float4   BaseColor;      // rgb = colour, a = opacity
    float4   AmbientColor;   // rgb = ambient, a unused
};

struct VS_Input
{
    float3 Pos    : POSITION;
    float3 Normal : NORMAL;
};

struct VS_Output
{
    float4 ClipPos : SV_Position;
    float3 WorldN  : TEXCOORD0;   // world-space normal (passed to PS)
};

VS_Output main(VS_Input vin)
{
    VS_Output vout;
    vout.ClipPos = mul(float4(vin.Pos, 1.0f), WorldViewProj);
    // Transform normal by the inverse-transpose of World.
    // For uniform-scale transforms (scale+rotation) this is just World's 3x3.
    vout.WorldN  = normalize(mul(float4(vin.Normal, 0.0f), World).xyz);
    return vout;
}
)HLSL";

// --------------------------------------------------------------------------
//  Pixel shader source
// --------------------------------------------------------------------------
static const char* kPS3D_HLSL = R"HLSL(
cbuffer PerDraw : register(b0)
{
    float4x4 WorldViewProj;
    float4x4 World;
    float4   LightDir;
    float4   BaseColor;
    float4   AmbientColor;
};

struct PS_Input
{
    float4 ClipPos : SV_Position;
    float3 WorldN  : TEXCOORD0;
};

float4 main(PS_Input pin) : SV_Target
{
    float3 N = normalize(pin.WorldN);
    float3 L = normalize(LightDir.xyz);

    // Lambertian diffuse — clamp to [0,1]
    float diffuse = saturate(dot(N, L));

    // Combine: ambient + diffuse * base colour
    float3 colour = AmbientColor.rgb + diffuse * BaseColor.rgb;

    // Gamma-approximate tonemapping (simple multiply won't over-saturate)
    colour = saturate(colour);

    return float4(colour, BaseColor.a);
}
)HLSL";
