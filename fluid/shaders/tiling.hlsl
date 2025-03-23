#include "fluid.hlsli"

Buffer<float4> InputParticleBuffer : register(t0);

void CreateList(uint2 tile)
{
    uint tileOffset = GetTileBufferOffset(tile);

    float2 xyTL = tile       / float2(Info.TilesX, Info.TilesY) * 2 - 1;
    float2 xyBR = (tile + 1) / float2(Info.TilesX, Info.TilesY) * 2 - 1;

    // swizzle into correct positions
    float tmp = xyTL.y;
    xyTL.y = -xyBR.y;
    xyBR.y = -tmp;

    float3 viewPoints[4];
    viewPoints[0] = normalize(ScreenToView(float2(xyTL.x, xyTL.y), 1)); // TL
    viewPoints[1] = normalize(ScreenToView(float2(xyTL.x, xyBR.y), 1)); // BL
    viewPoints[2] = normalize(ScreenToView(float2(xyBR.x, xyBR.y), 1)); // BR
    viewPoints[3] = normalize(ScreenToView(float2(xyBR.x, xyTL.y), 1)); // TR

    float3 viewNormals[4];
    viewNormals[0] = normalize(cross(viewPoints[3], viewPoints[2])); // right
    viewNormals[1] = normalize(cross(viewPoints[1], viewPoints[0])); // left
    viewNormals[2] = normalize(cross(viewPoints[0], viewPoints[3])); // top
    viewNormals[3] = normalize(cross(viewPoints[2], viewPoints[1])); // bottom

    float closest = 1e20;
    float farthest = -1e20;

    uint outIndex = 1;
    for (int i = 0; i < Info.ParticleCount && outIndex < TILE_MAX_PARTICLES; i++)
    {
        float4 particle = InputParticleBuffer.Load(i);

        // Roughly account for smoothing
        float overestimate = Info.BiggestRadius * pow(Info.SDFBlend, 0.75) * 4;
        float radius = particle.w + overestimate;

        if (radius <= 0)
            continue;

        float3 viewParticle = WorldToView(particle.xyz);

        if (-viewParticle.z < radius)
            continue;

        if (dot(viewNormals[0], viewParticle) > radius ||
            dot(viewNormals[1], viewParticle) > radius ||
            dot(viewNormals[2], viewParticle) > radius ||
            dot(viewNormals[3], viewParticle) > radius)
            continue;

        float dist = length(viewParticle);
        if (dist - radius < closest)  closest  = dist - radius;
        if (dist + radius > farthest) farthest = dist + radius;

        ParticleBuffer[tileOffset + outIndex] = particle;
        outIndex++;
    }

    // Store tile info in the first particle
    ParticleBuffer[tileOffset] = float4(outIndex - 1, closest, farthest, 0);
}

[numthreads(TILING_THREAD_X, TILING_THREAD_Y, 1)]
void TilingCS(uint3 DTid : SV_DispatchThreadID)
{
    uint2 tile = DTid.xy;
    if (tile.x >= Info.TilesX || tile.y >= Info.TilesY)
        return;

    CreateList(tile);
}
