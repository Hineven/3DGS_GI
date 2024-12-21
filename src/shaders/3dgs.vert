#include "3dgs.hlsl"
#include "lightgrid.hlsl"
struct DrawActiveGaussians_GSInput
{
    uint PrimitiveIndex : TEXCOORD0;
};

DrawActiveGaussians_GSInput DrawActiveGaussians (
    uint VertexIndex : SV_VertexID
) {
    DrawActiveGaussians_GSInput Input;
    // In reverse order (farthest to nearest)
    Input.PrimitiveIndex = g_RWActiveGaussianCountBuffer[0] - VertexIndex - 1;
    return Input;
}

struct DrawAreaLights_PSInput {
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL;
    float3 Color    : COLOR;
};

DrawAreaLights_PSInput DrawAreaLights (
    uint VertexIndex : SV_VertexID,
    uint InstanceIndex : SV_InstanceID
) {
    DrawAreaLights_PSInput Output = (DrawAreaLights_PSInput)0;
    LightData LD = FetchLightDetails(2 + InstanceIndex);
    switch(VertexIndex) {
        case 0: Output.Position = float4(LD.V1, 1); break;
        case 1: Output.Position = float4(LD.V2, 1); break;
        case 2: Output.Position = float4(LD.V3, 1); break;
    }
    Output.Position = mul(UB.MainCamera.ProjectionView, Output.Position);
    float3 Normal = normalize(cross(LD.V2 - LD.V1, LD.V3 - LD.V1));
    Output.Normal = Normal;
    Output.Color  = LD.Radiance;
    return Output;
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
    Output.Color = float4(g_Debug_VisualizeRayColorBuffer[VertexIndex / 2], 1);
    return Output;
}