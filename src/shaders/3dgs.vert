
float4 TonemapAndDraw (uint VertexIndex : SV_VertexID) : SV_Position {
    float3 Positions[3] = {
        float3(0, 0, 0),
        float3(1, 0, 0),
        float3(0, 1, 0)
    };
    return float4(Positions[VertexIndex] * 2, 1);
}