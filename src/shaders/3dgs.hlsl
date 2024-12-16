#ifndef INC_3DGS_HLSL
#define INC_3DGS_HLSL

#include "../device_shared.hlsl"

#include "3dgs_inc.hlsl"
#include "select.hlsl"
#include "math.hlsl"
#include "transforms.hlsl"
#include "conventions.hlsl"
#include "bluenoise.hlsl"
#include "material.hlsl"

bool IsInvalid (uint Value) {
    return Value == INVALID_U32;
}

// Convert NDC2 to screen position
// NDC2: [-1, 1] -> Screen: [0, ScreenDimensions]
float2 NDC2ToScreen(float2 NDC2) {
    float2 T = float2(NDC2.x, -NDC2.y);
    return 0.5f * UB.ScreenDimensions * (T + 1.0f);
}

float2 NDC2ToFilm (CameraDescription C, float2 NDC2) {
    float2 T = float2(NDC2.x, -NDC2.y);
    return 0.5f * float2(C.FilmDimensions) * (T + 1.0f);
}

// Convert screen position to NDC2
// Screen: [0, ScreenDimensions] -> NDC2: [-1, 1]
float2 ScreenToNDC2(float2 Screen) {
    float2 T = 2.0f * Screen / UB.ScreenDimensions - 1.0f;
    return float2(T.x, -T.y);
}

float2 ScreenToUV (float2 Screen) {
    return Screen / UB.ScreenDimensions;
}

float2 FilmToNDC2(CameraDescription C, float2 Film) {
    float2 T = 2.f * Film / float2(C.FilmDimensions) - 1.f;
    return float2(T.x, -T.y);
}

float3x4 FetchInstanceTransform (int Index) {
    return g_InstanceTransformBuffer[Index];
}
float3x4 FetchInstanceInverseTransform (int Index) {
    return g_InstanceInvTransformBuffer[Index];
}

float3 GaussianInstance_TransformLocalToWorld (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return TransformPoint(Transform, Position);
}
float3 GaussianInstance_TransformWorldToLocal (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return TransformPoint(Transform, Position);
}

float3 GaussianInstance_TransformLocalToWorld_Normal (float3 Normal, int InstanceIndex) {
    float3x3 Transform = g_InstanceNormalTransformBuffer[InstanceIndex];
    return TransformVector(Transform, Normal);
}

float3 GaussianInstance_TransformLocalToWorld_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

float3 GaussianInstance_TransformWorldToLocal_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

RayDesc GaussianInstance_TransformWorldToLocal (RayDesc Ray, int InstanceIndex, out float RayScaler) {
    RayDesc LocalRay = Ray;
    LocalRay.Origin = GaussianInstance_TransformWorldToLocal(Ray.Origin, InstanceIndex);
    float3 LocalDirection = GaussianInstance_TransformWorldToLocal_Vector(Ray.Direction, InstanceIndex);
    RayScaler = length(LocalDirection);
    LocalRay.TMin = Ray.TMin * RayScaler;
    LocalRay.TMax = Ray.TMax * RayScaler;
    LocalRay.Direction = normalize(LocalDirection);
    return LocalRay;
}

// 256 instance / 24 (1600 w) gaussians
uint PackGaussianHit (uint InstanceIndex, uint GaussianIndex) {
    return (InstanceIndex << 24) | GaussianIndex;
}
void UnpackGaussianHit (uint Packed, out uint InstanceIndex, out uint GaussianIndex) {
    InstanceIndex = Packed >> 24;
    GaussianIndex = Packed & 0xFFFFFF;
}


Gaussian FetchGaussian (uint Index) {
    float3 Position = g_GaussianPositionBuffer[Index];
    float  Alpha    = g_GaussianAlphaBuffer[Index];
    float4 Rotation = g_GaussianRotationBuffer[Index];
    float3 Scale    = g_GaussianScaleBuffer[Index];
    Gaussian GaussianData = { Position, Alpha, Rotation, Scale};
    return GaussianData;
}

