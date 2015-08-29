#include "Uniforms.hlsl"
#include "Samplers.hlsl"
#include "Transform.hlsl"

void VS(float4 iPos : POSITION,
    float2 iTexCoord : TEXCOORD0,
    out float2 oTexCoord : TEXCOORD0,
    out float4 oPos : POSITION)
{
    float4x3 modelMatrix = iModelMatrix;
    float3 worldPos = GetWorldPos(modelMatrix);
    oPos = GetClipPos(worldPos);
    oTexCoord = GetTexCoord(iTexCoord);
}

void PS(float2 iTexCoord : TEXCOORD0,
    out float4 oColor : COLOR0)
{
    float paletteIndex = tex2D(sDiffMap, iTexCoord).a;
    // Use tex2Dlod to ensure the palette is always sampled at LOD 0
    float3 color = tex2Dlod(sEmissiveMap, float4(paletteIndex, 0, 0, 0)).rgb;
    oColor = float4(color, 1.0);
}
