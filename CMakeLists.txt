 cmake_minimum_required(VERSION 3.7)
 
 project(VulkanPBRT
    VERSION 1.0.0
    DESCRIPTION "Vulkan physically based raytracer"
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig)
pkg_check_modules(xcb REQUIRED IMPORTED_TARGET xcb)
include(CMakeFindDependencyMacro)
set(vsg_glslang_DIR ${CMAKE_SOURCE_DIR}/vsg)
find_dependency(vsg_glslang)

add_subdirectory(vsg)
set(IMGUI_HEADER
    vsg/src/imgui/)
include_directories(vsg/include ${glslang_INCLUDE_DIRS} ${IMGUI_HEADER})

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES
    "*.h"
    "*.hpp"
    "*.cpp"
)

set(SHADERS
    simple_closesthit.rchit
    simple_miss.rmiss
    simple_raygen.rgen
)

add_executable(VulkanPBRT ${SOURCES} ${VSG_ALL})

target_link_libraries(VulkanPBRT Vulkan::Vulkan glslang::glslang Threads::Threads xcb)

## compilaiton of shader files
function(add_shader TARGET SHADER)
    find_program(GLSLC glslc)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})
    set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
           OUTPUT ${current-output-path}
           COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
           DEPENDS ${current-shader-path}
           IMPLICIT_DEPENDS CXX ${current-shader-path}
           VERBATIM)

    # Make sure our build depends on this output.
    set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
    target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

foreach(S IN LISTS SHADERS)
    add_shader(VulkanPBRT ${S})
endforeach()