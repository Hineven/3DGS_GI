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
                          -0.5f, -0.5f, 0.0f,
                        0.5f, -0.5f, 0.0f,
                        0.0f,  0.7f, 0.0f,
                        -0.5f, -0.5f, 0.0f };
    auto vertex_buffer = gfxCreateBuffer(gfx, sizeof(vertices), vertices);

    auto program = gfxCreateProgram(gfx, "../test/triangle");
    GfxDrawState state = {};
//    gfxDrawStateSetDepthWriteMask(state, D3D12_DEPTH_WRITE_MASK_ZERO);
//    gfxDrawStateSetDepthStencilTarget(state, DXGI_FORMAT_UNKNOWN);
//    gfxDrawStateSetDepthFunction(state, D3D12_COMPARISON_FUNC_ALWAYS);
    gfxDrawStateEnableAlphaBlending(state);
    gfxDrawStateSetDepthStencilTarget(state, DXGI_FORMAT_UNKNOWN);
    gfxDrawStateSetCullMode(state, D3D12_CULL_MODE_NONE);
    gfxDrawStateSetBlendMode(state,
             D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
             D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_OP_MAX);
    auto kernel = gfxCreateGraphicsKernel(gfx, program, state);

    for(float time = 0.0f; !gfxWindowIsCloseRequested(window); time += 0.1f)
    {

        gfxWindowPumpEvents(window);
        gfxCommandClearBackBuffer(gfx);

        gfxCommandBindKernel(gfx, kernel);
        gfxCommandBindVertexBuffer(gfx, vertex_buffer);

        gfxCommandDraw(gfx, 3, 2);


        // gfxCommandDraw(gfx, 3, 1, 0, 1);

        gfxFrame(gfx);
    }

    gfxDestroyContext(gfx);
    gfxDestroyWindow(window);

    return 0;
}
