#include "3dgs.hlsl"

struct DrawActiveGaussians_GSInput
{
    uint PrimitiveIndex : TEXCOORD0;
};

struct DrawActiveGaussians_GSOutput
{
    float4 Position : SV_POSITION;
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
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
    // Vec1 = float2(-0.005, 0);
    // Vec2 = float2(0, -0.005);
    // Expand the quad to be conservative
    float  Expand  = 3.f;
    float2 Top    = Center - Expand * Vec1;
    float2 Bottom = Center + Expand * Vec1;
    float2 Left   = Center - Expand * Vec2;
    float2 Right  = Center + Expand * Vec2;
    
    float Depth = g_RWActiveGaussianLinearDepthBuffer[ActiveListIndex] / C.FarPlane;
    int GaussianIndex = g_RWActiveGaussianListBuffer[ActiveListIndex];
    // Gaussian G = FetchGaussian(GaussianIndex);
    float4 ColorAlpha = g_RWActiveGaussianConicWBuffer[ActiveListIndex].xyzw;
    float3 Color = ColorAlpha.xyz;
    float  Alpha = ColorAlpha.w;
    // SHCoefficents3 SH = FetchGaussianSHCoefficients(GaussianIndex, 0);

    DrawActiveGaussians_GSOutput Output;
    
    Output.UVWR = float4(-Expand, 0, Alpha, Color.x);
    Output.GBMR = float4(Color.yz, 0.xx);
    Output.Position = float4(Left, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR = float4(0, -Expand,  Alpha, Color.x);
    Output.GBMR = float4(Color.yz, 0.xx);
    Output.Position = float4(Bottom, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR = float4(0, Expand, Alpha, Color.x);
    Output.GBMR = float4(Color.yz, 0.xx);
    Output.Position = float4(Top, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR = float4(Expand, 0,  Alpha, Color.x);
    Output.GBMR = float4(Color.yz, 0.xx);
    Output.Position = float4(Right, Depth, 1);
    TriStream.Append(Output);
    
    TriStream.RestartStrip();
}