GaussianPBR FetchGaussianPBR (uint Index) {
    float3 Normal    = g_GaussianNormalBuffer[Index];
    float3 Albedo    = g_GaussianAlbedoBuffer[Index];
    float  Roughness = g_GaussianRoughnessBuffer[Index];
    GaussianPBR GaussianData = { Normal, Albedo, Roughness };
    return GaussianData;
}

SHCoefficents3 FetchGaussianSHCoefficients (int Index, int Degree) {
    SHCoefficents3 SHData = (SHCoefficents3) 0;
    SHData.Low.Color = g_GaussianColorBuffer[Index];
    if(Degree >= 1) {
        SHData.Low.SH1[0] = g_GaussianSH1Buffer[Index * 3 + 0];
        SHData.Low.SH1[1] = g_GaussianSH1Buffer[Index * 3 + 1];
        SHData.Low.SH1[2] = g_GaussianSH1Buffer[Index * 3 + 2];
        if(Degree >= 2) {
            SHData.Low.SH2[0] = g_GaussianSH2Buffer[Index * 5 + 0];
            SHData.Low.SH2[1] = g_GaussianSH2Buffer[Index * 5 + 1];
            SHData.Low.SH2[2] = g_GaussianSH2Buffer[Index * 5 + 2];
            SHData.Low.SH2[3] = g_GaussianSH2Buffer[Index * 5 + 3];
            SHData.Low.SH2[4] = g_GaussianSH2Buffer[Index * 5 + 4];
            if(Degree >= 3) {
                SHData.SH3[0] = g_GaussianSH3Buffer[Index * 7 + 0];
                SHData.SH3[1] = g_GaussianSH3Buffer[Index * 7 + 1];
                SHData.SH3[2] = g_GaussianSH3Buffer[Index * 7 + 2];
                SHData.SH3[3] = g_GaussianSH3Buffer[Index * 7 + 3];
                SHData.SH3[4] = g_GaussianSH3Buffer[Index * 7 + 4];
                SHData.SH3[5] = g_GaussianSH3Buffer[Index * 7 + 5];
                SHData.SH3[6] = g_GaussianSH3Buffer[Index * 7 + 6];
            }
        }
    }
    return SHData;
}

// Evaluate a value from a standard 3d normal distribution.
float EvaluateNormalizedGaussian (float3 Position) {
    // The normalization factor is 1 / sqrt((2 * pi)^3)
    const float NormalizationFactor = float(1 / sqrt(8 * M_PI * M_PI * M_PI));
    return NormalizationFactor * exp(-0.5f * dot(Position, Position));
}

// Evaluate the alpha value of a gaussian at a given position
float EvaluateGaussian (Gaussian GaussianData, float3 Position) {
    // x - mu:
    float3 Delta = Position - GaussianData.Position;
    // Transform to gaussian-local space
    float3 RotatedPosition = QuaternionInverseRotate(Delta, GaussianData.Rotation);
    float3 ScaledPosition = float3(
        RotatedPosition.x / GaussianData.Scale.x,
        RotatedPosition.y / GaussianData.Scale.y,
        RotatedPosition.z / GaussianData.Scale.z
    );
    // Evaluate the normalized gaussian
    float Value = EvaluateNormalizedGaussian(ScaledPosition);
    return Value * GaussianData.Alpha;
}

struct SymmetricMatrix {
    float3 Diagonal;
    float3 OffDiagonal;
};

float3x3 ExpandSymmetricMatrix (SymmetricMatrix C) {
    float3x3 MC = float3x3(
        C.Diagonal.x, C.OffDiagonal.x, C.OffDiagonal.y,
        C.OffDiagonal.x, C.Diagonal.y, C.OffDiagonal.z,
        C.OffDiagonal.y, C.OffDiagonal.z, C.Diagonal.z
    );
    return MC;
}

