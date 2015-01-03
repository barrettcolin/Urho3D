#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

varying vec2 vTexCoord;

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    vTexCoord = GetTexCoord(iTexCoord);
}

void PS()
{
    float paletteIndex = texture2D(sDiffMap, vTexCoord).a;
    // HLSL uses tex2Dlod which isn't available to fragment programs in GLES 2
    // Given that the palette texture doesn't have mip LODs, this should work anyway
    gl_FragColor = vec4(texture2D(sEmissiveMap, vec2(paletteIndex, 0)).rgb, 1);
}
