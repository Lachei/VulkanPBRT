# add assimp if vailable
find_package(assimp 5.1 QUIET)
if(NOT assimp_FOUND)
find_package(assimp 5.0 QUIET)
endif()



if(assimp_FOUND)
    OPTION(vsgXchange_assimp "Optional Assimp support provided" ON)
endif()

if (${vsgXchange_assimp})

    string(REPLACE "." ";" ASSIMP_VERSION_LIST ${assimp_VERSION})
    list(GET ASSIMP_VERSION_LIST 0 ASSIMP_VERSION_MAJOR)
    list(GET ASSIMP_VERSION_LIST 1 ASSIMP_VERSION_MINOR)
    list(GET ASSIMP_VERSION_LIST 2 ASSIMP_VERSION_PATCH)

    message("\nassimp_VERSION "${assimp_VERSION})

    set(EXTRA_DEFINES ${EXTRA_DEFINES} "ASSIMP_VERSION_MAJOR=${ASSIMP_VERSION_MAJOR}" "ASSIMP_VERSION_MINOR=${ASSIMP_VERSION_MINOR}" "ASSIMP_VERSION_PATCH=${ASSIMP_VERSION_PATCH}" )

    set(SOURCES ${SOURCES}
        assimp/assimp.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${assimp_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} assimp::assimp)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(assimp)")
    endif()
else()
    set(SOURCES ${SOURCES}
        assimp/assimp_fallback.cpp
    )
endif()
