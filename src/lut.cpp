/*
 * Created: 2024/12/25
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "renderer.h"

void Renderer::ComputeShadingLUT () {
    auto gfx = AppInternal::GetInstance().GetGfx();
    auto program = gfxCreateProgram(gfx, "src/shaders/brdf_lut/brdf_lut", AppInternal::GetInstance().GetRootPath().c_str());
    auto kernel = gfxCreateComputeKernel(gfx, program, "ComputeBrdfLut");
    gfxCommandBindKernel(gfx, kernel);

    gfxProgramSetParameter(gfx, program, "g_LutBuffer", tex_.shading_LUT);
    gfxProgramSetParameter(gfx, program, "g_LutSize", kernel);
    gfxProgramSetParameter(gfx, program, "g_SampleSize", 4096);

    // Compute BRDF LUT once in initialization
    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx, kernel);
    uint32_t const  num_groups_x = (32 + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (32 + num_threads[1] - 1) / num_threads[1];
    gfxCommandBindKernel(gfx, kernel);
    gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);

    gfxDestroyKernel(gfx, kernel);
    gfxDestroyProgram(gfx, program);
}