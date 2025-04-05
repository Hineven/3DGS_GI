#ifndef MATERIAL_HLSL
#define MATERIAL_HLSL
#include "math_constants.hlsl"
#include "material_evaluation.hlsl"
#include "brdf_lut/material_sampling.hlsl"
#include "transforms.hlsl"
struct Material {
    float3 Normal;
    float3 Albedo;
    float  Alpha;
    float  F0;
    float  Roughness;
};

Material MakeMaterial (float3 Normal, float3 Albedo, float Roughess = 0.5f) {
    Material Result;
    Result.Normal = Normal;
    Result.Albedo = Albedo;
    Result.F0 = 0.04f; // Dielectric constant
    Result.Roughness =  Roughess;
    return Result;
}


float3 EvaluateLambertian (float3 Albedo) {
    return Albedo / PI;
}

float3 EvaluateBSDFDotNL (Material M, float3 LightDirection, float3 ViewDirection) {
    // Simple Lambertian BRDF
    float3 Diffuse = EvaluateLambertian(M.Albedo);
    float  RoughnessAlpha = M.Roughness * M.Roughness;
    float3 F;
    float3 HalfVector = normalize(LightDirection + ViewDirection);
    float  DotHV = dot(HalfVector, ViewDirection);
    float  DotNH = dot(HalfVector, M.Normal);
    float  DotNL = dot(M.Normal, LightDirection);
    float  DotNV = dot(M.Normal, ViewDirection);
    float3 Specular = evaluateGGX(
        RoughnessAlpha, RoughnessAlpha * RoughnessAlpha,
        M.F0, DotHV, DotNH, DotNL, DotNV, F);
    return (Specular + Diffuse * diffuseCompensationTerm(F, DotHV)) * saturate(DotNL);
}

float SampleBDSF (Material M, float3 ViewDirection, float2 U, out float3 SampledDirection) {
    float DiffuseProbability = 0.5f;
    float3 Tangent, Bitangent;
    GetOrthoVectors(M.Normal, Tangent, Bitangent);
    float3 LocalView = float3(
        dot(ViewDirection, Tangent),
        dot(ViewDirection, Bitangent),
        dot(ViewDirection, M.Normal)
    );
    if(U.x <= DiffuseProbability) {
        U.x = U.x / DiffuseProbability;
        SampledDirection = sampleLambert(M.Albedo, U);
    } else {
        U.x = (U.x - DiffuseProbability) / (1.0f - DiffuseProbability);
        SampledDirection = sampleGGX(M.Roughness * M.Roughness, LocalView, U);
    }
    SampledDirection = SampledDirection.x * Tangent + SampledDirection.y * Bitangent + SampledDirection.z * M.Normal;
    float DotNL = dot(M.Normal, SampledDirection);
    float RoughnessAlpha = M.Roughness * M.Roughness;
    float3 HalfVector = normalize(SampledDirection + ViewDirection);
    float  DotNH = dot(HalfVector, M.Normal);
    float  DotNV = dot(HalfVector, ViewDirection);
    float Pdf = sampleLambertPDF(DotNL) * DiffuseProbability
     + sampleGGXPDF(RoughnessAlpha * RoughnessAlpha, DotNH, DotNV, LocalView) * (1.0f - DiffuseProbability);
    return Pdf;
}

#endif // MATERIAL_HLSL