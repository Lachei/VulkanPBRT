
include_directories(${PROJECT_SOURCE_DIR}/External/vsg/include/vsg)
include_directories(Buffers)
include_directories(RenderModules)
include_directories(Scene)
include_directories(IO)

file(GLOB SRC_TOP
       ${PROJECT_SOURCE_DIR}/Source/*.cpp)
file(GLOB_RECURSE SRC_NESTED
       ${PROJECT_SOURCE_DIR}/Source/Buffers/*.cpp
	   ${PROJECT_SOURCE_DIR}/Source/RenderModules/*.cpp
       ${PROJECT_SOURCE_DIR}/Source/Scene/*.cpp
	   ${PROJECT_SOURCE_DIR}/Source/IO/*.cpp)
set(SRC ${SRC_TOP} ${SRC_NESTED})
#list(FILTER SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/vsg/.*" )
file(GLOB_RECURSE VSG_SRC vsg/*)
foreach(source ${VSG_SRC})
    list(REMOVE_ITEM SRC ${source})
endforeach()

set(VulkanPBRT_SRC ${VulkanPBRT_SRC} ${SRC} PARENT_SCOPE)
