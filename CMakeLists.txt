cmake_minimum_required(VERSION 3.7)
 
project(VulkanPBRT
    VERSION 1.0.0
    DESCRIPTION "Vulkan physically based raytracer"
    LANGUAGES C CXX
)

include(ExternalProject)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR}/external/vsg/lib/cmake/vsg_glslang ${CMAKE_BINARY_DIR}/external/vsg/lib/cmake/vsg ${CMAKE_BINARY_DIR}/external/vsgXchange/lib/cmake/vsgXchange/)
string(REPLACE ";" "|" CMAKE_PREFIX_PATH_ALT_SEP "${CMAKE_PREFIX_PATH}")
set(common_args
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_FLAGS_DEBUG:STRING=${CMAKE_CXX_FLAGS_DEBUG}
    -DCMAKE_CXX_FLAGS_RELEASE:STRING=${CMAKE_CXX_FLAGS_RELEASE}
    -DCMAKE_PREFIX_PATH:STRING=${CMAKE_PREFIX_PATH_ALT_SEP}
)
ExternalProject_Add(vsg
    URL "${CMAKE_CURRENT_SOURCE_DIR}/external/vsg"
    LIST_SEPARATOR |
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/external/vsg/ ${common_args}
)
ExternalProject_Add(vsgXchange
    URL "${CMAKE_CURRENT_SOURCE_DIR}/external/vsgXchange"
    LIST_SEPARATOR |
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/external/vsgXchange/ ${common_args}
)
ExternalProject_Add(vsgImGui
    URL "${CMAKE_CURRENT_SOURCE_DIR}/external/vsgImGui"
    LIST_SEPARATOR |
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/external/vsgImGui/ ${common_args}
)
find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)
find_package(vsgImGui REQUIRED)

add_subdirectory(Denoiser)
add_subdirectory(TAA)

include_directories(${OpenEXR_INCLUDE_DIRS})
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

add_executable(VulkanPBRT ${SRC})
add_dependencies(vsgXchange vsg)
add_dependencies(vsgImGui vsg vsgXchange)
add_dependencies(VulkanPBRT vsg vsgXchange vsgImGui)

target_link_libraries(VulkanPBRT vsg::vsg vsgXchange::vsgXchange vsgImGui::vsgImGui)

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