// Perform frustrum culling (copy pasted from original 3dgs impl)
bool IsInFrustrum (
	float3 Position,
	float4x4 View,
	float4x4 Projection,
	out float3 ViewSpacePosition)
{
	// Bring points to screen space
    float4 ViewW = mul(View, float4(Position, 1.0f));
    
    ViewSpacePosition = ViewW.xyz;
	
    float4 Homogeneous = mul(Projection, ViewW);
	float  InvW = 1.f / (Homogeneous.w + 1e-7f);
	float3 Projected = Homogeneous.xyz * InvW;

    float Tolerance = 0.2f;
    float ZNear = 0.2f;

    // Check if the point is inside the frustrum
    if(any(Projected.xy < -1.0f - Tolerance) || any(Projected.xy > 1.0f + Tolerance) 
    // Make sure that z is within 0 and 1
    || Projected.z < ZNear || Projected.z >= 1.f)
    {
        return false;
    }
	return true;
}

bool IsPointInFrustrum (CameraDescription C, float3 Position, out float3 ViewSpacePosition, bool Ortho = false, float NearClip = 0.f, float Expand = 0.f) {
    ViewSpacePosition = mul(C.View, float4(Position, 1.0f)).xyz;
    // -z axis is aligned with camera direction
    if(ViewSpacePosition.z >= -NearClip) return false;
    float4 Homogeneous = mul(C.Projection, float4(ViewSpacePosition, 1.0f));
    if(Ortho) {
        // FIXME
        return false;
    } else {
        if(Homogeneous.w > 0) {
            float3 Projected = Homogeneous.xyz / Homogeneous.w;
            return all(abs(Projected.xy) < 1.0f + Expand) && Projected.z >= 0.15 && Projected.z <= 1.0f;
        } else {
            return false;
        }
    }
}

SymmetricMatrix ComputeCovarianceMatrix (float3 Scale, float4 Rotation) {
    float3x3 M = GetRotationScaleTransform(Rotation, Scale);
    // Covariance matrix
    float3x3 Covariance = mul(M, transpose(M));

    float3 Diagonal = float3(
        Covariance[0][0],
        Covariance[1][1],
        Covariance[2][2]
    );
    float3 OffDiagonal = float3(
        Covariance[0][1],
        Covariance[0][2],
        Covariance[1][2]
    );
    SymmetricMatrix Ret = (SymmetricMatrix)0;
    Ret.Diagonal = Diagonal;
    Ret.OffDiagonal = OffDiagonal;
    return Ret;
}

// Project the covariance matrix to 2D
// @return The screen space 2D covariance matrix (m00, m01, m11)
float3 ProjectCovarianceMatrixToScreen(float3 mean, float2 focal, float2 tan_fov, SymmetricMatrix Covariance3D, float4x4 View)
{
    float3 t = mul(View, float4(mean, 1.0)).xyz;
    const float limx = 0.65f * tan_fov.x;
    const float limy = 0.65f * tan_fov.y;
    const float txtz = t.x / t.z;
    const float tytz = t.y / t.z;
    // Clamp to (fov expanded) frustum
    t.x = clamp(txtz, -limx, limx) * t.z;
    t.y = clamp(tytz, -limy, limy) * t.z;

    float3x3 J = float3x3(
        focal.x / t.z, 0.0f, -(focal.x * t.x) / (t.z * t.z),
        0.0f, focal.y / t.z, -(focal.y * t.y) / (t.z * t.z),
        0.0f, 0.0f, 0.0f);

    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        View[2][0], View[2][1], View[2][2]);

    float3x3 Mk = mul(J, W);
    
    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));

    return float3(C_2D[0][0], C_2D[0][1], C_2D[1][1]);
}

// TanFoV: 2 * tan(fov / 2)
// @return The first two rows of Jacobian Matrix.
float3x3 EWAJacobian2 (float3 Mean, float2 TanFoV, float4x4 View) {
    float3 P = mul(View, float4(Mean, 1.0)).xyz;
    const float limx = 0.65f * TanFoV.x;
    const float limy = 0.65f * TanFoV.y;
    const float txtz = P.x / P.z;
    const float tytz = P.y / P.z;
    // Clamp to (fov expanded) frustum
    P.x = clamp(txtz, -limx, limx) * P.z;
    P.y = clamp(tytz, -limy, limy) * P.z;

    float3x3 J = float3x3(
        (2 / TanFoV.x) / P.z, 0.0f, - (2 / TanFoV.x) * P.x / (P.z * P.z),
        0.0f, (2 / TanFoV.y) / P.z, - (2 / TanFoV.y) * P.y / (P.z * P.z),
        0.0f, 0.0f, 0.0f);
    return J;
}

