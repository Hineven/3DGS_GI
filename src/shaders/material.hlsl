#ifndef MATERIAL_HLSL
#define MATERIAL_HLSL
#include "math_constants.hlsl"
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

float3 EvaluateBSDF (Material Material, float3 LightDirection, float3 ViewDirection) {
    // Simple Lambertian BRDF
    float3 Result = Material.Albedo * max(0.0f, dot(Material.Normal, LightDirection));
    return Result;
}

float3 EvaluateLambertian (float3 Albedo) {
    return Albedo / PI;
}
#endif // MATERIAL_HLSL