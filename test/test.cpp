/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "gfx_window.h"

int main()
{
    auto window = gfxCreateWindow(1280, 720, "gfx - Hello, triangle!");
    auto gfx = gfxCreateContext(window);

    float vertices[] = {  0.5f, -0.5f, 0.0f,
                          0.0f,  0.7f, 0.0f,
                         -0.5f, -0.5f, 0.0f };
    auto vertex_buffer = gfxCreateBuffer(gfx, sizeof(vertices), vertices);

    auto program = gfxCreateProgram(gfx, "../test/triangle");

    GfxTexture col_tex = gfxCreateTexture2D(gfx, 500, 500, DXGI_FORMAT_R8G8B8A8_UNORM);

    GfxDrawState state {};
    gfxDrawStateSetBlendMode(
        state, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD
    );
    auto kernel = gfxCreateGraphicsKernel(gfx, program, state);

    for(float time = 0.0f; !gfxWindowIsCloseRequested(window); time += 0.1f)
    {
        gfxWindowPumpEvents(window);

        float color[] = { 0.5f * cosf(time) + 0.5f,
                          0.5f * sinf(time) + 0.5f,
                          1.0f };
        gfxProgramSetParameter(gfx, program, "Color", color);

        gfxCommandBindKernel(gfx, kernel);
        gfxCommandBindVertexBuffer(gfx, vertex_buffer);

        gfxCommandDraw(gfx, 3);

        gfxFrame(gfx);
    }

    gfxDestroyContext(gfx);
    gfxDestroyWindow(window);

    return 0;
}
