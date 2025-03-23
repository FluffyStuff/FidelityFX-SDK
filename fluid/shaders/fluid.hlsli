#pragma once

#ifdef __cplusplus
#include <shaders/shadercommon.h>
#else
#include "shadercommon.h"
#endif

#define TILES_MAX_X         128
#define TILES_MAX_Y         128
#define TILE_MAX_PARTICLES   64
#define MAX_PARTICLES      1024

#define TILING_THREAD_X       8
#define TILING_THREAD_Y       8

struct GPUParticle
{
#if __cplusplus
    Vec4 posSize;
#else
    float3 position;
    float  radius;
#endif
};

struct FluidInfo
{
    SceneInformation SceneInfo;

    int              ParticleCount;
    int              TilesX;
    int              TilesY;
    float            BiggestRadius;

    float            SDFBlend;
    float            TriplanarBlend;
    float            UVScaling;
    float            Padding;
};

#ifndef __cplusplus
cbuffer CBFluidInformation : register(b0)
{
    FluidInfo Info;
};

RWBuffer<float4> ParticleBuffer : register(u0);

uint GetTileBufferOffset(uint2 tile)
{
    return (tile.y * Info.TilesX + tile.x) * TILE_MAX_PARTICLES;
}

uint2 GetTileFromUV(float2 uv)
{
    return uint2(uv * float2(Info.TilesX, Info.TilesY));
}

float3 ScreenToView(float2 xy, float depth)
{
    float4 view = mul(Info.SceneInfo.CameraInfo.InvProjectionMatrix, float4(xy, depth, 1));
    return view.xyz / view.w;
}

float3 ScreenToWorld(float2 xy, float depth)
{
    float4 world = mul(Info.SceneInfo.CameraInfo.InvViewProjectionMatrix, float4(xy, depth, 1));
    return world.xyz / world.w;
}

float3 WorldToView(float3 worldPosition)
{
    float4 view = mul(Info.SceneInfo.CameraInfo.ViewMatrix, float4(worldPosition, 1));
    return view.xyz /= view.w;
}

float3 WorldToScreen(float3 worldPosition)
{
    float4 screen = mul(Info.SceneInfo.CameraInfo.ViewProjectionMatrix, float4(worldPosition, 1));
    return screen.xyz /= screen.w;
}
#endif
