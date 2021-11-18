cmake_minimum_required(VERSION 3.7)
 
project(VulkanPBRT
    VERSION 1.0.0
    DESCRIPTION "Vulkan physically based raytracer"
    LANGUAGES C CXX
)

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(OpenEXR REQUIRED)
include(CMakeFindDependencyMacro)
set(vsg_glslang_DIR ${CMAKE_SOURCE_DIR}/vsg)
find_dependency(vsg_glslang)

add_subdirectory(vsg)
add_subdirectory(Denoiser)
add_subdirectory(TAA)
set(IMGUI_HEADER
    vsg/src/imgui/)
include_directories(vsg/include ${glslang_INCLUDE_DIRS} ${IMGUI_HEADER} ${OpenEXR_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_TOP
       ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SRC_NESTED
       ${PROJECT_SOURCE_DIR}/TAA/*.cpp
	   ${PROJECT_SOURCE_DIR}/Denoiser/*.cpp
       ${PROJECT_SOURCE_DIR}/UtilityPipelines/*.cpp)
set(SRC ${SRC_TOP} ${SRC_NESTED})
list(FILTER SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/vsg/.*" )

set(SHADERS
    shadow.rmiss
    ptAlphaHit.rahit
    bfr.comp
    taa.comp
    bfrBlender.comp
    bmfrPre.comp
    bmfrFit.comp
    bmfrFitLegacy.comp
    bmfrPost.comp
    ptRaygen.rgen
    ptClosesthit.rchit
    ptMiss.rmiss
)

set(UNCOMPILED_SHADERS
    brdf.glsl
    geometry.glsl
    layoutPTAccel.glsl
    layoutPTGeometry.glsl
    layoutPTGeometryImages.glsl
    layoutPTImages.glsl
    layoutPTLights.glsl
    layoutPTPushConstants.glsl
    layoutPTUniform.glsl
    lighting.glsl
    math.glsl
    ptConstants.glsl
    ptStructures.glsl
    random.glsl
    sampling.glsl
    camera.glsl
    color.glsl
    ptRaygen.rgen
    accumulator.comp
    formatConverter.comp
)

add_executable(VulkanPBRT ${SRC} ${VSG_ALL})

target_compile_definitions(VulkanPBRT PRIVATE ${EXTRA_DEFINES})
target_link_libraries(VulkanPBRT Vulkan::Vulkan 
    glslang::glslang glslang::MachineIndependent glslang::OSDependent glslang::OGLCompiler glslang::SPIRV glslang::GenericCodeGen glslang::SPIRV-Tools-opt glslang::SPIRV-Tools
     Threads::Threads OpenEXR::IlmImf)
if(UNIX)
    target_link_libraries(VulkanPBRT xcb ${ASSIMP_LIBRARIES})
elseif(WIN32)
    target_link_libraries(VulkanPBRT assimp::assimp)
endif()

## compilation of shader files
function(add_shader TARGET SHADER)
    find_program(GLSLC glslc)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})
    set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
           OUTPUT ${current-output-path}
           COMMAND ${GLSLC} --target-spv=spv1.4 -o ${current-output-path} ${current-shader-path}
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

foreach(S IN LISTS UNCOMPILED_SHADERS)
    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${S})
    set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${S})

    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
           OUTPUT ${current-output-path}
           COMMAND ${CMAKE_COMMAND} -E copy ${current-shader-path} ${current-output-path}
           DEPENDS ${current-shader-path}
           VERBATIM)

    # Make sure our build depends on this output.
    add_custom_target("copyShader_${S}" DEPENDS ${current-output-path})
    add_dependencies(VulkanPBRT "copyShader_${S}")
endforeach()