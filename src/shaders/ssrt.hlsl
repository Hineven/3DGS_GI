#include "3dgs.hlsl"

// Copy-pasted from UE
/*
* Clips a ray to an AABB.  Does not handle rays parallel to any of the planes.
*
* @param RayOrigin - The origin of the ray in world space.
* @param RayEnd - The end of the ray in world space.  
* @param BoxMin - The minimum extrema of the box.
* @param BoxMax - The maximum extrema of the box.
* @return - Returns the closest intersection along the ray in x, and furthest in y.  
*			If the ray did not intersect the box, then the furthest intersection <= the closest intersection.
*			The intersections will always be in the range [0,1], which corresponds to [RayOrigin, RayEnd] in worldspace.
*			To find the world space position of either intersection, simply plug it back into the ray equation:
*			WorldPos = RayOrigin + (RayEnd - RayOrigin) * Intersection;
*/
float2 LineBoxIntersect(float3 RayOrigin, float3 RayEnd, float3 BoxMin, float3 BoxMax)
{
	float3 InvRayDir = 1.0f / (RayEnd - RayOrigin);
	
	//find the ray intersection with each of the 3 planes defined by the minimum extrema.
	float3 FirstPlaneIntersections = (BoxMin - RayOrigin) * InvRayDir;
	//find the ray intersection with each of the 3 planes defined by the maximum extrema.
	float3 SecondPlaneIntersections = (BoxMax - RayOrigin) * InvRayDir;
	//get the closest of these intersections along the ray
	float3 ClosestPlaneIntersections = min(FirstPlaneIntersections, SecondPlaneIntersections);
	//get the furthest of these intersections along the ray
	float3 FurthestPlaneIntersections = max(FirstPlaneIntersections, SecondPlaneIntersections);

	float2 BoxIntersections;
	//find the furthest near intersection
	BoxIntersections.x = max(ClosestPlaneIntersections.x, max(ClosestPlaneIntersections.y, ClosestPlaneIntersections.z));
	//find the closest far intersection
	BoxIntersections.y = min(FurthestPlaneIntersections.x, min(FurthestPlaneIntersections.y, FurthestPlaneIntersections.z));
	//clamp the intersections to be between RayOrigin and RayEnd on the ray
	return saturate(BoxIntersections);
}

