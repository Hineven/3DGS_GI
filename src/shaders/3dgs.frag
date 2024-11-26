#include "3dgs.hlsl"

struct DrawActiveGaussians_FSInput
{
    float4 Position : SV_Position;
#ifdef BITPACK_VERTEX_ATTRIBUTES
    float4 UV_RGBA_MRXX : TEXCOORD0;
#else
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
#endif
};

float Evaluate2DUnnormalizedGaussian (float2 P) {
    // float NormalizationFactor = 1 / (2 * M_PI);
    return exp(-dot(P, P) / 2);
}

struct GBufferOutput {
    float4 AlbedoAlpha : SV_Target0;
    float4 Normal      : SV_Target1;
    float4 Momentum    : SV_Target2;
};

GBufferOutput DrawActiveGaussians (DrawActiveGaussians_FSInput Input) {
#ifdef BITPACK_VERTEX_ATTRIBUTES
    float2 UV     = Input.UV_RGBA_MRXX.xy;
    float4 RGBA   = UnpackRGBA8(asuint(Input.UV_RGBA_MRXX.z));
    float2 MR     = UnpackRGBA8(asuint(Input.UV_RGBA_MRXX.w)).xy;
#else
    float2 UV     = Input.UVWR.xy;
    float4 RGBA   = float4(Input.UVWR.w, Input.GBMR.xy, Input.UVWR.z);
    float2 MR     = Input.GBMR.zw;
#endif
    float  Alpha  = RGBA.w *  Evaluate2DUnnormalizedGaussian(UV);
    float3 Albedo = saturate(RGBA.xyz);
    float  LinearDepth  = Input.Position.z * GetCameraDescription().FarPlane;
    GBufferOutput Result = (GBufferOutput)0;
    Result.AlbedoAlpha = float4(Albedo, Alpha);
    Result.Normal      = float4(0, 0, 1, Alpha);
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
    Color.rgb = RadianceToColor(Color.rgb);
    // float Depth = g_GMomentumTexture.Sample(g_LinearClampSampler, UV).x;
    // float Momentum = g_GMomentumTexture.Sample(g_LinearClampSampler, UV).y;
    // float NormVariance = sqrt(max(Momentum - Depth * Depth, 0)) / max(Depth, 1e-6f);
    // Color = float4(RadianceToColor(float3(Depth, NormVariance, 0) * 0.02), 1);
    // float Alpha = g_GColorTexture.Sample(g_LinearClampSampler, UV).a;
    // Color = float4(Alpha.xxx, 1);
    return Color;
}