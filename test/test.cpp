/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
//#include <filesystem>
//#include <iostream>
//#include "gfx_window.h"
//
//int main()
//{
//
//    // print working directory
//    std::cout << std::filesystem::current_path() << std::endl;
//
//    auto window = gfxCreateWindow(1280, 720);
//    auto gfx = gfxCreateContext(window);
//
//    float vertices[] = {  0.5f, -0.5f, 0.0f,
//                          0.0f,  0.7f, 0.0f,
//                          -0.5f, -0.5f, 0.0f };
//    auto vertex_buffer = gfxCreateBuffer(gfx, sizeof(vertices), vertices);
//
//    std::string shader_path = "../";
//
//    auto program = gfxCreateProgram(gfx, "test/triangle", shader_path.c_str());
//    auto kernel = gfxCreateGraphicsKernel(gfx, program);
//
//    // Check if the kernel is properly created
//    if (!kernel)
//    {
//        return 1;
//    }
//
//    for (auto time = 0.0f; !gfxWindowIsCloseRequested(window); time += 0.1f)
//    {
//        gfxWindowPumpEvents(window);
//
//        float color[] = { 0.5f * cosf(time) + 0.5f,
//                          0.5f * sinf(time) + 0.5f,
//                          1.0f };
//        gfxProgramSetParameter(gfx, program, "Color", color);
//
//        gfxCommandBindKernel(gfx, kernel);
//        gfxCommandBindVertexBuffer(gfx, vertex_buffer);
//
//        gfxCommandDraw(gfx, 3);
//
//        gfxFrame(gfx);
//    }
//
//    gfxDestroyContext(gfx);
//    gfxDestroyWindow(window);
//
//    return 0;
//}


#include <plycpp.h>
#include <filesystem>
#include <iostream>
#include <array>

int main()
{

    try
    {
        std::cout << "Loading PLY data..." << std::endl;
        plycpp::PLYData data;


        plycpp::load(std::string(MODELS_DIRECTORY) + "/bunny.ply", data);
        //plycpp::load(std::string(MODELS_DIRECTORY) + "/bunny_ascii.ply", data);

        // Listing PLY content
        {
            std::cout << "List of elements and properties:\n"
                      << "===========================" << std::endl;
            for (const auto& element : data)`
            {
                std::cout << "* " << element.key << " -- size: " << element.data->size() << std::endl;
                for (const auto& prop : element.data->properties)
                {
                    std::cout << "    - " << prop.key
                              << " -- type: "
                              << (prop.data->isList ? "list of " : "")
                              << prop.data->type.name()
                              << " -- size: " << prop.data->size() << std::endl;
                }
            }
            std::cout << "\n";
        }

        // Example of direct access
        {
            auto xData = data["vertex"]->properties["x"];
            std::cout << "x value of the first vertex element:\n" << xData->at<float>(0) << std::endl;
            std::cout << "\n";
        }

        // Example of raw pointer access
        {
            auto vertexElement = data["vertex"];
            std::cout << "Coordinates of the 5 first vertices (out of " << vertexElement->size() << "):\n";
            const float* ptX = vertexElement->properties["x"]->ptr<float>();
            const float* ptY = vertexElement->properties["y"]->ptr<float>();
            const float* ptZ = vertexElement->properties["z"]->ptr<float>();
            for (size_t i = 0; i < 5; ++i)
            {
                assert(i < vertexElement->size());
                std::cout << "* " << ptX[i] << " " << ptY[i] << " " << ptZ[i] << std::endl;
            }
            std::cout << "\n";
        }

        // Helper functions to repack data
        typedef std::vector<std::array<float, 3 > > Cloud;
        Cloud points;
        Cloud normals;
        plycpp::toPointCloud<float, Cloud>(data, points);
        plycpp::toNormalCloud<float, Cloud>(data, normals);
        std::cout << "Same output of the 5 first vertices:\n";
        for (size_t i = 0; i < 5; ++i)
        {
            assert(i < points.size());
            std::cout << "* " << points[i][0] << " " << points[i][1] << " " << points[i][2] << std::endl;
        }
        std::cout << "\n";

        // Generic method to pack multiple properties of the same type together
        try
        {
            typedef std::vector<std::array<unsigned char, 4> > RGBACloud;
            RGBACloud rgbaCloud;
            std::vector<plycpp::PropertyArrayConstPtr> properties
                    {
                            data["vertex"]->properties["red"],
                            data["vertex"]->properties["green"],
                            data["vertex"]->properties["blue"],
                            data["vertex"]->properties["alpha"]
                    };
            plycpp::packProperties<unsigned char, RGBACloud>(properties, rgbaCloud);
            std::cout << "RGBA colour of the 5 first vertices:\n";
            for (size_t i = 0; i < 5; ++i)
            {
                assert(i < rgbaCloud.size());
                std::cout << "* "
                          << static_cast<unsigned int>(rgbaCloud[i][0]) << " "
                          << static_cast<unsigned int>(rgbaCloud[i][1]) << " "
                          << static_cast<unsigned int>(rgbaCloud[i][2]) << " "
                          << static_cast<unsigned int>(rgbaCloud[i][3]) << std::endl;
            }
            std::cout << "\n";
        }
        catch (const plycpp::Exception &e)
        {
            std::cout << e.what();
        }

        // Property lists are handled in a similar manner
        try
        {
            const auto& vertexIndicesData = data["face"]->properties["vertex_indices"];
            if (vertexIndicesData && vertexIndicesData->isList)
            {
                // Only triplet lists are supported
                assert(vertexIndicesData->size() % 3 == 0);
                assert(vertexIndicesData->size() > 3);

                std::cout << "Vertex indices of the first triangle:\n" << "* "
                          << vertexIndicesData->at<int>(0) << " "
                          << vertexIndicesData->at<int>(1) << " "
                          << vertexIndicesData->at<int>(2) << std::endl;
            }
            else
                std::cout << "No valid list of vertex indices." << std::endl;
        }
        catch(const plycpp::Exception)
        {
            std::cout << "INvalid or unsupported face elements." << std::endl;

        }
        std::cout << "\n";
        // Export a PLY file
        {
            // Back conversion of the point cloud and normal cloud to PLYData
            plycpp::PLYData newPlyData;
            plycpp::fromPointCloudAndNormals<float, Cloud>(points, normals, newPlyData);

            {
                std::string filename = "point_cloud_ascii.ply";
                plycpp::save(filename, newPlyData, plycpp::FileFormat::ASCII);
                std::cout << "Point cloud exported to " << filename << std::endl;
            }

            {
                std::string filename = "point_cloud_binary.ply";
                plycpp::save(filename, newPlyData, plycpp::FileFormat::BINARY);
                std::cout << "Point cloud exported to " << filename << std::endl;
            }
        }
    }
    catch (const plycpp::Exception& e)
    {
        std::cout << "An exception happened:\n" << e.what() << std::endl;
    }

    std::cout << "Enter a char to exit..." << std::endl;
    std::getchar();
}