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