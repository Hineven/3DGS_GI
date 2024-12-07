#ifndef CONVENTIONS_HLSL
#define CONVENTIONS_HLSL

#include "math.hlsl"

float3x4 ClipMatrix (float4x4 M) {
    return float3x4(
        M[0][0], M[0][1], M[0][2], M[0][3],
        M[1][0], M[1][1], M[1][2], M[1][3],
        M[2][0], M[2][1], M[2][2], M[2][3]
    );
}

void Swap(inout float a, inout float b) {
    float temp = a;
    a = b;
    b = temp;
}
void Swap(inout int a, inout int b) {
    int temp = a;
    a = b;
    b = temp;
}

float2 NDC2ToUV (float2 NDC2) {
    return float2(0.5f, -0.5f) * NDC2 + 0.5f.xx;
}

float2 UVToNDC2 (float2 UV) {
    return float2(2.f, -2.f) * (UV - 0.5f.xx);
}


float UnpackUnorm16 (uint Packed) {
    return Packed / 65535.0f;
}

uint PackUnorm16 (float Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked * 65536.0f);
}

float2 UnpackUnorm16x2 (uint Packed) {
    return float2(
        (Packed & 0xFFFF) / 65535.0f,
        (Packed >> 16) / 65535.0f
    );
}

uint PackUnorm16x2 (float2 Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked.x * 65536.0f) + (uint(Unpacked.y * 65536.0f) << 16);
}

uint2 PackFp16x4Safe (float4 Unpacked) {
    // Clamp to fp16 range
    Unpacked = clamp(Unpacked, -65504.0f, 65504.0f);
    // Convert to fp16
    uint2 Packed = uint2(
        f32tof16(Unpacked.x) | (f32tof16(Unpacked.y) << 16),
        f32tof16(Unpacked.z) | (f32tof16(Unpacked.w) << 16)
    );
    return Packed;
}

float4 UnpackFp16x4 (uint2 Packed) {
    float4 Unpacked = float4(
        f16tof32(Packed.x & 0xFFFF),
        f16tof32(Packed.x >> 16),
        f16tof32(Packed.y & 0xFFFF),
        f16tof32(Packed.y >> 16)
    );
    return Unpacked;
}

uint2 PackFp16x3Safe (float3 Unpacked) {
    // Clamp to fp16 range
    Unpacked = clamp(Unpacked, -65504.0f, 65504.0f);
    // Convert to fp16
    uint2 Packed = uint2(
        f32tof16(Unpacked.x) | (f32tof16(Unpacked.y) << 16),
        f32tof16(Unpacked.z)
    );
    return Packed;
}

float3 UnpackFp16x3 (uint2 Packed) {
    float3 Unpacked = float3(
        f16tof32(Packed.x & 0xFFFF),
        f16tof32(Packed.x >> 16),
        f16tof32(Packed.y & 0xFFFF)
    );
    return Unpacked;
}

uint2 PackRadianceA16 (float4 Unpacked) {
    uint2 RGBPacked = PackFp16x3Safe(Unpacked.xyz);
    return uint2(
        RGBPacked.x,
        RGBPacked.y | (PackUnorm16(Unpacked.w) << 16)
    );
}

float4 UnpackRadianceA16 (uint2 Packed) {
    float3 RGBUnpacked = UnpackFp16x3(Packed);
    float AUnpacked = UnpackUnorm16(Packed.y >> 16);
    return float4(RGBUnpacked, AUnpacked);
}

uint PackRGBA8 (float4 Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked.x * 256.0f) 
        | (uint(Unpacked.y * 256.0f) << 8)
        | (uint(Unpacked.z * 256.0f) << 16)
        | (uint(Unpacked.w * 256.0f) << 24);
}

float4 UnpackRGBA8 (uint Packed) {
    return float4(
        (Packed & 0xFFu) / 256.0f,
        ((Packed >> 8u) & 0xFF) / 256.0f,
        ((Packed >> 16u) & 0xFFu) / 256.0f,
        ((Packed >> 24u) & 0xFFu) / 256.0f
    );
}


#endif