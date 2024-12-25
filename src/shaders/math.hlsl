#ifndef MATH_HLSL
#define MATH_HLSL
#include "math_constants.hlsl"
#include "select.hlsl"

float squared (float x) {
    return x * x;
}

float2 UnitVectorToOctahedron(float3 N)
{
	N.xy /= dot( 1, abs(N) );
	if( N.z <= 0 )
	{
		N.xy = ( 1 - abs(N.yx) ) * select(N.xy >= 0, float2(1,1), float2(-1,-1));
	}
	return N.xy;
}

float2 UnitVectorToOctahedron01(float3 N)
{
    return (UnitVectorToOctahedron(N) + 1) * 0.5;
}

float3 OctahedronToUnitVector( float2 Oct )
{
	float3 N = float3( Oct, 1 - dot( 1, abs(Oct) ) );
	float t = max( -N.z, 0 );
	N.xy += select(N.xy >= 0, float2(-t, -t), float2(t, t));
	return normalize(N);
}

float3 Octahedron01ToUnitVector (float2 Oct)
{
    return OctahedronToUnitVector(Oct * 2 - 1);
}

float2 UnitVectorToHemiOctahedron( float3 N )
{
	N.xy /= dot( 1, abs(N) );
	return float2( N.x + N.y, N.x - N.y );
}

// Area preserving mapping
float2 UnitVectorToHemiOctahedron01A( float3 direction )
{
    // Modified version of "Fast Equal-Area Mapping of the (Hemi)Sphere using SIMD" - Clarberg
    float3 absDir = abs(direction);

    float radius = sqrt(1.0f - absDir.z);
    float a = max(absDir.x, absDir.y);
    float b = min(absDir.x, absDir.y);
    b = a == 0.0f ? 0.0f : b / a;

    float phi = atan(b) * (2.0f / PI);
    phi = (absDir.x >= absDir.y) ? phi : 1.0f - phi;

    float t = phi * radius;
    float s = radius - t;
    float2 st = float2(s, t);
    st *= sign(direction).xy;

    // Since we only care about the hemisphere above the surface we rescale and center the output
    //   value range to the it occupies the whole unit square
    st = float2(st.x + st.y, st.x - st.y);

    // Transform from [-1,1] to [0,1]
    st = 0.5f.xx * st + 0.5f.xx;

    return st;
}

float3 HemiOctahedronToUnitVector( float2 Oct )
{
	Oct = float2( Oct.x + Oct.y, Oct.x - Oct.y );
	float3 N = float3( Oct, 2.0 - dot( 1, abs(Oct) ) );
	return normalize(N);
}

// Area preserving mapping
float3 HemiOctahedron01ToUnitVectorA( float2 mapped )
{
    // Transform from [0,1] to [-1,1]
    float2 st = 2.0f.xx * mapped - 1.0f.xx;

    // Transform from unit square to diamond corresponding to +hemisphere
    st = float2(st.x + st.y, st.x - st.y) * 0.5f;

    float2 absMapped = abs(st);
    float distance = 1.0f - (absMapped.x + absMapped.y);
    float radius = 1.0f - abs(distance);

    float phi = (radius == 0.0f) ? 0.0f : QUARTER_PI * ((absMapped.y - absMapped.x) / radius + 1.0f);
    float radiusSqr = radius * radius;
    float sinTheta = radius * sqrt(2.0f - radiusSqr);
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    float x = sinTheta * sign(st.x) * cosPhi;
    float y = sinTheta * sign(st.y) * sinPhi;
    float z = sign(distance) * (1.0f - radiusSqr);

    return float3(x, y, z);
}

// Clamp to [0, 1)
float saturateDown (float Value) {
    return clamp(Value, 0.0f, 1.f - FLT_EPSILON);
}
float2 saturateDown (float2 Value) {
    return clamp(Value, 0.0f.xx, 1.f.xx - FLT_EPSILON.xx);
}
float3 saturateDown (float3 Value) {
    return clamp(Value, 0.0f.xxx, 1.f.xxx - FLT_EPSILON.xxx);
}
float4 saturateDown (float4 Value) {
    return clamp(Value, 0.0f.xxxx, 1.f.xxxx - FLT_EPSILON.xxxx);
}

// Clamp to (0, 1]
float saturateUp (float Value) {
    return clamp(Value, FLT_EPSILON, 1.0f);
}
float2 saturateUp (float2 Value) {
    return clamp(Value, FLT_EPSILON, 1.0f);
}

float  InterpolateBarycentrics (float A, float B, float C, float2 UV) {
    return A * (1 - UV.x - UV.y) + B * UV.x + C * UV.y;
}
float3 InterpolateBarycentrics (float3 A, float3 B, float3 C, float2 UV) {
    return A * (1 - UV.x - UV.y) + B * UV.x + C * UV.y;
}

uint hadd (uint2 Value) {
    return Value.x + Value.y;
}

uint hadd (uint3 Value) {
    return Value.x + Value.y + Value.z;
}

uint hadd (uint4 Value) {
    return Value.x + Value.y + Value.z + Value.w;
}

#endif