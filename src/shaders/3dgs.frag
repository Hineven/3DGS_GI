#include "3dgs_inc.hlsl"

struct DrawActiveGaussians_FSInput
{
    float4 UVWActiveID : TEXCOORD0;
};

float Evaluate2DNormalizedGaussian (float2 P) {
    float NormalizationFactor = 1 / (2 * M_PI);
    return NormalizationFactor * exp(-dot(P, P) / 2);
}

float4 DrawActiveGaussians (DrawActiveGaussians_FSInput Input) : SV_Target {
    int ActiveListIndex = (int)Input.UVWActiveID.w;
    float2 UV    = Input.UVWActiveID.xy;
    float  Alpha = Input.UVWActiveID.z * Evaluate2DNormalizedGaussian(UV);
    float3 Color = g_RWActiveGaussianColorBuffer[ActiveListIndex];
    // ...
    return float4(Color, Alpha);
}

float4 TonemapAndDraw (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float4 Color = g_GColorTexture.Sample(g_LinearClampSampler, UV);
    // Just simply do a gamma correction
    Color.rgb = pow (Color.rgb, 1.0 / 2.2);
    return Color;
}