#ifndef RADIOMETRY_HLSL
#define RADIOMETRY_HLSL

// Map color to radiance (using a simple inverse gamma correction)
float3 ColorToRadiance (float3 Color, float Gamma = 2.2f) {
    return pow(Color, Gamma);
}

// Map radiance to color (using a simple gamma correction)
float3 RadianceToColor (float3 Radiance, float Gamma = 2.2f) {
    return pow(Radiance, 1.0f / Gamma);
}

// Calculate luminance (perceived brightness) from a radiance (not color)
float Luminance (float3 Radiance) {
    // TODO - This is a simple approximation of luminance
    // I failed find a better conversion...which may not be worthy of the effort.

    // Note: DO NOT use the "color to luminance" conversion widely accessible on the 
    // internet. COLOR IS NOT RADIANCE.
    // Maybe i should convert to color and then to luminance?
    return dot(Radiance, 0.3333333f.xxx);
}

#endif