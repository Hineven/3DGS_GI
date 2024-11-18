float4 DrawActiveGaussians (uint VertexIndex : SV_VertexID) : SV_POSITION {
    float2 Position2 = 0;
    {
        // 6 vertices (hexagon) for each gaussian
        float XOffs = sqrt(3) / 2;
        float2 Positions[6] = {
            float2(0, 1),
            float2(-XOffs, 0.5),
            float2(-XOffs, -0.5),
            float2(0, -1),
            float2(XOffs, -0.5),
            float2(XOffs, 0.5)
        };
        int3 Indices[4] = {
            int3(0, 1, 2),
            int3(0, 2, 3),
            int3(0, 3, 4),
            int3(0, 4, 5)
        };
        Position2 = Positions[Indices[VertexIndex / 3][VertexIndex % 3]];
        float3 Transform = 
    }

}

float4 TonemapAndDraw (uint VertexIndex : SV_VertexID) : SV_Position {
    float2 Positions[3] = {
        float2(0, 0),
        float2(1, 0),
        float2(0, 1)
    };
    return float4(Positions[VertexIndex] * 4 - 1, 0, 1);
}