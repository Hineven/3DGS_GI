# Dynamic Global Illumination for Interactive Gaussian Splatting Scenes in Real Time
_Paper source code_

This is an implementation of a simple RTGI pipeline for interactive relighting of mixed scenes including 3D Gaussians and mesh models.
The implementation is minimalistic. It does not include many possible optimizations or denoising techniques, neither a proper material system.
It's meant to explore proper 3D Gaussian integration techniques and validate the potential of using 3D Gaussians within applications requiring real-time dynamic global illumination. 

## Requirements

Compilable with `cmake` and MSVC (VS2022) on Windows 10/11 (tested).

Requires NVIDIA RTX 30 series or newer GPUs to run. Based on DX12 and DXR (which should be supported on modern hardware).

**Incompatible with AMD and Intel GPUs. It assumes a fixed wave / warp / subgroup size of 32.**

## Compile and run
Executable CMake target name: `3DGS_AdvGI`
Modify the scene loading logic inside `app_internal.cpp` to load different 3D Gaussians / Gltf scenes.

**NOTE**: Only 3D Gaussians with PBR material paramters are supported for relighting. We use [Relightable 3DGS](https://github.com/NJU-3DV/Relightable3DGaussian) to produce 3D Gaussians with PBR features.
Other inverse rendering frameworks may also work.

## Sample Data
_coming soon_

## Open source libraries / code used
* [gfx](https://github.com/gboisse/gfx)
* [glm](https://github.com/icaven/glm)
* half
* [happly](https://github.com/nmwsharp/happly)
* [Capsaicin](https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin) : some of the code are borrowed and modified from Capsaicin.