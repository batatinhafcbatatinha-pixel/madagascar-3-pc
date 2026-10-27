# madagascar3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(madagascar3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(MADAGASCAR3_SOURCES
    src/main.cpp
    src/native_gpu/gpu_hooks.cpp
    src/native_audio/audio_exports.cpp
    src/native_audio/audio_system.cpp
    src/native_audio/xma_engine.cpp
    src/native_audio/xma_commands.cpp
    src/pc_settings.cpp
    src/pc_theme.cpp
    src/pc_font.cpp
)

if(WIN32)
    add_executable(madagascar3 WIN32 ${MADAGASCAR3_SOURCES})
else()
    add_executable(madagascar3 ${MADAGASCAR3_SOURCES})
endif()

rexglue_setup_target(madagascar3)
# PDB with public symbols (sub_XXXXXXXX names) for crash stacks; no debug info in objects.
if(WIN32)
    target_link_options(madagascar3 PRIVATE -Xlinker /DEBUG)
endif()

# Host decoder/output; do not instantiate the SDK APU through the app factory.
target_link_libraries(madagascar3 PRIVATE rex::libavcodec rex::libavutil)
target_include_directories(madagascar3 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../battlefield2/project/third_party/ffmpeg/include")

# Offline native shader generation is independent of the Xenos GPU plugin.
# Draw/resource hooks remain to be recovered before replacing that plugin.
add_library(madagascar3_native_shaders STATIC src/native_gpu/xenos_hlsl.cpp)
target_include_directories(madagascar3_native_shaders PUBLIC src/native_gpu)
add_library(madagascar3_native_gpu STATIC
    src/native_gpu/renderer.cpp
    src/native_gpu/textures.cpp)
target_include_directories(madagascar3_native_gpu PRIVATE src/native_gpu)
target_link_libraries(madagascar3_native_gpu PUBLIC madagascar3_native_shaders
    rex::runtime rex::xxhash d3d11 dxgi d3dcompiler winmm)
target_link_libraries(madagascar3 PRIVATE madagascar3_native_gpu)
if(WIN32)
    add_executable(madagascar3_shaderc src/native_gpu/shaderc.cpp)
    target_link_libraries(madagascar3_shaderc PRIVATE madagascar3_native_shaders d3dcompiler)
endif()

# The settings menu's face: PT Sans (ParaType Free Font License / OFL), as the game uses.
add_custom_command(TARGET madagascar3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:madagascar3>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/fonts/PT_Sans-Web-Bold.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/PT_Sans-Web-Regular.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/OFL.txt"
            "$<TARGET_FILE_DIR:madagascar3>/fonts/")

# Vulkan option of the settings menu: DXVK's d3d11.dll/dxgi.dll in vulkan\ next to the
# executable (pc_settings.cpp loads them by full path when "API grafica" is Vulkan).
set(DXVK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/dxvk-3.1.1" CACHE PATH
    "DXVK release for Windows (x64/d3d11.dll, x64/dxgi.dll, LICENSE)")
if(WIN32 AND EXISTS "${DXVK_DIR}/x64/d3d11.dll")
    add_custom_command(TARGET madagascar3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:madagascar3>/vulkan"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXVK_DIR}/x64/d3d11.dll" "${DXVK_DIR}/x64/dxgi.dll"
                "$<TARGET_FILE_DIR:madagascar3>/vulkan/")
    if(EXISTS "${DXVK_DIR}/LICENSE")
        add_custom_command(TARGET madagascar3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXVK_DIR}/LICENSE"
                    "$<TARGET_FILE_DIR:madagascar3>/vulkan/LICENSE.dxvk.txt")
    endif()
endif()
