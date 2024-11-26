#include "3dgs.hlsl"

struct DrawActiveGaussians_GSInput
{
    uint PrimitiveIndex : TEXCOORD0;
};

struct DrawActiveGaussians_GSOutput
{
    float4 Position : SV_POSITION;

#ifdef BITPACK_VERTEX_ATTRIBUTES
    float4 UV_RGBA_MRXX : TEXCOORD0;
#else
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
#endif
};

[maxvertexcount(4)]
void DrawActiveGaussians(point DrawActiveGaussians_GSInput Input[1], inout TriangleStream<DrawActiveGaussians_GSOutput> TriStream)
{
    int ActiveListIndex = Input[0].PrimitiveIndex;
    
    CameraDescription C = GetCameraDescription();
    
    // Output a quad to bound the Gaussian in screen space
    float2 Center  = UnpackUnorm16x2(g_RWActiveGaussianNDCPositionBuffer[ActiveListIndex]) * 4 - 2;
    float2 Vec1    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector0Buffer[ActiveListIndex]) * 2 - 1;
    float2 Vec2    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector1Buffer[ActiveListIndex]) * 2 - 1;
    // Expand the quad to be conservative
    float  Expand  = 5.f;
    float2 Top    = Center - Expand * Vec1;
    float2 Bottom = Center + Expand * Vec1;
    float2 Left   = Center - Expand * Vec2;
    float2 Right  = Center + Expand * Vec2;
    
    float Depth = g_RWActiveGaussianLinearDepthBuffer[ActiveListIndex] / C.FarPlane;
    int GaussianIndex = g_RWActiveGaussianListBuffer[ActiveListIndex];
    Gaussian G = FetchGaussian(GaussianIndex);
    GaussianPBR G_PBR = FetchGaussianPBR(GaussianIndex);
    float4 AlbedoAlpha = float4(G_PBR.Albedo, G.Alpha);//g_RWActiveGaussianConicWBuffer[ActiveListIndex].xyzw;
    float3 Albedo = AlbedoAlpha.xyz;
    float  Alpha = AlbedoAlpha.w;
    float  Roughness = G_PBR.Roughness;
    // SHCoefficents3 SH = FetchGaussianSHCoefficients(GaussianIndex, 0);

    DrawActiveGaussians_GSOutput Output;
#ifdef BITPACK_VERTEX_ATTRIBUTES
    uint2 RGBA_MRXX = uint2(PackRGBA8(float4(Albedo, Alpha)), PackRGBA8(float4(1, Roughness, 0, 0)));

    Output.UV_RGBA_MRXX.zw = asfloat(RGBA_MRXX);

    Output.UV_RGBA_MRXX.xy = float2(-Expand, 0);
    Output.Position = float4(Left, Depth, 1);
    TriStream.Append(Output);

    Output.UV_RGBA_MRXX.xy = float2(0, -Expand);
    Output.Position = float4(Bottom, Depth, 1);
    TriStream.Append(Output);

    Output.UV_RGBA_MRXX.xy = float2(0, Expand);
    Output.Position = float4(Top, Depth, 1);
    TriStream.Append(Output);

    Output.UV_RGBA_MRXX.xy = float2(Expand, 0);
    Output.Position = float4(Right, Depth, 1);
    TriStream.Append(Output);
#else
    Output.UVWR.zw = float2(Alpha, Albedo.x);
    Output.GBMR = float4(Albedo.yz, 1, Roughness);

    Output.UVWR.xy = float2(-Expand, 0);
    Output.Position = float4(Left, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = float2(0, -Expand);
    Output.Position = float4(Bottom, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = float2(0, Expand);
    Output.Position = float4(Top, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = float2(Expand, 0);
    Output.Position = float4(Right, Depth, 1);
    TriStream.Append(Output);
#endif

    
    TriStream.RestartStrip();
}
