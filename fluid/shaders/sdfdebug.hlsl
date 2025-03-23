#include "fluid.hlsli"

struct PixelIn
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

float3 blend(float v, float minV, float maxV, float3 colMin, float3 colMax)
{
    return lerp(colMin, colMax, saturate((v - minV) / (maxV - minV)));
}

float4 ps_main(PixelIn input) : SV_Target0
{
    uint2 tile = GetTileFromUV(input.texcoord);

    uint    tileOffset   = GetTileBufferOffset(tile);
    float4  tileInfoData = ParticleBuffer[tileOffset];
    uint   particleCount = (uint)tileInfoData.x;

    if (particleCount == 0) discard;

    float alpha    = 0.5;
    float strength = particleCount / float(TILE_MAX_PARTICLES - 1);

    float3 color = float3(0, 0, 0.5);

    color = blend(strength, 0.00, 0.25, color, float3(0, 1, 0));
    color = blend(strength, 0.25, 0.50, color, float3(1, 1, 0));
    color = blend(strength, 0.50, 0.75, color, float3(1, 0, 0));
    color = blend(strength, 0.75, 1.00, color, float3(1, 1, 1));

    // Show pure black if the tile is saturated
    if (strength >= 1)
    {
        color = 0;
        alpha = 1;
    }

    return float4(color, alpha);
}