float3 ProjectCovarianceMatrixToNDC(float3x3 J, SymmetricMatrix Covariance3D, float4x4 View)
{
    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        View[2][0], View[2][1], View[2][2]);
    // W = transpose(W);

    float3x3 Mk = mul(J, W);
    // Mk = mul(W, J);
    // Mk = transpose(Mk);
    
    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));

    return float3(C_2D[0][0], C_2D[0][1], C_2D[1][1]);
}

struct RayToTrace {
    float3 Direction;
    float3 Origin;
    float  RayTMin;
    float  RayTMax;
    // Specifies if at least 1 hit has been found
    bool   bHit;
    // Ray seed when performing stochastic operations
    float  Seed;
};

// Initialize the ray to trace struct
RayToTrace InitRayToTrace (float RayTMax) {
    RayToTrace Ray = (RayToTrace)0;
    Ray.RayTMin = 0.0f;
    Ray.RayTMax = RayTMax;
    return Ray;
}

// Fetch the ray to trace by its index
RayToTrace FetchRayToTrace (int RayIndex, float RayTMax, bool bFetchOrigin = true) {
    RayToTrace Ray = (RayToTrace)0;
    Ray.Direction = Octahedron01ToUnitVector(UnpackUnorm16x2(g_RWRayToTraceDirectionBuffer[RayIndex]));
    if(bFetchOrigin) Ray.Origin    = g_RWRayToTraceOriginBuffer[RayIndex];
    uint Flags    = g_RWRayToTraceFlagsBuffer[RayIndex];
    Ray.RayTMax   = RayTMax;
    Ray.RayTMin   = asfloat(Flags & RAY_FLAG_TMIN_MASK);
    Ray.bHit      = (Flags & RAY_FLAG_HIT_BIT) != 0;
    Ray.Seed      = g_RWRayToTraceSeedBuffer[RayIndex];
    return Ray;
}

// Write the ray to trace by its index
void WriteRayToTrace (int RayIndex, RayToTrace Ray, bool bWriteOrigin = true) {
    g_RWRayToTraceDirectionBuffer[RayIndex] = PackUnorm16x2(UnitVectorToOctahedron01(Ray.Direction));
    if(bWriteOrigin) g_RWRayToTraceOriginBuffer[RayIndex] = Ray.Origin;
    uint Flags = ((uint)Ray.bHit * RAY_FLAG_HIT_BIT) | asuint(abs(Ray.RayTMin));
    g_RWRayToTraceFlagsBuffer[RayIndex] = Flags;
    g_RWRayToTraceSeedBuffer[RayIndex] = Ray.Seed;
}

void WriteRayTraceFlags (int RayIndex, bool bHit, float TMin) {
    uint Flags = ((uint)bHit * RAY_FLAG_HIT_BIT) | asuint(abs(TMin));
    g_RWRayToTraceFlagsBuffer[RayIndex] = Flags;
}

float4 FetchRayTraceResult (int RayIndex) {
    return UnpackRadianceA16(g_RWRayToTraceResultBuffer[RayIndex]);
}

float FetchRayTraceHitT (int RayIndex) {
    uint Flags = g_RWRayToTraceFlagsBuffer[RayIndex];
    return asfloat(Flags & RAY_FLAG_TMIN_MASK);
}

