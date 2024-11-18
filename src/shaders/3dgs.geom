#include "3dgs.hlsl"

struct DrawActiveGaussians_GSInput
{
    uint VertexIndex : SV_VertexID;
};

struct DrawActiveGaussians_GSOutput
{
    float4 Pos : SV_POSITION;
};

[maxvertexcount(4)]
void DrawActiveGaussians(point DrawActiveGaussians_GSInput Input[1], inout TriangleStream<DrawActiveGaussians_GSOutput> TriStream)
{
    Gaussian G = FetchGaussian(Input[0].VertexIndex);
    CameraDescription C = GetCamera(RASTERIZATION_CAMERA_INDEX);
    if(!IsPointInFrustrum(C, G.Position, RASTERIZATION_IS_ORTHO_CAMERA, 0.3f)) return;

    
    SymmetricMatrix Cov3D = ComputeCovarianceMatrix(G.Scale, G.Rotation);

    // Project to 2D covariance
    float3x3 J = float3x3(
        1, 0, 0,
        0, 1, 0,
        0, 0, 0
    );
#if !(RASTERIZATION_IS_ORTHO_CAMERA)
    J = EWAJacobian(G.Position, C.FieldOfView, C.View);
#endif
    float3 Cov2D = ProjectCovarianceMatrixToNDC(
        J, Cov3D, C.View
    );

    // Expand the variance magnitude (screen space) to be conservative
    float  Expand        = 0.01f;
    float  Det           = Cov2D.x * Cov2D.z - Cov2D.y * Cov2D.y;
    float3 Cov2DExpanded = float3(Cov2D.x + Expand, Cov2D.y, Cov2D.z + Expand);
    float  DetExpanded   = Cov2DExpanded.x * Cov2DExpanded.z - Cov2DExpanded.y * Cov2DExpanded.y;
    // Reduce the opacity of the projected 2d gaussian (because we expanded the variance, ie. size of the gaussian)
    float  Scaling       =  sqrt(max(0.000025f, Det / DetExpanded));

    if (DetExpanded <= 0.0f) return;

    float  InvDet = 1.f / DetExpanded;
    float3 Conic  = float3(Cov2DExpanded.z * InvDet, -Cov2DExpanded.y * InvDet, Cov2DExpanded.x * InvDet);

    // Compute extent in screen space (by finding eigenvalues of
    // 2D covariance matrix). Use extent to compute a bounding rectangle
    // of screen-space tiles that this Gaussian overlaps with. Quit if
    // rectangle covers 0 tiles. 
    // Eigenvalues of 2D covariance matrix denotes the variance of the 
    // Gaussian in the directions of the eigenvectors, which are the
    // principal axes of the Gaussian. 
    float Mean    = 0.5f * (Cov2DExpanded.x + Cov2DExpanded.z);
    float Lambda1 = Mean + sqrt(max(0.1f, Mean * Mean - DetExpanded));
    float Lambda2 = Mean - sqrt(max(0.1f, Mean * Mean - DetExpanded));
    // Find the eigenvectors of the covariance matrix
    float2 Eigenvector1 = float2(Cov2D.y, Lambda1 - Cov2D.x);
    float2 Eigenvector2 = float2(Cov2D.y, Lambda2 - Cov2D.x);
    Eigenvector1 = normalize(Eigenvector1);
    Eigenvector2 = normalize(Eigenvector2);
    
    // Output a quad to bound the Gaussian in screen space
    float2 Center  = Homogeneous.xy;
    float2 LeftTop = Center - Lambda1 * Eigenvector1 - Lambda2 * Eigenvector2;
    float2 RightTop = Center + Lambda1 * Eigenvector1 - Lambda2 * Eigenvector2;
    float2 LeftBottom = Center - Lambda1 * Eigenvector1 + Lambda2 * Eigenvector2;
    float2 RightBottom = Center + Lambda1 * Eigenvector1 + Lambda2 * Eigenvector2;
    
    VS_OUTPUT output;
    
    output.Pos = float4(LeftTop, Homogeneous.z, 1);
    TriStream.Append(output);
    
    output.Pos = float4(RightTop, Homogeneous.z, 1);
    TriStream.Append(output);
    
    output.Pos = float4(RightBottom, Homogeneous.z, 1);
    TriStream.Append(output);
    
    output.Pos = float4(LeftBottom, Homogeneous.z, 1);
    TriStream.Append(output);
    
    TriStream.RestartStrip();
}