#include "3dgs_inc.hlsl"

struct DrawActiveGaussians_FSInput
{
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
    float4 Position : SV_Position;
};

float Evaluate2DNormalizedGaussian (float2 P) {
    float NormalizationFactor = 1 / (2 * M_PI);
    return NormalizationFactor * exp(-dot(P, P) / 2);
}

float4 DrawActiveGaussians (DrawActiveGaussians_FSInput Input) : SV_Target {
    float2 UV     =   Input.UVWR.xy;
    float  Alpha  = Input.UVWR.z * Evaluate2DNormalizedGaussian(UV);
    float3 Albedo = float3(Input.UVWR.w, Input.GBMR.xy);
    float  LinearDepth  = Input.Position.z;
    return float4(Albedo, Alpha);
}

float4 TonemapAndDraw (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float4 Color = g_GColorTexture.Sample(g_LinearClampSampler, UV);
    // Just simply do a gamma correction
    Color.rgb = pow (Color.rgb, 1.0 / 2.2);
    return Color;
}