// Modified from UE5's SSRT implementation.
// Conventions are consistent with my codebase.
void ScreenSpaceRayTrace (
    CameraDescription C,
    Texture2D<float> ZDepthTexture,
    Texture2D<float> NearHZBTexture,
    float3 RayWorldOrigin,
    float3 RayWorldDirection,
    float  MaxTraceDistance,
    int    MaxNumIterations,
    float  RelTexelThickness,
    int    MinWarpOccupancy,
    inout  bool   bHit,                   // If a trustworthy hit is found
    inout  float3 OutHitUVZ,
    inout  float3 OutLastVisibleUVZ,
    inout  float  OutHitTileZ
) {
    // UV + Linear Depth
    float3 RayStartUVW;
    {
        float3 Homogeneous = TransformPointWithPerspectiveDivide(C.ProjectionView, RayWorldOrigin);
        RayStartUVW = float3(NDC2ToUV(Homogeneous.xy), Homogeneous.z);
    }
    // UV + Linear Depth
    float3 RayEndUVW;
    {
        float3 ViewRayDirection  = TransformVector(ClipMatrix(C.View), RayWorldDirection);
        float  NOriginLinearDepth = mul(C.View, float4(RayWorldOrigin, 1.f)).z;
        // Clamp the ray end to the near plane, avoid bad homogenous coordinates
		float RayEndWorldDistance = ViewRayDirection.z > 0.0 
            ? min(0.99f * (- NOriginLinearDepth - C.NearPlane) / ViewRayDirection.z, MaxTraceDistance)
            : MaxTraceDistance;

		float3 RayWorldEnd = RayWorldOrigin + RayWorldDirection * RayEndWorldDistance;
        float3 Homogeneous = TransformPointWithPerspectiveDivide(C.ProjectionView, RayWorldEnd);
		RayEndUVW = float3(NDC2ToUV(Homogeneous.xy), Homogeneous.z);

		float2 ScreenEdgeIntersections = LineBoxIntersect(RayStartUVW, RayEndUVW, 0.xxx, 1.xxx);

		// Recalculate end point where it leaves the screen
		RayEndUVW = RayStartUVW + (RayEndUVW - RayStartUVW) * ScreenEdgeIntersections.y;
    }

    float3 RayDirectionUVW = RayEndUVW - RayStartUVW;
    // Offset to pick which XY boundary planes to intersect
    //  First Texel
    // +------------+ << this boundary is tested agains first.
    // |   \        |
    // |    \       |
    // |     x      | << ray origin
    // |            |
    // +------------+

    float2 FloorOffset = select(RayDirectionUVW.xy < 0, 0.0, 1.0);

    // Tracing states
    int    MipLevel = -1; // -1 stands for the full resolution depth buffer
	float  CurrentT = 0;  // Ray T
	float3 CurrentUVW = RayStartUVW;

	// Step out of current tile (HZB texel) without hit test to avoid self-intersection
	bool bStepOutOfCurrentTile = true;

	if (bStepOutOfCurrentTile)
	{
		float MipLevelForStepOut = MipLevel;
		float2 CurrentMipTexelSize = exp2(MipLevelForStepOut) * C.HZBBaseTexelSize;
		float2 CurrentMipResolution = 1.0f / CurrentMipTexelSize;

        // Go a little further from the current texel
		float2 UVOffset = .005f * CurrentMipTexelSize;
		UVOffset = select(RayDirectionUVW.xy < 0, -UVOffset, UVOffset);

		float2 XYPlane = floor(CurrentUVW.xy * CurrentMipResolution) + FloorOffset;
		XYPlane = XYPlane * CurrentMipTexelSize + UVOffset;
		
		float2 PlaneIntersections = (XYPlane - RayStartUVW.xy) / RayDirectionUVW.xy;
		CurrentT   = min(PlaneIntersections.x, PlaneIntersections.y);
		CurrentUVW = RayStartUVW + CurrentT * RayDirectionUVW;
	}


    int Iteration = 0;
	bHit = false;
	OutHitTileZ = 0;

	float LastAboveSurfaceT = CurrentT;

	// Stackless HZB traversal
	while (MipLevel >= -1
		&& Iteration < MaxNumIterations 
		&& CurrentT < 1.0f
#if SSRT_TERMINATE_ON_LOW_OCCUPANCY
		&& WaveActiveCountBits(true) > MinimumTracingThreadOccupancy
#endif
		)
	{
		float2 CurrentMipTexelSize = exp2(MipLevel) * C.HZBBaseTexelSize;
		float2 CurrentMipResolution = 1.0f / CurrentMipTexelSize;

		float2 UVOffset = .005f * CurrentMipTexelSize;
		UVOffset = select(RayDirectionUVW.xy < 0, -UVOffset, UVOffset);

		float2 XYPlane = floor(CurrentUVW.xy * CurrentMipResolution) + FloorOffset;
		XYPlane = XYPlane * CurrentMipTexelSize + UVOffset;

		float TileZ;

		if(MipLevel < 0) {
            // Sample from full resolution depth buffer
            TileZ = ZDepthTexture.SampleLevel(g_PointClampSampler, CurrentUVW.xy, 0).r;
        } else {
            // Sample from HZB
			TileZ = NearHZBTexture.SampleLevel(g_PointClampSampler, CurrentUVW.xy, MipLevel).r;
		}

		float3 BoundaryPlanes = float3(XYPlane, TileZ);

		float3 PlaneIntersections = (BoundaryPlanes - RayStartUVW) / RayDirectionUVW;
        // Do not intersect with the Z-plane when the ray is heading toward the camera (may miss a closer hit)
		PlaneIntersections.z = RayDirectionUVW.z > 0 ? PlaneIntersections.z : 1.0f;
        // Ray T used to update the current T
		float  UpdateT = min(PlaneIntersections.x, PlaneIntersections.y);

		bool bAboveSurface = CurrentUVW.z < TileZ;
		bool bSkippedTile  = bAboveSurface;

        UpdateT = min(UpdateT, PlaneIntersections.z);
        bSkippedTile &= UpdateT != PlaneIntersections.z;

		if (bSkippedTile)
		{
			LastAboveSurfaceT = UpdateT;
		}
		
		CurrentT   = bAboveSurface ? UpdateT : CurrentT;
		CurrentUVW = RayStartUVW + min(CurrentT, 1.0f) * RayDirectionUVW;
		MipLevel  += bSkippedTile ? 1 : -1;

		Iteration ++;
	}

    // Somehow went below the surface
	if (MipLevel < -1 && CurrentT < 1.0f)
	{
		float TileZ = ZDepthTexture.SampleLevel(g_PointClampSampler, CurrentUVW.xy, 0).r;

		OutHitTileZ = TileZ;

		float HitLinearDepth = ZDepthToLinear(C, TileZ);
		float CurLinearDepth = ZDepthToLinear(C, CurrentUVW.z);

		bHit = (CurLinearDepth - HitLinearDepth) < RelTexelThickness * max(HitLinearDepth, .00001f);

		if (!bHit)
		{
			// We went below the surface and couldn't count it as a hit, rewind to the last time we were above
			CurrentUVW = RayStartUVW + LastAboveSurfaceT * RayDirectionUVW;
		}
	}

	OutHitUVZ = CurrentUVW;
	float3 LastVisibleUVW = RayStartUVW + LastAboveSurfaceT * RayDirectionUVW;
	OutLastVisibleUVZ = LastVisibleUVW;
}