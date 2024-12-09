#include "3dgs.hlsl"
struct DrawActiveGaussians_GSInput
{
    uint PrimitiveIndex : TEXCOORD0;
    uint InstanceIndex  : SV_InstanceID;
};

DrawActiveGaussians_GSInput DrawActiveGaussians (
    uint VertexIndex : SV_VertexID,
    uint InstanceIndex : SV_InstanceID
) {
    DrawActiveGaussians_GSInput Input;
    // In reverse order (farthest to nearest)
    Input.PrimitiveIndex = g_RWActiveGaussianCountBuffer[0] - VertexIndex - 1;
    Input.InstanceIndex  = InstanceIndex;
    return Input;
}

float4 TonemapAndDraw (uint VertexIndex : SV_VertexID) : SV_Position {
    float2 Positions[3] = {
        float2(0, 0),
        float2(1, 0),
        float2(0, 1)
    };
    return float4(Positions[VertexIndex] * 4 - 1, 0, 1);
}

struct Debug_VisualizeRays_FSInput {
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

Debug_VisualizeRays_FSInput Debug_VisualizeRays (
    uint VertexIndex : SV_VertexID
) {
    Debug_VisualizeRays_FSInput Output;
    float4 Position = float4(g_Debug_VisualizeRayVertexBuffer[VertexIndex], 1.f);
    Output.Position = mul(UB.MainCamera.ProjectionView, Position);
    Output.Color = g_Debug_VisualizeRayColorBuffer[VertexIndex / 2];
    return Output;
}