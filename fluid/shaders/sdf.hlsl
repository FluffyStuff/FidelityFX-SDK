#include "fluid.hlsli"
#include "surfacerendercommon.h"

Texture2D<float>       DepthTexture : register(t1);
Texture2D<float4>     AlbedoTexture : register(t2);
Texture2D<float4>     HeightTexture : register(t3);
Texture2D<float4>     NormalTexture : register(t4);
Texture2D<float4>  RoughnessTexture : register(t5);

SamplerState linearSampler : register(s0);

struct TileInfo
{
    uint  particleCount;
    uint  tileOffset;
    float closest;
    float farthest;
};

struct PixelIn
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

struct GBuffer
{
    float4 AoRoughnessMetallic : SV_Target0;
    float4 Albedo              : SV_Target1;
    float4 Normals             : SV_Target2;
    float  Depth               : SV_Depth;
};

float sdSphere(float3 p, float s)
{
    return length(p) - s;
}

float smin(float a, float b, float k)
{
    if (k <= 0)
        return min(a, b);

    k *= 4.0;
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * (1.0 / 4.0);
}

float opS(float d1, float d2, float k)
{
    return smin(d1, d2, k);
}

float map(TileInfo info, float3 pos)
{
    float ret = 1e20;

    for (uint i = 0; i < info.particleCount; i++)
    {
        float4 particle = ParticleBuffer[info.tileOffset + i];

        if (particle.w <= 0)
            continue;
        float o = sdSphere(pos - particle.xyz, particle.w);
        ret = opS(o, ret, particle.w * Info.SDFBlend);
    }

    return ret;
}

float3 normalAt(TileInfo info, float3 pos)
{
    float epsilon = 0.001;

    float s  = map(info, pos);
    float dx = map(info, float3(pos.x + epsilon, pos.y, pos.z)) - s;
    float dy = map(info, float3(pos.x, pos.y + epsilon, pos.z)) - s;
    float dz = map(info, float3(pos.x, pos.y, pos.z + epsilon)) - s;

    return normalize(float3(dx, dy, dz));
}

float march(TileInfo info, float3 offset, float3 dir)
{
    float closest = 1e20;
    float d = 0.01;
    
    for (int t = 0; t < 256 && d < info.farthest; t++)
    {
        float3 pos = offset + dir * d;
        float ter = map(info, pos);
        
        if (ter <= 0)
            return d + ter;
        
        if (ter < 0.001)
            return d - ter;
        
        if (ter < closest)
            closest = ter;
        
        d += ter;
    }
    
    return -closest;
}

struct TriplanarData
{
    float3 albedo;
    float3 normal;
    float  roughness;
    float  ao;
};

float3 BlendTriplanarNormal(float3 mappedNormal, float3 surfaceNormal)
{
    float3 n;
    n.xy = mappedNormal.xy + surfaceNormal.xy;
    n.z  = mappedNormal.z * surfaceNormal.z;
    return n;
}

TriplanarData GetTriplanar(float3 worldPos, float3 normal)
{
    worldPos *= Info.UVScaling;

    float2 uvX     = worldPos.zy;
    float2 uvY     = worldPos.xz;
    float2 uvZ     = worldPos.xy;
    float3 albedoX = AlbedoTexture.Sample(linearSampler, uvX).rgb;
    float3 albedoY = AlbedoTexture.Sample(linearSampler, uvY).rgb;
    float3 albedoZ = AlbedoTexture.Sample(linearSampler, uvZ).rgb;
    float  heightX = 1 - HeightTexture.Sample(linearSampler, uvX).x;
    float  heightY = 1 - HeightTexture.Sample(linearSampler, uvY).x;
    float  heightZ = 1 - HeightTexture.Sample(linearSampler, uvZ).x;
    float3 normalX = NormalTexture.Sample(linearSampler, uvX).xyz;
    float3 normalY = NormalTexture.Sample(linearSampler, uvY).xyz;
    float3 normalZ = NormalTexture.Sample(linearSampler, uvZ).xyz;
    float  roughnessX = RoughnessTexture.Sample(linearSampler, uvX).x;
    float  roughnessY = RoughnessTexture.Sample(linearSampler, uvY).x;
    float  roughnessZ = RoughnessTexture.Sample(linearSampler, uvZ).x;

    float3 blend = abs(normal.xyz);
    blend /= dot(blend, 1);
    float3 heights = float3(heightX, heightY, heightZ) + (blend * 3);

    float heightStart = max(max(heights.x, heights.y), heights.z) - Info.TriplanarBlend;
    float3 h = max(heights - heightStart.xxx, 0.xxx);
    blend = h / dot(h, 1.xxx);

    if (normal.x <  0) normalX.x *= -normalX.x;
    if (normal.y <  0) normalY.x *= -normalY.x;
    if (normal.z >= 0) normalZ.x *= -normalZ.x;

    float3 worldNormalX = BlendTriplanarNormal(normalX, normal.zyx).zyx;
    float3 worldNormalY = BlendTriplanarNormal(normalY, normal.xzy).xzy;
    float3 worldNormalZ = BlendTriplanarNormal(normalZ, normal);

    TriplanarData data;
    data.albedo =
        albedoX * blend.x +
        albedoY * blend.y +
        albedoZ * blend.z;
    data.normal = normalize(
        worldNormalX * blend.x +
        worldNormalY * blend.y +
        worldNormalZ * blend.z);
    data.roughness =
        roughnessX * blend.x +
        roughnessY * blend.y +
        roughnessZ * blend.z;
    data.ao =
        heightX * blend.x +
        heightY * blend.y +
        heightZ * blend.z;
    return data;
}

GBuffer ps_main(PixelIn input)
{
    uint2 tile = GetTileFromUV(input.texcoord);

    uint   tileOffset   = GetTileBufferOffset(tile);
    float4 tileInfoData = ParticleBuffer[tileOffset];

    TileInfo info;
    info.tileOffset    = tileOffset + 1; // First particle is the tile info
    info.particleCount = (uint)tileInfoData.x;
    info.closest       =       tileInfoData.y;
    info.farthest      =       tileInfoData.z;

    float2 xy = input.texcoord * float2(2, -2) + float2(-1, 1);
    float depth = DepthTexture.SampleLevel(linearSampler, input.texcoord, 0).x;

    float viewDepth = length(ScreenToView(xy, depth));
    if (viewDepth < info.closest) discard;

    info.farthest = min(info.farthest, viewDepth);

    float3 cameraPos = Info.SceneInfo.CameraInfo.CameraPos.xyz;
    float3 rayDir    = normalize(ScreenToWorld(xy, 1) - cameraPos);
    float3 startPos  = cameraPos + rayDir * info.closest;
    float  dist      = march(info, startPos, rayDir);

    if (dist < 0) discard;

    float3 intersection = startPos + dist * rayDir;

    float intersectionDepth = WorldToScreen(intersection).z;
    if (intersectionDepth < depth) discard;

    TriplanarData triplanar = GetTriplanar(intersection, normalAt(info, intersection));

    GBuffer buffer;

    // Tweak AO
    buffer.AoRoughnessMetallic = float4(triplanar.ao * 0.7, triplanar.roughness, 0, 1);
    buffer.Albedo              = float4(triplanar.albedo, 1);
    buffer.Normals             = float4(CompressNormals(triplanar.normal), 1);
    buffer.Depth               = intersectionDepth;

    return buffer;
}
