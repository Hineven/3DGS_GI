/*
 * Created: 2024/11/30
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef BLUENOISE_H
#define BLUENOISE_H

#include <gfx.h>

class BlueNoiseSampler {
public:
    BlueNoiseSampler(GfxContext gfx) noexcept;
    ~BlueNoiseSampler () ;
    bool Initialize ();
    void Destroy ();
    void InstallParameters(GfxProgram program);
protected:
    GfxContext gfx_;
    GfxBuffer sobol_buffer_;
    GfxBuffer ranking_tile_buffer_;
    GfxBuffer scrambling_tile_buffer_;
};

#endif //BLUENOISE_H
