/*
 * Created: 2025/1/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <fstream>
#include <stb_image_write.h>
#include "renderer.h"

void Renderer::SaveImage (std::string file_name, void * data, int width, int height) {
    std::ofstream file(file_name, std::ios::binary);
    if(!file.is_open()) {
        app_warning("Failed to open file for writing: " << file_name);
        return;
    }
    stbi_write_png_to_func(
            [](void * context, void * data, int size) {
                auto & file = *reinterpret_cast<std::ofstream *>(context);
                file.write(reinterpret_cast<char *>(data), size);
            },
            &file,
            width, height, 4, data, width * 4
    );
}