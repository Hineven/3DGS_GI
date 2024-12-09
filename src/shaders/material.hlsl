#ifndef MATERIAL_HLSL
#define MATERIAL_HLSL
struct Material {
    float3 Normal;
    float3 Albedo;
    float  Alpha;
};

Material MakeMaterial (float3 Normal, float3 Albedo) {
    Material Result;
    Result.Normal = Normal;
    Result.Albedo = Albedo;
    return Result;
}

float3 EvaluateBSDF (Material Material, float3 LightDirection, float3 ViewDirection) {
    // Simple Lambertian BRDF
    float3 Result = Material.Albedo * max(0.0f, dot(Material.Normal, LightDirection));
    return Result;
}
#endif // MATERIAL_HLSL