struct DrawActiveGaussians_GSOutput
{
    float4 Position    : SV_POSITION;
    float4 UVWR : TEXCOORD0;
    float4 GBMR : TEXCOORD1;
};

DrawActiveGaussians_GSOutput DrawActiveGaussians (
    DrawActiveGaussians_GSOutput Input
) {
    // Fall through
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