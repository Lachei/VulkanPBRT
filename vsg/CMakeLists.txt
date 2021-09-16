# cmake file to get all src files for compilation and to add include to include directories
file(GLOB_RECURSE VSG_SRC src/
    *.cpp
    +.h
)
file(GLOB_RECURSE VSG_IMGUI_SRC src/imgui/*)
foreach(source ${VSG_IMGUI_SRC})
    list(REMOVE_ITEM VSG_SRC ${source})
endforeach()

file(GLOB IMGUI_SRC src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/backends/imgui_impl_vulkan.cpp)

file(GLOB ANDROID_PLATFORM src/vsg/platform/android/Android_Window.cpp)
file(GLOB MAC_PLATFORM src/vsg/platform/macos/MacOS_Window.mm)
file(GLOB WIN_PLATFORM src/vsg/platform/win32/Win32_Window.cpp)
file(GLOB UNIX_PLATFORM src/vsg/platform/unix/Xcb_Window.cpp)
include(src/vsgXchange/assimp/build_vars.cmake)
#exclude wrong platfrom things
if (ANDROID)
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${UNIX_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${WIN_PLATFORM}")
elseif (WIN32)
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${UNIX_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${ANDROID_PLATFORM}")
elseif (APPLE)
     list(REMOVE_ITEM VSG_SRC "${ANDROID_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${UNIX_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${WIN_PLATFORM}")
else()
	find_package(PkgConfig)
	pkg_check_modules(xcb REQUIRED IMPORTED_TARGET xcb)
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${WIN_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${ANDROID_PLATFORM}")
endif()

file(GLOB SPIRV_REFLECT_SRC src/SPIRV-Reflect/spirv_reflect.c)

set(VSG_ALL ${VSG_SRC} ${IMGUI_SRC} ${SPIRV_REFLECT_SRC} PARENT_SCOPE)

if (glslang_FOUND)
    message("-- glslang defines added to compilation")
    set(LIBRARIES ${LIBRARIES} PUBLIC ${glslang_LIBRARIES} PARENT_SCOPE)

    set(EXTRA_DEFINES ${EXTRA_DEFINES} HAS_GLSLANG)

    include(CheckCXXSourceRuns)

    set(CMAKE_REQUIRED_LIBRARIES ${glslang_LIBRARY})
    set(CMAKE_REQUIRED_INCLUDES ${glslang_INCLUDE_DIR})

    if (NOT GLSLANG_ResourceLimits_maxDualSourceDrawBuffersEXT)
        check_cxx_source_runs("
            #include <glslang/Include/ResourceLimits.h>

            int main()
            {
                TBuiltInResource resource;
                resource.maxDualSourceDrawBuffersEXT = 0;
                return 0;
            }
        " GLSLANG_ResourceLimits_maxDualSourceDrawBuffersEXT)
    endif()

    # check whether glslang/build_info.h exists
    if (GLSLANG_ResourceLimits_maxDualSourceDrawBuffersEXT)
        set(EXTRA_DEFINES ${EXTRA_DEFINES} "GLSLANG_ResourceLimits_maxDualSourceDrawBuffersEXT" PARENT_SCOPE)
    endif()

    install(
        FILES vsg_glslangConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vsg_glslang
    )
endif()