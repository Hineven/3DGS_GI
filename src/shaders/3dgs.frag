#include "3dgs_inc.hlsl"

float4 TonemapAndDraw (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float4 Color = g_GColorTexture.Sample(g_LinearClampSampler, UV);
    // Just simply do a gamma correction
    Color.rgb = pow (Color.rgb, 1.0 / 2.2);
    return Color;
}