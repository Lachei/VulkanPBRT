# add openexr if vailable
find_package(OpenEXR QUIET)

if(OpenEXR_FOUND)
    OPTION(vsgXchange_openEXR "Optional OpenEXR support provided" ON)
endif()

if (${vsgXchange_openEXR})
    set(SOURCES ${SOURCES}
        openexr/openexr.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${OpenEXR_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} OpenEXR::IlmImf)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(OpenEXR)")
    endif()
else()
    set(SOURCES ${SOURCES}
        openexr/openexr_fallback.cpp
    )
endif()