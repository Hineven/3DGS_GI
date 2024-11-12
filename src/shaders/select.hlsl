#ifndef INC_3DGS_SELECT_HLSL
#define INC_3DGS_SELECT_HLSL
#if __HLSL_VERSION < 2021
float2 select(bool2 a, float2 b, float2 c)
{
    return a ? b : c;
}

float3 select(bool3 a, float3 b, float3 c)
{
    return a ? b : c;
}

float4 select(bool4 a, float4 b, float4 c)
{
    return a ? b : c;
}

int2 select(bool2 a, int2 b, int2 c)
{
    return a ? b : c;
}

int3 select(bool3 a, int3 b, int3 c)
{
    return a ? b : c;
}

int4 select(bool4 a, int4 b, int4 c)
{
    return a ? b : c;
}

uint2 select(bool2 a, uint2 b, uint2 c)
{
    return a ? b : c;
}

uint3 select(bool3 a, uint3 b, uint3 c)
{
    return a ? b : c;
}

uint4 select(bool4 a, uint4 b, uint4 c)
{
    return a ? b : c;
}

double2 select(bool2 a, double2 b, double2 c)
{
    return a ? b : c;
}

double3 select(bool3 a, double3 b, double3 c)
{
    return a ? b : c;
}

double4 select(bool4 a, double4 b, double4 c)
{
    return a ? b : c;
}

half2 select(bool2 a, half2 b, half2 c)
{
    return a ? b : c;
}

half3 select(bool3 a, half3 b, half3 c)
{
    return a ? b : c;
}

half4 select(bool4 a, half4 b, half4 c)
{
    return a ? b : c;
}

bool2 and(bool2 a, bool2 b)
{
    return a && b;
}

bool3 and(bool3 a, bool3 b)
{
    return a && b;
}

bool4 and(bool4 a, bool4 b)
{
    return a && b;
}

bool2 or(bool2 a, bool2 b)
{
    return a || b;
}

bool3 or(bool3 a, bool3 b)
{
    return a || b;
}

bool4 or(bool4 a, bool4 b)
{
    return a || b;
}
#endif
#endif // INC_3DGS_SELECT_HLSL