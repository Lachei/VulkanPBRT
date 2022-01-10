cmake_minimum_required(VERSION 3.7)

project(VulkanPBRT
    VERSION 1.0.0
    DESCRIPTION "Vulkan physically based raytracer"
    LANGUAGES C CXX
)

find_package(nlohmann_json REQUIRED)

set(SLAVE_PROJECTS true)    #information for the subprojects to not find packes via find_package they can assume the packages exist
add_subdirectory(external/vsg)
add_subdirectory(external/vsgXchange)
add_subdirectory(external/vsgImGui)
set(SLAVE_PROJECTS false)

add_subdirectory(source)

include_directories(${OpenEXR_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 17)

add_executable(VulkanPBRT ${VulkanPBRT_SRC})
target_include_directories(VulkanPBRT PRIVATE source)
target_link_libraries(VulkanPBRT vsg vsgXchange vsgImGui nlohmann_json)
set_property(TARGET VulkanPBRT PROPERTY CXX_STANDARD 17)

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

## compilation of shader files
foreach(SHADER IN LISTS SHADERS)
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
	list(APPEND SPIRV_BINARY_FILES ${current-output-path})
endforeach()

add_custom_target(CompileShaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(VulkanPBRT CompileShaders)


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
	list(APPEND GLSL_SHADER_FILES ${current-output-path})
endforeach()

add_custom_target(CopyShaders DEPENDS ${GLSL_SHADER_FILES})
add_dependencies(VulkanPBRT CopyShaders)