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

float Evaluate2DGaussian (float2 P) {
    float NormalizationFactor = 1 / (2 * M_PI);
    return NormalizationFactor * exp(-dot(P, P) / 2);
}

struct GBufferOutput {
    float4 AlbedoAlpha    : SV_Target0;
#ifndef CARD_SHADERS
#ifdef OUTPUT_PBR_G_BUFFER
    float4 Roughness      : SV_Target1;
    float4 Depth          : SV_Target2;
#ifndef RECONSTRUCT_NORMALS_FROM_DEPTH
    float4 Normal         : SV_Target3;
#endif
#endif
#else // CARD_SHADERS
    float4 Depth          : SV_Target1;
    float4 Normal         : SV_Target2;
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
    float  LinearDepth  = ZDepthToLinear(C, Input.Position.z);
    GBufferOutput Result = (GBufferOutput)0;
    Result.AlbedoAlpha    = float4(Albedo, Alpha);
#ifdef OUTPUT_PBR_G_BUFFER
#ifndef CARD_SHADERS // Cards have no material properties.
    Result.Roughness      = float4(Roughness,   0, 0, Alpha);
#endif
    // Clip the alpha values for calculating the depth helps smooth the transition between
    // gaussians (cause the gaussians are not fully drawn when rasterizing).
    float DepthAlphaClipValue = UB.DepthAlphaClipValue;
    float DepthMultiplier = min(DepthAlphaClipValue + Alpha, 1.f);
    Result.Depth          = float4(
        LinearDepth, 1, 0, DepthMultiplier);
    // Cards have normals directly rendered.
#if !defined(RECONSTRUCT_NORMALS_FROM_DEPTH) || defined(CARD_SHADERS)
    Result.Normal         = float4(NormalU, Alpha);
#endif
#endif
    return Result;
}


struct DrawRegulareMeshes_PSInput {
    float4 Position : SV_POSITION;
    // float2 UV       : TEXCOORD0;
    float3 Normal   : TEXCOORD0;
    float4 AlbedoRoughess : COLOR0;
    float3 Emission : COLOR1;
};

struct GBufferOutput_RegularMesh {
    float4 AlbedoAlpha    : SV_Target0;
    float4 EmissionAlpha  : SV_Target1;
    float4 Roughness      : SV_Target2;
    float4 Normal         : SV_Target3;
};

GBufferOutput_RegularMesh DrawRegularMeshes (DrawRegulareMeshes_PSInput Input) {
    CameraDescription C = GetCameraDescription();
    GBufferOutput_RegularMesh Result = (GBufferOutput_RegularMesh)0;
    // We do not write to the alpha channel of the albedo texture.
    // The alpha value of gaussians is kept by this texture.
    Result.AlbedoAlpha    = float4(Input.AlbedoRoughess.xyz, 0);
    Result.EmissionAlpha  = float4(Input.Emission, 1);
    Result.Normal         = float4(normalize(Input.Normal) * 0.5f + 0.5f, 1.f);
    Result.Roughness      = float4(Input.AlbedoRoughess.w, 0, 0, 1);
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
	// Debugging
	if(UB.DebugMode == 1) {
		Color.rgb = ColorToRadiance(g_GColorTexture.Sample(g_LinearClampSampler, UV).rgb);
    } else if(UB.DebugMode == 2) {
        Color.rgb = g_GMaterialTexture.Sample(g_LinearClampSampler, UV).rrr;
    } else if(UB.DebugMode == 3) {
        Color.rgb = g_GNormalTexture.Sample(g_LinearClampSampler, UV).xyz;
    } else if(UB.DebugMode == 4) {
		float Depth = g_GFilteredDepthTexture.Sample(g_LinearClampSampler, UV).r * 0.2f;
        Color = float4(Depth.xxx, 1);
    } else if(UB.DebugMode == 5) {
        float Alpha = g_GColorTexture.Sample(g_LinearClampSampler, UV).a;
        Color = float4(Alpha.xxx, 1);
    } else if(UB.DebugMode == 6) {
		float3 VColor = g_DebugTexture.Sample(g_LinearClampSampler, UV).rgb;
		Color = float4(VColor, 1);
	}
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

struct Debug_SSRC_VisualizeProbeUpdateRays_FSInput {
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

float4 Debug_SSRC_VisualizeProbeUpdateRays (
    in Debug_SSRC_VisualizeProbeUpdateRays_FSInput Input
) : SV_Target {
    if(Input.Color.w == 0) discard;
    return Input.Color;
}