#ifndef SAMPLING_HLSL
#define SAMPLING_HLSL

#include "math_constants.hlsl"

float3 UniformSampleHemisphere (float2 u) {
    float2 SinCos;
    sincos(2.f * PI * u.x, SinCos.x, SinCos.y);
    float Z = u.y;
    float R = sqrt(max(1.f - Z * Z, 0.f));
    return float3(R * SinCos.y, R * SinCos.x, Z);
}

float UniformSampleHemispherePdf ()
{
    return 1.f / TWO_PI;
}

float3 UniformSampleSphere(float2 u)
{
    float2 SinCos;
    sincos(2.f * PI * u.x, SinCos.x, SinCos.y);
    float Z = 1.f - 2.f * u.y;
    float R = sqrt(max(1.f - Z * Z, 0));
    return float3(R * SinCos.y, R * SinCos.x, Z);
}

float UniformSampleSpherePdf ()
{
    return 1.f / FOUR_PI;
}

float3 CosineWeightedSampleHemisphere (float2 u) {
    float2 SinCos;
    sincos(2.f * PI * u.x, SinCos.x, SinCos.y);
    float Z = sqrt(u.y);
    float R = sqrt(max(1.f - Z * Z, 0.f));
    return float3(R * SinCos.y, R * SinCos.x, Z);
}

float CosineWeightedSampleHemispherePDF (float CosTheta) {
    return CosTheta * (1.f / PI);
}

float CosineWeightedSampleHemispherePDF (float3 Direction, float3 Normal) {
    return max(0.f, dot(Direction, Normal)) * (1.f / PI);
}

#endif