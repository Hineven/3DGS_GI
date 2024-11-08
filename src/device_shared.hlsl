#ifndef 3DGS_ADVGI_DEVICE_SHARED_HLSL
#define 3DGS_ADVGI_DEVICE_SHARED_HLSL

// Dummy file for now. The shader path search is dependent on this file.

#ifndef M_PI
#define M_PI 3.14159265359f
#endif

#ifndef M_PIf
#define M_PIf 3.14159265359f
#endif

#ifdef __cplusplus


#include <glm/glm.hpp>
// Make hlsl compile with c++ compiler.
// I dont really care about project arch and maintainability.
// So just leave these typedefs inside root namespace.
typedef glm::vec2 float2;
typedef glm::vec3 float3;
typedef glm::vec4 float4;
typedef glm::mat3 float3x3;
typedef glm::mat4 float4x4;
typedef glm::mat3x4 float3x4;
typedef glm::uint uint;
typedef glm::uvec2 uint2;
typedef glm::uvec3 uint3;
typedef glm::uvec4 uint4;
typedef glm::ivec2 int2;
typedef glm::ivec3 int3;
typedef glm::ivec4 int4;

#endif

#endif