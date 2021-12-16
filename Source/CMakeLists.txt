project(VulkanPBRT
    VERSION 1.0.0
    DESCRIPTION "Vulkan physically based raytracer"
    LANGUAGES C CXX
)

find_package(nlohmann_json REQUIRED)

set(SLAVE_PROJECTS true)    #information for the subprojects to not find packes via find_package they can assume the packages exist
add_subdirectory(External/vsg)
add_subdirectory(External/vsgXchange)
add_subdirectory(External/vsgImGui)
set(SLAVE_PROJECTS false)


include_directories(External/vsg/include/vsg)
include_directories(Buffers)
include_directories(RenderModules)
include_directories(Scene)
include_directories(IO)

include_directories(${OpenEXR_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_TOP
       ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SRC_NESTED
       ${PROJECT_SOURCE_DIR}/Buffers/*.cpp
	   ${PROJECT_SOURCE_DIR}/RenderModules/*.cpp
       ${PROJECT_SOURCE_DIR}/Scene/*.cpp
	   ${PROJECT_SOURCE_DIR}/IO/*.cpp)
set(SRC ${SRC_TOP} ${SRC_NESTED})
#list(FILTER SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/vsg/.*" )
file(GLOB_RECURSE VSG_SRC vsg/*)
foreach(source ${VSG_SRC})
    list(REMOVE_ITEM SRC ${source})
endforeach()

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

target_link_libraries(VulkanPBRT vsg vsgXchange vsgImGui)

## compilation of shader files
function(add_shader TARGET SHADER)
    find_program(GLSLC glslc)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${SHADER})
    set(current-output-path ${CMAKE_BINARY_DIR}/Shaders/${SHADER}.spv)

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
    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${S})
    set(current-output-path ${CMAKE_BINARY_DIR}/Shaders/${S})

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