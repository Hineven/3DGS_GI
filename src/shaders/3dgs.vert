#include "3dgs.hlsl"
#include "lightgrid.hlsl"
#include "probes.hlsl"
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

struct DrawRegulareMeshes_PSInput {
    float4 Position : SV_POSITION;
    // float2 UV       : TEXCOORD0;
    float3 Normal   : NORMAL;
    float4 AlbedoRoughess : COLOR0;
    float3 Emission : COLOR1;
};

uint g_DrawRegulareMeshes_InstanceIndex;

DrawRegulareMeshes_PSInput DrawRegularMeshes (
    Vertex InVertex,
    uint VertexIndex : SV_VertexID
) {
    DrawRegulareMeshes_PSInput Output = (DrawRegulareMeshes_PSInput)0;
    float3 Position = mul(g_InstanceTransformBuffer[g_DrawRegulareMeshes_InstanceIndex], float4(InVertex.Position, 1.f));
    Output.Position = mul(UB.MainCamera.ProjectionView, float4(Position, 1));
    float3 Normal   = mul(g_InstanceNormalTransformBuffer[g_DrawRegulareMeshes_InstanceIndex], InVertex.Normal);
    Output.Normal   = Normal;
    SimpleMaterial M = g_MaterialBuffer[g_DrawRegulareMeshes_InstanceIndex];
    Output.AlbedoRoughess = float4(M.Albedo, M.Roughness);
    Output.Emission       = M.Emissive;
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

struct Debug_SSRC_VisualizeProbeUpdateRays_FSInput {
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

Debug_SSRC_VisualizeProbeUpdateRays_FSInput Debug_SSRC_VisualizeProbeUpdateRays (
    uint VertexIndex : SV_VertexID,
    uint InstanceIndex : SV_InstanceID
) {
    CameraDescription C = GetCameraDescription();
    int2 ProbeIndex = g_Debug_SSRC_ProbeIndexBuffer[0];
    int  ProbeIndex1 = ProbeIndex.x + ProbeIndex.y * UB.TileDimensions.x;
    ProbeHeader Header = GetScreenProbeHeader(ProbeIndex);
    int RayRank   = InstanceIndex;
    int RayIndex  = g_RWProbeUpdateRayOffsetsBuffer[ProbeIndex1] + RayRank;
    float3 RayOrigin         = Header.Position;
    float3 RayDirection      = OctahedronToUnitVector(UnpackUnorm16x2(g_RWProbeUpdateRayDirectionBuffer[RayIndex]) * 2.f - 1.f);
    ProbeUpdateRayResult Result = FetchProbeUpdateRayResult(RayIndex);
    float3 RayRadiance       = Result.Radiance;
    float  InvPdf            = Result.InvPdf;
    // Negative depth indicate backface hits
    float  RayLinearDepth    = abs(g_RWProbeUpdateRayDepthBuffer[RayIndex]);
    float3 World;
    if(VertexIndex == 0) {
        World = RayOrigin;
    } else {
        World = RayOrigin + RayDirection * RayLinearDepth;
    }
    Debug_SSRC_VisualizeProbeUpdateRays_FSInput Output;
    Output.Position  = mul(C.ProjectionView, float4(World, 1.f));
    float3 Color     = RayRadiance;
    Output.Color     = float4(Color, 1.f);//UB.DebugOption_SSRC_VisualizeInvalidRays ? 1.f : (InvPdf > 0 ? 1.f : 0.f));
    return Output;
}