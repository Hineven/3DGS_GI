/*
 * Created: 2024/11/30
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include "bluenoise.h"
#include "sobol_samples.h" // Separate file as does not play well with IDEs

BlueNoiseSampler::BlueNoiseSampler(GfxContext gfx) noexcept {
    gfx_ = gfx;
}

BlueNoiseSampler::~BlueNoiseSampler() noexcept
{
    Destroy();
}

bool BlueNoiseSampler::Initialize()
{
    sobol_buffer_           = gfxCreateBuffer(gfx_, sizeof(Sobol256x256), Sobol256x256);
    ranking_tile_buffer_    = gfxCreateBuffer(gfx_, sizeof(RankingTiles), RankingTiles);
    scrambling_tile_buffer_ = gfxCreateBuffer(gfx_, sizeof(ScramblingTiles), ScramblingTiles);
    return true;
}

void BlueNoiseSampler::Destroy()
{
    gfxDestroyBuffer(gfx_, sobol_buffer_);
    gfxDestroyBuffer(gfx_, ranking_tile_buffer_);
    gfxDestroyBuffer(gfx_, scrambling_tile_buffer_);
}

void BlueNoiseSampler::InstallParameters(GfxProgram program)
{
    gfxProgramSetParameter(gfx_, program, "g_SobolBuffer", sobol_buffer_);
    gfxProgramSetParameter(gfx_, program, "g_RankingTileBuffer", ranking_tile_buffer_);
    gfxProgramSetParameter(gfx_, program, "g_ScramblingTileBuffer", scrambling_tile_buffer_);
}
