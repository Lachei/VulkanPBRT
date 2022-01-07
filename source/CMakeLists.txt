
include_directories(${PROJECT_SOURCE_DIR}/external/vsg/include/vsg)
include_directories(buffers)
include_directories(renderModules)
include_directories(scene)
include_directories(io)

file(GLOB SRC_TOP
       ${PROJECT_SOURCE_DIR}/source/*.cpp)
file(GLOB_RECURSE SRC_NESTED
       ${PROJECT_SOURCE_DIR}/source/buffers/*.cpp
	   ${PROJECT_SOURCE_DIR}/source/renderModules/*.cpp
       ${PROJECT_SOURCE_DIR}/source/scene/*.cpp
	   ${PROJECT_SOURCE_DIR}/source/io/*.cpp)
set(SRC ${SRC_TOP} ${SRC_NESTED})
#list(FILTER SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/vsg/.*" )
file(GLOB_RECURSE VSG_SRC vsg/*)
foreach(source ${VSG_SRC})
    list(REMOVE_ITEM SRC ${source})
endforeach()

set(VulkanPBRT_SRC ${VulkanPBRT_SRC} ${SRC} PARENT_SCOPE)
