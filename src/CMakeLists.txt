
add_executable(3DGS_AdvGI
        main.cpp
        app.cpp
        app_internal.cpp
        scene.cpp
        device_scene.cpp
        renderer.cpp
        renderer_init.cpp
        timed.cpp
        lut.cpp
        bluenoise.cpp
)
target_link_libraries(3DGS_AdvGI PUBLIC gfx glm)

# Make gfx scene header work
target_compile_definitions(3DGS_AdvGI PRIVATE GFX_ENABLE_SCENE)

# Set the working directory to source directory (to make shader paths work)
set_target_properties(3DGS_AdvGI PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Post-build command to copy WinPixEventRuntime.dll to the output directory
add_custom_command(TARGET 3DGS_AdvGI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:3DGS_AdvGI> $<TARGET_FILE_DIR:3DGS_AdvGI>
        COMMAND_EXPAND_LISTS
)