// Compute the ray t to evaluate max respose of a ray-gaussian intersection according to the paper.
// @param Origin The origin of the ray
// @param Direction The direction of the ray
// @param G The gaussian to evaluate
// @return The ray t to evaluate the maximum response of the gaussian along the ray
float EvaluateGaussianResponseRayT (float3 Origin, float3 Direction, Gaussian G, inout float3x3 InvCov) {
    float3x3 InvScaleM = float3x3(
        1.0f / G.Scale.x, 0, 0,
        0, 1.0f / G.Scale.y, 0,
        0, 0, 1.0f / G.Scale.z
    );
    float3x3 R = EvaluateRotationMatrix(G.Rotation);
    InvCov = mul(InvScaleM, transpose(R));
    InvCov = mul(transpose(InvCov), InvCov);
    float3 Temp = mul(InvCov, Direction);
    float  Numerator  = dot(G.Position - Origin, Temp);
    float  Denominator = dot(Direction, Temp);
    return Numerator / max(Denominator, 1e-7f);
}

// Evaluate the gaussian response along the ray
float EvaluateGaussianResponse (float3 Origin, float3 Direction, Gaussian G, out float RayMaxResponseT) {
    float3x3 InvCov;
    RayMaxResponseT = EvaluateGaussianResponseRayT(Origin, Direction, G, InvCov);
    float3 Position = Origin + RayMaxResponseT * Direction;
    return exp(dot(G.Position - Position, mul(InvCov, Position - G.Position))) * G.Alpha;//EvaluateGaussian(G, Position);
}

// Map (film space) NDC to a direction in world space
float3 NDC2ToCameraDirectionUnnormalized (CameraDescription Camera, float2 NDC2) {
    float3 UnnormalizedDirection = Camera.Right * NDC2.x + Camera.Up * NDC2.y + Camera.Direction;
    return UnnormalizedDirection;
}

float3 NDC2ToCameraDirection (CameraDescription Camera, float2 NDC2) {
    return normalize(NDC2ToCameraDirectionUnnormalized(Camera, NDC2));
}

// Spawn a camera ray from the screen position
RayToTrace SpawnCameraRay (CameraDescription Camera, float2 ScreenPosition) {
    RayToTrace Ray = InitRayToTrace(UB.RT_MaxTraceDistance);
    Ray.Origin = Camera.Position;
    float2 NDC2 = ScreenToNDC2(ScreenPosition);
    float3 DirectionUnnormalized = NDC2ToCameraDirectionUnnormalized(Camera, NDC2);
    float  Length = length(DirectionUnnormalized);
    Ray.RayTMin = Length * Camera.NearPlane;
    Ray.RayTMax = Length * Camera.FarPlane;
    Ray.Direction = normalize(DirectionUnnormalized);
    return Ray;
}

uint PackPadScreenPosition (float2 ScreenPosition, float2 InvScreenDimensions) {
    // Scale and pad the borders of the screen space position to make sure it's in the range [0, 1]
    float2 ScaledScreenPosition = saturateDown(.5f * ScreenPosition * InvScreenDimensions + 0.25f);
    return PackUnorm16x2(ScaledScreenPosition);
}

bool IsOutOfFilm(CameraDescription C, int2 FilmPosition) {
    return any(FilmPosition < 0) || any(FilmPosition >= C.FilmDimensions);
}
bool IsOutOfFilm(CameraDescription C, uint2 FilmPosition) {
    return any(FilmPosition < 0) || any(FilmPosition >= C.FilmDimensions);
}

float3 DebugColorTable (int Index) {
    const float3 _ColorTable[15] = {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1),
        float3(1, 1, 0),
        float3(1, 0, 1),
        float3(0, 1, 1),
        float3(1, 0.5, 0),
        float3(0, 1, 0.5),
        float3(0.5, 0, 1),
        float3(1, 0, 0.5),
        float3(0, 0.5, 1),
        float3(0.5, 1, 0),
        float3(1, 0.5, 0.5),
        float3(0.5, 1, 0.5),
        float3(0.5, 0.5, 1)
    };
    return _ColorTable[Index % 15];
}

// Gives a heatmap about h in [0, 1]
float3 DebugColorHeatMap (float h) {
    float H = saturate(1.0f - h) * 5.0f;
    float R = saturate(min(H - 1.5f, 4.5f - H));
    float G = saturate(min(H - 0.5f, 3.5f - H));
    float B = saturate(min(H + 0.5f, 2.5f - H));
    return float3(R, G, B);
}

