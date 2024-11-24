#include "3dgs.hlsl"

struct DrawActiveGaussians_FSInput
{
    float4 Position : SV_Position;
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
};

float Evaluate2DUnnormalizedGaussian (float2 P) {
    // float NormalizationFactor = 1 / (2 * M_PI);
    return exp(-dot(P, P) / 2);
}

struct GBufferOutput {
    float4 AlbedoAlpha : SV_Target0;
    float4 Momentum    : SV_Target1;
};

GBufferOutput DrawActiveGaussians (DrawActiveGaussians_FSInput Input) {
    float2 UV     = Input.UVWR.xy;
    float  Alpha  = Input.UVWR.z *  Evaluate2DUnnormalizedGaussian(UV);
    float3 Albedo = saturate(float3(Input.UVWR.w, Input.GBMR.xy));
    float  LinearDepth  = Input.Position.z * GetCameraDescription().FarPlane;
    GBufferOutput Result = (GBufferOutput)0;
    Result.AlbedoAlpha = float4(Albedo, Alpha);
    Result.Momentum    = float4(LinearDepth, LinearDepth * LinearDepth, 0, Alpha);
    return Result;
}

float4 TonemapAndDraw (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float4 Color = g_Radiance.Sample(g_LinearClampSampler, UV);
    // Just simply do a gamma correction
    Color.rgb = RadianceToColor(Color.rgb);
    // FIXME
    Color = float4(g_GColorTexture.Sample(g_LinearClampSampler, UV).rgb, Color.a);
    float Depth = g_GMomentumTexture.Sample(g_LinearClampSampler, UV).x / 100;
    Color = float4(RadianceToColor(Depth.xxx), 1);
    // float Alpha = g_GColorTexture.Sample(g_LinearClampSampler, UV).a;
    // Color = float4(Alpha.xxx, 1);
    return Color;
}