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

add_executable(VulkanPBRT ${SRC})

target_link_libraries(VulkanPBRT vsg vsgXchange vsgImGui)