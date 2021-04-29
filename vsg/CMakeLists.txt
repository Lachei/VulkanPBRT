# cmake file to get all src files for compilation and to add include to include directories

file(GLOB_RECURSE VSG_SRC src/
    *.cpp
)
list(FILTER VSG_SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/vsg/src/imgui/.*" )

file(GLOB IMGUI_SRC src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/backends/imgui_impl_vulkan.cpp)

file(GLOB ANDROID_PLATFORM src/vsg/platform/android/Android_Window.cpp)
file(GLOB MAC_PLATFORM src/vsg/platform/macos/MacOS_Window.mm)
file(GLOB WIN_PLATFORM src/vsg/platform/win32/Win32_Window.cpp)
file(GLOB UINX_PLATFORM src/vsg/platform/unix/Xcb_Window.cpp)

#exclude wrong platfrom things
if (ANDROID)
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}${WIN_PLATFORM}${UINX_PLATFORM}")

elseif (WIN32)
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}${ANDROID_PLATFORM}${UINX_PLATFORM}")
elseif (APPLE)
    list(REMOVE_ITEM VSG_SRC "${ANDROID_PLATFORM}${WIN_PLATFORM}${UINX_PLATFORM}")
else()
    list(REMOVE_ITEM VSG_SRC "${MAC_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${WIN_PLATFORM}")
    list(REMOVE_ITEM VSG_SRC "${ANDROID_PLATFORM}")
endif()

set(VSG_ALL ${VSG_SRC} ${IMGUI_SRC} PARENT_SCOPE)