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

#endif