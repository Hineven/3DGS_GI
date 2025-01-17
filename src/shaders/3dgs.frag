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

// Simple FXAA for figure visuals, Modified from https://github.com/mattdesl/glsl-fxaa/blob/master/fxaa.glsl
// This should be performed on RGB color buffer. Anyway i do that on the radiance buffer.
#ifndef FXAA_REDUCE_MIN
	#define FXAA_REDUCE_MIN   (1.0 / 128.0)
#endif
#ifndef FXAA_REDUCE_MUL
	#define FXAA_REDUCE_MUL   (1.0 / 8.0)
#endif
#ifndef FXAA_SPAN_MAX
	#define FXAA_SPAN_MAX     8.0
#endif

//optimized version for mobile, where dependent 
//texture reads can be a bottleneck
float4 fxaa(Texture2D tex, float2 fragCoord, float2 resolution,
			float2 v_rgbNW, float2 v_rgbNE, 
			float2 v_rgbSW, float2 v_rgbSE, 
			float2 v_rgbM) {
	float4 color;
	float2 inverseVP = float2(1.0 / resolution.x, 1.0 / resolution.y);
	float3 rgbNW = tex.SampleLevel(g_LinearClampSampler, v_rgbNW, 0).xyz;
	float3 rgbNE = tex.SampleLevel(g_LinearClampSampler, v_rgbNE, 0).xyz;
	float3 rgbSW = tex.SampleLevel(g_LinearClampSampler, v_rgbSW, 0).xyz;
	float3 rgbSE = tex.SampleLevel(g_LinearClampSampler, v_rgbSE, 0).xyz;
	float4 texColor = tex.SampleLevel(g_LinearClampSampler, v_rgbM, 0);
	float3 rgbM  = texColor.xyz;
	float3 luma = float3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	float2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
	
	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
						  (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
			  max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
			  dir * rcpDirMin)) * inverseVP;
	
	float3 rgbA = 0.5 * (
		tex.SampleLevel(g_LinearClampSampler, fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5), 0).xyz +
		tex.SampleLevel(g_LinearClampSampler, fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5), 0).xyz);
	float3 rgbB = rgbA * 0.5 + 0.25 * (
		tex.SampleLevel(g_LinearClampSampler, fragCoord * inverseVP + dir * -0.5, 0).xyz +
		tex.SampleLevel(g_LinearClampSampler, fragCoord * inverseVP + dir * 0.5, 0).xyz);

	float lumaB = dot(rgbB, luma);
	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		color = float4(rgbA, texColor.a);
	else
		color = float4(rgbB, texColor.a);
	return color;
}

float4 AntiAliasing (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    float2 Resolution = UB.ScreenDimensions;
    float2 InvResolution = 1.0 / UB.ScreenDimensions;
    float2 v_rgbNW = UV + float2(-1.0, -1.0) * InvResolution;
    float2 v_rgbNE = UV + float2(1.0, -1.0) * InvResolution;
    float2 v_rgbSW = UV + float2(-1.0, 1.0) * InvResolution;
    float2 v_rgbSE = UV + float2(1.0, 1.0) * InvResolution;
    float2 v_rgbM  = UV;
    float4 AA = fxaa(g_MappedRGBA, InPosition.xy, Resolution, v_rgbNW, v_rgbNE, v_rgbSW, v_rgbSE, v_rgbM);
    if(UB.EnableAA) return AA;
    return g_MappedRGBA.SampleLevel(g_LinearClampSampler, UV, 0);
}

float4 DrawToBackBuffer (float4 InPosition : SV_Position) : SV_Target {
    float2 UV = InPosition.xy / UB.ScreenDimensions;
    return g_FinalRGBA.SampleLevel(g_LinearClampSampler, UV, 0);
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