// Get the current active camera description
CameraDescription GetCameraDescription () {
    // TODO meshcards
    return UB.MainCamera;
}

Texture2D<float2> GetDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GDepthTexture;
}

RWTexture2D<float2> GetRWDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GDepthTexture;
}

Texture2D<float4> GetColorTexture (CameraDescription C) {
    // TODO meshcards
    return g_GColorTexture;
}

RWTexture2D<float4> GetRWColorTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GColorTexture;
}

Texture2D<float4>  GetNormalTexture (CameraDescription C) {
    // TODO meshcards
    return g_GNormalTexture;
}

float3 GetTexelNormalFromTextureUV (Texture2D<float4> NormalTexture, float2 UV) {
    return normalize(NormalTexture.SampleLevel(g_PointClampSampler, UV, 0.0f).xyz * 2.0f - 1.0f);
}

RWTexture2D<float4> GetRWNormalTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GNormalTexture;
}

Texture2D<float> GetFilteredDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GFilteredDepthTexture;
}

RWTexture2D<float> GetRWFilteredDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GFilteredDepthTexture;
}

Texture2D<float> GetZDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GZDepthTexture;
}

RWTexture2D<float> GetRWZDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GZDepthTexture;
}

Texture2D<float> GetHistoryZDepthTexture (CameraDescription C) {
    return g_HistoryZDepthTexture;
}

Texture2D<float> GetNearHZBTexture (CameraDescription C) {
    return g_NearHZBTexture;
}

Texture2D<float4> GetRadianceTexture (CameraDescription C) {
    // TODO meshcards
    return g_Radiance;
}

RWTexture2D<float4> GetRWRadianceTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_Radiance;
}

Texture2D<float4> GetHistoryRadianceTexture (CameraDescription C) {
    return g_HistoryRadiance;
}

Texture2D<float4> GetDirectIlluminationTexture (CameraDescription C) {
    // TODO meshcards
    return g_DirectIllumination;
}

RWTexture2D<float4> GetRWDirectIlluminationTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_DirectIllumination;
}

Texture2D<float4> GetHistoryDirectIlluminationTexture (CameraDescription C) {
    // TODO meshcards
    return g_HistoryDirectIllumination;
}

Texture2D<float4> GetFilteredDirectIlluminationTexture (CameraDescription C) {
    // TODO meshcards
    return g_FilteredDirectIllumination;
}

RWTexture2D<float4> GetRWFilteredDirectIlluminationTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_FilteredDirectIllumination;
}


float3 RecoverWorldSpacePositionNDC2 (CameraDescription C, float2 NDC2, float LinearDepth) {
    float3 Direction = NDC2ToCameraDirectionUnnormalized(C, NDC2);
    float3 Origin = C.Position;
    float3 WorldSpacePosition = Origin + LinearDepth * Direction;
    return WorldSpacePosition;
}

float3 RecoverWorldSpacePosition (CameraDescription C, float2 FilmPosition, float LinearDepth) {
    float2 NDC2 = FilmToNDC2(C, FilmPosition);
    return RecoverWorldSpacePositionNDC2(C, NDC2, LinearDepth);
}

float LinearToZDepth (CameraDescription C, float LinearDepth) {
    float A = C.Projection[2][2];
    float B = C.Projection[2][3];
    return (-A * LinearDepth + B) / LinearDepth;
}

float ZDepthToLinear (CameraDescription C, float ZDepth) {
    float ZDepthN = 2.0 * ZDepth - 1.0;
    return 2.f * C.NearPlane * C.FarPlane / (C.FarPlane + C.NearPlane - ZDepthN * (C.FarPlane - C.NearPlane));
}

float3 GetSkyBoxDirection (int i) {
    float3 Directions[6] = {
        float3(1, 0, 0),
        float3(-1, 0, 0),
        float3(0, 1, 0),
        float3(0, -1, 0),
        float3(0, 0, 1),
        float3(0, 0, -1)
    };
    return Directions[i];
}

float3 EvaluateSkyRadiance (float3 Direction, float LOD = 0) {
    return g_EnvironmentMap.SampleLevel(g_LinearWrapSampler, Direction, LOD).xyz;
}

