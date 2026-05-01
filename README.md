# Dynamic Global Illumination for Interactive Gaussian Splatting Scenes in Real Time
_Paper source code, [paper link](https://arxiv.org/abs/2503.17897)_

![Cover Image](cover.png)
This is an implementation of a simple RTGI pipeline for interactive relighting of mixed scenes including 3D Gaussians and mesh models.
The implementation is minimalistic. It does not include many possible optimizations or denoising techniques, neither a proper material system.
It's meant to explore proper 3D Gaussian integration techniques and validate the potential of using 3D Gaussians within applications requiring real-time dynamic global illumination. 

## Requirements

Compilable with `cmake` and MSVC (VS2022) on Windows 10/11.

Requires NVIDIA RTX 30 series or newer GPUs to run. Based on DX12 and DXR (which should be supported on modern hardware).

**Incompatible with AMD and Intel GPUs. It assumes a fixed wave / warp / subgroup size of 32.**

## Compile and run
Executable CMake target name: `3DGS_AdvGI`
Modify the scene loading logic inside `app_internal.cpp` to load different 3D Gaussians / Gltf scenes.

**NOTE**: Only 3D Gaussians with PBR material parameters are "good" for relighting. We use [Relightable 3DGS](https://github.com/NJU-3DV/Relightable3DGaussian) to produce 3D Gaussians with PBR features.
Other inverse rendering frameworks may also work.

**NOTE2**: While loading 3D Gaussians without proper material parameters for relighting, they are empirically converted. Most of the time the results are bad.
Remember to switch on "Reconstruct Normals" option for those models to always use fallback normals when relighting.

## Sample Data
The project relies on PBR-featured 3D Gaussian models produced by [Relightable 3DGS](https://github.com/NJU-3DV/Relightable3DGaussian). Download the data here: [Data](https://drive.google.com/file/d/1e6XrQieyw-NMTD3-SeuyZHT4-DW65nEl/view?usp=sharing)

Unpack the data and place the `data` folder in the root directory of the project. 

Directory structure:
```
/3DGS_AdvGI
    /data
        /air_baloons
            /point_cloud
                /iteration_50000
        ...
    /ext
    /src
    CMakeLists.txt
    ...
```
The program expects to find the data folder in the root directory.

## Open source libraries / code used
* [gfx](https://github.com/gboisse/gfx)
* [glm](https://github.com/icaven/glm)
* half
* [happly](https://github.com/nmwsharp/happly)
* [Capsaicin](https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin) : some of the code are borrowed and modified from Capsaicin.
