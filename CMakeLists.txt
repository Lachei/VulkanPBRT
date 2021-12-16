cmake_minimum_required(VERSION 3.7)

add_subdirectory(Source)

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
	add_custom_target("compileShader_${SHADER}" DEPENDS ${current-output-path})
    add_dependencies(VulkanPBRT "compileShader_${SHADER}")
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