cmake_minimum_required(VERSION 3.26)

project(3DGS_AdvGI)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_subdirectory(ext/gfx)
add_subdirectory(ext/plycpp)

# A test executable for dependencies and gfx
add_executable(test test/test.cpp)
target_link_libraries(test PUBLIC gfx plycpp)

# Set the working directory to source directory (to make shader paths work)
set_target_properties(test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Post-build command to copy WinPixEventRuntime.dll to the output directory
add_custom_command(TARGET test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:test> $<TARGET_FILE_DIR:test>
        COMMAND_EXPAND_LISTS
)

# The major executable
add_subdirectory(src)