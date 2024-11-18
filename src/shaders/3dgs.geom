#include "3dgs.hlsl"

struct DrawActiveGaussians_GSInput
{
    uint VertexIndex : SV_VertexID;
};

struct DrawActiveGaussians_GSOutput
{
    float4 Position    : SV_POSITION;
    float4 UVWActiveID : TEXCOORD;
};

[maxvertexcount(4)]
void DrawActiveGaussians(point DrawActiveGaussians_GSInput Input[1], inout TriangleStream<DrawActiveGaussians_GSOutput> TriStream)
{
    if(Input[0].VertexIndex >= g_RWActiveGaussianCountBuffer[0]) return ;
    int ActiveListIndex = Input[0].VertexIndex;
    
    CameraDescription C = GetCameraDescription();
    
    // Output a quad to bound the Gaussian in screen space
    float2 Center  = UnpackUnorm16x2(g_RWActiveGaussianNDCPositionBuffer[ActiveListIndex]) * 4 - 2;
    float2 Vec1    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector0Buffer[ActiveListIndex]) * 2 - 1;
    float2 Vec2    = UnpackUnorm16x2(g_RWActiveGaussianQuadNDCVector1Buffer[ActiveListIndex]) * 2 - 1;
    // Expand the quad to be conservative
    float  Expand  = 1.5f;
    float2 Top    = Center - Expand * Vec1;
    float2 Bottom = Center + Expand * Vec1;
    float2 Left   = Center - Expand * Vec2;
    float2 Right  = Center + Expand * Vec2;
    
    DrawActiveGaussians_GSOutput Output;
    
    Output.UVWActiveID = float4(-Expand, 0, Alpha, ActiveListIndex);
    Output.Position = float4(Top, 0, 1);
    TriStream.Append(Output);

    Output.UVWActiveID = float4(Expand, 0,  Alpha, ActiveListIndex);
    Output.Position = float4(Bottom, 0, 1);
    TriStream.Append(Output);

    Output.UVWActiveID = float4(0, -Expand, Alpha, ActiveListIndex);
    Output.Position = float4(Left, 0, 1);
    TriStream.Append(Output);

    Output.UVWActiveID = float4(0, Expand,  Alpha, ActiveListIndex);
    Output.Position = float4(Right, 0, 1);
    TriStream.Append(Output);
    
    TriStream.RestartStrip();
}