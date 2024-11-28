#include "3dgs.hlsl"

struct DrawActiveGaussians_GSInput
{
    uint PrimitiveIndex : TEXCOORD0;
    uint InstanceIndex  : SV_InstanceID;
};

struct DrawActiveGaussians_GSOutput
{
    float4 Position : SV_POSITION;

#ifdef BITPACK_VERTEX_ATTRIBUTES
    float2 UV        : TEXCOORD0;
    uint2  RGBA_RNNN : TEXCOORD1;
#else
    float4 UVWR  : TEXCOORD0;
    float4 GBRN  : TEXCOORD1;
    float2 NN    : TEXCOORD2;
#endif
};

// bool CullActiveListGaussian(uint ActiveListIndex, out float2 NDCPosition, out float LinearDepth) {
//     uint Level = g_RWActiveGaussianHiZLevelBuffer[ActiveListIndex];
//     NDCPosition = UnpackUnorm16x2(g_RWActiveGaussianNDCPositionBuffer[ActiveListIndex]) * 4 - 2;
//     LinearDepth = g_RWActiveGaussianLinearDepthBuffer[ActiveListIndex];
//     float2 UV = NDCPosition * 0.5 + 0.5;
//     float4 DepthValues = g_HiZTexture.GatherRed(g_PointClampSampler, UV, Level);
//     float4 AlphaValues = g_HiATexture.GatherRed(g_PointClampSampler, UV, Level);
//     bool4  AlphaMask = AlphaValues > 0.95f;
//     bool4  DepthMask = DepthValues < LinearDepth;
//     return all(AlphaMask & DepthMask);
// }

[maxvertexcount(6)]
void DrawActiveGaussians(point DrawActiveGaussians_GSInput Input[1], inout TriangleStream<DrawActiveGaussians_GSOutput> TriStream)
{
    int ActiveListIndex = Input[0].PrimitiveIndex;
    // if(ActiveListIndex % 4 != UB.FrameIndex % 4) return ;
    // if(CullActiveListGaussian(ActiveListIndex)) return ;
    
    CameraDescription C = GetCameraDescription();
    
    // Output a quad to bound the Gaussian in screen space
    float2 Center  = UnpackUnorm16x2(g_RWActiveGaussianNDCPositionBuffer[ActiveListIndex]) * 4 - 2;
    float2 Vec1    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector0Buffer[ActiveListIndex]) * 2 - 1;
    float2 Vec2    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector1Buffer[ActiveListIndex]) * 2 - 1;
    // Expand the quad to be conservative
    float  Expand  = 2.25f;
    float2 Top    = Center + Expand * -Vec1;
    float2 Bottom = Center + Expand *  Vec1;
    float2 Vec1H  = Vec1 * 0.5;
    float2 Left1  = Center + Expand * (-Vec2 -Vec1H);
    float2 Left2  = Center + Expand * (-Vec2 +Vec1H);
    float2 Right1 = Center + Expand * ( Vec2 -Vec1H);
    float2 Right2 = Center + Expand * ( Vec2 +Vec1H);
    
    float Depth = g_RWActiveGaussianLinearDepthBuffer[ActiveListIndex] / C.FarPlane;
    int GaussianIndex = g_RWActiveGaussianListBuffer[ActiveListIndex];
    Gaussian G = FetchGaussian(GaussianIndex);
    // GaussianIndex = max(0, min(GaussianIndex, 10000));
    GaussianPBR G_PBR = FetchGaussianPBR(GaussianIndex);
    
    float4 AlbedoAlpha = float4(G_PBR.Albedo, G.Alpha);
#ifdef OUTPUT_COLORED_GAUSSIANS
    AlbedoAlpha.rgb = saturateDown(g_RWActiveGaussianColorBuffer[ActiveListIndex]);
#endif
    
    float3 Albedo = AlbedoAlpha.xyz;
    float  Alpha = AlbedoAlpha.w;
    float  Roughness = G_PBR.Roughness;
    // SHCoefficents3 SH = FetchGaussianSHCoefficients(GaussianIndex, 0);
    // FIXME bad?
    float3 WorldNormal = GaussianInstance_TransformLocalToWorld_Normal(G_PBR.Normal, Input[0].InstanceIndex);
    float3 NormalU = saturateDown(WorldNormal * 0.5 + 0.5);

    DrawActiveGaussians_GSOutput Output;
#ifdef BITPACK_VERTEX_ATTRIBUTES
    uint2 RGBA_RNNN = uint2(PackRGBA8(float4(Albedo, Alpha)), PackRGBA8(float4(Roughness, NormalU)));

    Output.RGBA_RNNN = RGBA_RNNN;

    // L1, L2, T, B, R1, R2

    Output.UV = Expand * float2(-1, -0.5);
    Output.Position = float4(Left1, Depth, 1);
    TriStream.Append(Output);

    Output.UV = Expand * float2(-1,  0.5);
    Output.Position = float4(Left2, Depth, 1);
    TriStream.Append(Output);

    Output.UV = Expand * float2(0, -1);
    Output.Position = float4(Top, Depth, 1);
    TriStream.Append(Output);

    Output.UV = Expand * float2(0, 1);
    Output.Position = float4(Bottom, Depth, 1);
    TriStream.Append(Output);

    Output.UV = Expand * float2(1,  -0.5);
    Output.Position = float4(Right1, Depth, 1);
    TriStream.Append(Output);

    Output.UV = Expand * float2(1,  0.5);
    Output.Position = float4(Right2, Depth, 1);
    TriStream.Append(Output);

#else
    Output.UVWR.zw = float2(Alpha, Albedo.x);
    Output.GBRN = float4(Albedo.yz, Roughness, NormalU.x);
    Output.NN = NormalU.yz;

    Output.UVWR.xy = Expand * float2(-1, -0.5);
    Output.Position = float4(Left1, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = Expand * float2(-1,  0.5);
    Output.Position = float4(Left2, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = Expand * float2(0, -1);
    Output.Position = float4(Top, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = Expand * float2(0, 1);
    Output.Position = float4(Bottom, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = Expand * float2(1, -0.5);
    Output.Position = float4(Right1, Depth, 1);
    TriStream.Append(Output);

    Output.UVWR.xy = Expand * float2(1,  0.5);
    Output.Position = float4(Right2, Depth, 1);
    TriStream.Append(Output);
#endif

    
    TriStream.RestartStrip();
}