// Copy-pasted from UE5. A simple and fast way to get an interleaved gradient noise.

// high frequency dither pattern appearing almost random without banding steps
//note: from "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
//      http://advances.realtimerendering.com/s2014/index.html
// Epic extended by FrameId
// ~7 ALU operations (2 frac, 3 mad, 2 *)
// @return 0..1
float InterleavedGradientNoise( float2 uv, float FrameId )
{
	// magic values are found by experimentation
	uv += FrameId * (float2(47, 17) * 0.695f);

    const float3 magic = float3( 0.06711056f, 0.00583715f, 52.9829189f );
    return frac(magic.z * frac(dot(uv, magic.xy)));
}

float3 ReprojectToPreviousUVWFromUVW (CameraDescription C, float3 UVW) {
    float3 NDC = float3(UVToNDC2(UVW.xy), UVW.z);
    float3 ReprojectedNDC = TransformPointWithPerspectiveDivide(C.Reprojection, NDC);
    return float3(NDC2ToUV(ReprojectedNDC.xy), ReprojectedNDC.z);
}

float3 ReprojectToPreviousNDCFromNDC (CameraDescription C, float3 NDC) {
    return TransformPointWithPerspectiveDivide(C.Reprojection, NDC);
}

bool IsMaterialValid (Material M) {
    return M.Alpha >= UB.OpaqueThreshold;
}

float3 SH3Evaluate(float3 ViewDirection, SHCoefficents3 SH3, int Degree)
{
	const float SH_C0 = 0.28209479177387814f;
	const float SH_C1 = 0.4886025119029199f;
	const float SH_C2[] = {
		1.0925484305920792f,
		-1.0925484305920792f,
		0.31539156525252005f,
		-1.0925484305920792f,
		0.5462742152960396f
	};
	const float SH_C3[] = {
		-0.5900435899266435f,
		2.890611442640554f,
		-0.4570457994644658f,
		0.3731763325901154f,
		-0.4570457994644658f,
		1.445305721320277f,
		-0.5900435899266435f
	};
	// The SH stored in the gaussians is "flipped" compared to ordinary computer graphics
	// conventions.
	float3 NViewDirection = -ViewDirection;

	float3 result = SH_C0 * SH3.Low.Color;

	if(Degree >= 1) {
		float x = NViewDirection.x;
		float y = NViewDirection.y;
		float z = NViewDirection.z;
		result = result - SH_C1 * y * SH3.Low.SH1[0] + SH_C1 * z * SH3.Low.SH1[1] - SH_C1 * x * SH3.Low.SH1[2];

		if(Degree >= 2) {
			float xx = x * x, yy = y * y, zz = z * z;
			float xy = x * y, yz = y * z, xz = x * z;
			result = result +
				SH_C2[0] * xy * SH3.Low.SH2[0] +
				SH_C2[1] * yz * SH3.Low.SH2[1] +
				SH_C2[2] * (2.0f * zz - xx - yy) * SH3.Low.SH2[2] +
				SH_C2[3] * xz * SH3.Low.SH2[3] +
				SH_C2[4] * (xx - yy) * SH3.Low.SH2[4];

			if(Degree >= 3) {
				result = result +
					SH_C3[0] * y * (3.0f * xx - yy) * SH3.SH3[0] +
					SH_C3[1] * xy * z * SH3.SH3[1] +
					SH_C3[2] * y * (4.0f * zz - xx - yy) * SH3.SH3[2] +
					SH_C3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy) * SH3.SH3[3] +
					SH_C3[4] * x * (4.0f * zz - xx - yy) * SH3.SH3[4] +
					SH_C3[5] * z * (xx - yy) * SH3.SH3[5] +
					SH_C3[6] * x * (xx - 3.0f * yy) * SH3.SH3[6];
			}
		}
	}
	// Seems that 3DGS padded the SH (maybe this made optimization easier?)
	result += 0.5f;
	return max(result, 0.0f);
}

#endif // INC_3DGS_HLSL