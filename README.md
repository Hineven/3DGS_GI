# RTGI for Dynamic 3DGS Scenes Code
Compile with cmake, tested on windows 10 with visual studio 2022.
Requires NVIDIA RTX 30 series or newer GPU to run. 

**Incompatible with AMD and Intel GPUs.**

## Compile and run
Executable name: `3DGS_AdvGI`
Modify the scene loading logic inside `app_internal.cpp` to load different 3D Gaussians / Gltf scenes.

**NOTE**: Only 3D Gaussians with PBR material paramters are supported for relighting. We use Relightable 3DGS to produce 3D Gaussians with PBR features.
Other inverse rendering frameworks may also work.

## Open source libraries used
* gfx
* glm
* half
* happly