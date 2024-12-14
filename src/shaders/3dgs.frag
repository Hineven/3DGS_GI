#include "3dgs.hlsl"
#include "radiometry.hlsl"

struct DrawActiveGaussians_FSInput
{
    float4 Position : SV_Position;
#ifdef BITPACK_VERTEX_ATTRIBUTES
    float2 UV : TEXCOORD0;
    uint2  RGBA_RNNN : TEXCOORD1;
#else
    float4 UVWR : TEXCOORD0;
    float4 GBRN : TEXCOORD1;
    float2 NN   : TEXCOORD2;
#endif
};

float Evaluate2DUnnormalizedGaussian (float2 P) {
    // float NormalizationFactor = 1 / (2 * M_PI);
    return exp(-dot(P, P) / 2);
}

struct GBufferOutput {
    float4 AlbedoAlpha    : SV_Target0;
#ifdef OUTPUT_PBR_G_BUFFER
    float4 Roughness      : SV_Target1;
    float4 Depth          : SV_Target2;
#ifndef RECONSTRUCT_NORMALS_FROM_DEPTH
    float4 Normal         : SV_Target3;
#endif
#endif
};

GBufferOutput DrawActiveGaussians (DrawActiveGaussians_FSInput Input) {
#ifdef BITPACK_VERTEX_ATTRIBUTES
    float2 UV     = Input.UV;
    float4 RGBA   = UnpackRGBA8(Input.RGBA_RNNN.x);
#ifdef OUTPUT_PBR_G_BUFFER
    float4 Roughness_NormalU = UnpackRGBA8(Input.RGBA_RNNN.y);
    float  Roughness = Roughness_NormalU.x;
    float3 NormalU   = Roughness_NormalU.yzw;
#endif
#else
    float2 UV     = Input.UVWR.xy;
    float4 RGBA   = float4(Input.UVWR.w, Input.GBRN.xy, Input.UVWR.z);
#ifdef OUTPUT_PBR_G_BUFFER
    float  Roughness = Input.GBRN.z;
    float3 NormalU   = float3(Input.GBRN.w, Input.NN);
#endif
#endif
    float  Alpha  = RGBA.w *  Evaluate2DUnnormalizedGaussian(UV);
    
    float3 Albedo = saturate(RGBA.xyz);
    CameraDescription C = GetCameraDescription();
    float  LinearDepth  = Input.Position.z * C.FarPlane;
    GBufferOutput Result = (GBufferOutput)0;
    Result.AlbedoAlpha    = float4(Albedo, Alpha);
#ifdef OUTPUT_PBR_G_BUFFER
    Result.Roughness      = float4(Roughness,   0, 0, Alpha);
    // Clip the alpha values for calculating the depth helps smooth the transition between
    // gaussians (cause the gaussians are not fully drawn when rasterizing).
    float DepthAlphaClipValue = UB.DepthAlphaClipValue;
    float DepthMultiplier = min(DepthAlphaClipValue + Alpha, 1.f);
    Result.Depth          = float4(
        LinearDepth, 1, 0, DepthMultiplier);
    // Result.Roughness = Roughness;
    // Result.Depth = float2(LinearDepth, 1);
#ifndef RECONSTRUCT_NORMALS_FROM_DEPTH
    Result.Normal         = float4(NormalU, Alpha);
#endif
#endif
    return Result;
}

float3 ACESToneMapping(float3 color, float Exposure)
{
	float A = 2.51f;
	float B = 0.03f;
	float C = 2.43f;
	float D = 0.59f;
	float E = 0.14f;
	color *= Exposure;
	return (color * (A * color + B)) / (color * (C * color + D) + E);
}

float4 TonemapAndDraw (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float4 Color = g_Radiance.Sample(g_LinearClampSampler, UV);
    // Color.rgb = ACESToneMapping(Color.rgb, UB.TonemapExposure);
    Color.rgb = RadianceToColor(Color.rgb * UB.TonemapExposure);
    return Color;
}

struct Debug_VisualizeRays_FSInput {
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

float4 Debug_VisualizeRays (
    Debug_VisualizeRays_FSInput Input
) : SV_Target0 {
    return Input.Color;
}