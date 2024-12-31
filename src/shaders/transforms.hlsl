#ifndef TRANSFORMS_HLSL
#define TRANSFORMS_HLSL

#include "math.hlsl"

float3x3 EvaluateRotationMatrix (float4 q) {
    float3x3 R = float3x3(
		1.f - 2.f * (q.y * q.y + q.z * q.z), 2.f * (q.x * q.y - q.w * q.z), 2.f * (q.x * q.z + q.w * q.y),
		2.f * (q.x * q.y + q.w * q.z), 1.f - 2.f * (q.x * q.x + q.z * q.z), 2.f * (q.y * q.z - q.w * q.x),
		2.f * (q.x * q.z - q.w * q.y), 2.f * (q.y * q.z + q.w * q.x), 1.f - 2.f * (q.x * q.x + q.y * q.y)
    );
    return R;
}

float3x3 GetScaleRotationTransform (float3 Scale, float4 Rotation) {
    // Scaling matrix
    float3x3 S = {
        Scale.x, 0, 0,
        0, Scale.y, 0,
        0, 0, Scale.z
    };
    // Rotation matrix
    float3x3 R = EvaluateRotationMatrix(Rotation);
    return mul(S, R);
}

float3x3 GetRotationScaleTransform (float4 Rotation, float3 Scale) {
    // Rotation matrix
    float3x3 R = EvaluateRotationMatrix(Rotation);
    // Scaling matrix
    float3x3 S = {
        Scale.x, 0, 0,
        0, Scale.y, 0,
        0, 0, Scale.z
    };
    return mul(R, S);
}

float3 QuaternionRotate (float3 Vector, float4 Quaternion) {
    float3 q = Quaternion.xyz;
    float3 t = 2.0f * cross(q, Vector);
    return Vector + Quaternion.w * t + cross(q, t);
}

float4 QuaternionConjugate (float4 Quaternion) {
    return float4(-Quaternion.x, -Quaternion.y, -Quaternion.z, Quaternion.w);
}

float3 QuaternionInverseRotate(float3 Vector, float4 Quaternion) {
    float4 Conjugate = QuaternionConjugate(Quaternion);
    return QuaternionRotate(Vector, Conjugate);
}

void GetOrthoVectors(in float3 n, out float3 b1, out float3 b2)
{
    bool sel = abs(n.z) > 0;
    float3 p2 = sel ? n : n.zyx;
    float k = 1.0f / sqrt(squared(p2.z) + squared(n.y));
    b1 = float3(0.0f, -p2.z * k, n.y * k);
    b1 = sel ? b1 : b1.zyx;
    b2 = cross(n, b1);
}


float3 TransformPointWithPerspectiveDivide (float4x4 Transform, float3 Point, bool Max1 = false) {
    float4 Homogeneous = mul(Transform, float4(Point, 1.0f));
    if(!Max1) return Homogeneous.xyz / Homogeneous.w;
    else return Homogeneous.xyz / max(Homogeneous.w, 1.0f);
}

float3 TransformPoint (float3x4 Transform, float3 Point) {
    return mul(Transform, float4(Point, 1.0f)).xyz;
}

float3 TransformVector (float3x4 Transform, float3 Vector) {
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

float3 TransformVector (float3x3 Transform, float3 Vector) {
    return mul(Transform, Vector);
}

#endif