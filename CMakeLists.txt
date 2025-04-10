cmake_minimum_required(VERSION 3.14)

project(dsoal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(fmt-11.1.1)

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

set(VERSION 0.9)

set(DSOAL_CPPDEFS )
set(DSOAL_CFLAGS )
set(DSOAL_LINK_FLAGS )

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P MATCHES "8")
        set(DEF_FILE msvc64.def)
    else()
        set(DEF_FILE msvc.def)
    endif()

    set(DSOAL_CPPDEFS ${DSOAL_CPPDEFS} _CRT_SECURE_NO_WARNINGS NOMINMAX)

    check_cxx_compiler_flag(/permissive- HAVE_PERMISSIVE_SWITCH)
    if(HAVE_PERMISSIVE_SWITCH)
        set(DSOAL_CFLAGS ${DSOAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    endif()
    set(DSOAL_CFLAGS ${DSOAL_CFLAGS} /W4 /w14640 /wd4065 /wd4127 /wd4268 /wd4324 /wd5030 /wd5051
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
else()
    set(DEF_FILE mingw.def)

    set(DSOAL_CFLAGS ${DSOAL_CFLAGS} -Winline -Wunused -Wall -Wextra -Wshadow -Wconversion
        -Wcast-align -Wpedantic
        $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual>)

    check_cxx_compiler_flag(-ftrivial-auto-var-init=pattern HAVE_FTRIVIAL_AUTO_VAR_INIT)
    if(HAVE_FTRIVIAL_AUTO_VAR_INIT)
        set(DSOAL_CFLAGS ${DSOAL_CFLAGS} -ftrivial-auto-var-init=pattern)
    endif()

    set(OLD_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    # Yes GCC, really don't accept command line options you don't support
    set(CMAKE_REQUIRED_FLAGS "${OLD_REQUIRED_FLAGS} -Werror")
    check_cxx_compiler_flag(-msse2 HAVE_MSSE2_SWITCH)
    if(HAVE_MSSE2_SWITCH)
        set(SSE2_SWITCH "-msse2")
    endif()
    set(CMAKE_REQUIRED_FLAGS ${OLD_REQUIRED_FLAGS})
    unset(OLD_REQUIRED_FLAGS)

    if(CMAKE_SIZEOF_VOID_P MATCHES "4" AND SSE2_SWITCH)
        option(DSOAL_ENABLE_SSE2_CODEGEN "Enable SSE2 code generation instead of x87 for 32-bit targets." TRUE)
        if(DSOAL_ENABLE_SSE2_CODEGEN)
            check_cxx_compiler_flag("${SSE2_SWITCH} -mfpmath=sse" HAVE_MFPMATH_SSE_2)
            if(HAVE_MFPMATH_SSE_2)
                set(DSOAL_CFLAGS ${DSOAL_CFLAGS} ${SSE2_SWITCH} -mfpmath=sse)
            endif()
        endif()
    endif()

    set(DSOAL_LINK_FLAGS ${DSOAL_LINK_FLAGS} -static-libgcc -static-libstdc++)
    set(DSOAL_LINK_FLAGS ${DSOAL_LINK_FLAGS} "-Wl,--push-state,-Bstatic,-lstdc++,-lwinpthread,--pop-state")
endif(MSVC)


find_package(Git)
option(DSOAL_UPDATE_BUILD_VERSION "Update git build version info" ON)
if(DSOAL_UPDATE_BUILD_VERSION AND GIT_FOUND AND EXISTS "${dsoal_SOURCE_DIR}/.git")
    set(GIT_DIR "${dsoal_SOURCE_DIR}/.git")

    # Check if this is a submodule, if it is then find the .git directory
    if(NOT IS_DIRECTORY "${dsoal_SOURCE_DIR}/.git")
        file(READ ${GIT_DIR} submodule)
        string(REGEX REPLACE "gitdir: (.*)$" "\\1" GIT_DIR_RELATIVE ${submodule})
        string(STRIP ${GIT_DIR_RELATIVE} GIT_DIR_RELATIVE)
        get_filename_component(SUBMODULE_DIR ${GIT_DIR} PATH)
        get_filename_component(GIT_DIR ${SUBMODULE_DIR}/${GIT_DIR_RELATIVE} ABSOLUTE)
    endif()

    # Get the current working branch and its latest abbreviated commit hash
    add_custom_command(OUTPUT "${dsoal_BINARY_DIR}/version_witness.txt"
        BYPRODUCTS "${dsoal_BINARY_DIR}/version.h"
        COMMAND ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D LIB_VERSION=${VERSION}
            -D SRC=${dsoal_SOURCE_DIR}/version.h.in -D DST=${dsoal_BINARY_DIR}/version.h
            -P ${dsoal_SOURCE_DIR}/version.cmake
        COMMAND ${CMAKE_COMMAND} -E touch "${dsoal_BINARY_DIR}/version_witness.txt"
        WORKING_DIRECTORY "${dsoal_SOURCE_DIR}"
        MAIN_DEPENDENCY "${dsoal_SOURCE_DIR}/version.h.in"
        DEPENDS "${GIT_DIR}/index" "${dsoal_SOURCE_DIR}/version.cmake"
        VERBATIM
    )

    add_custom_target(dsoal.build_version DEPENDS "${dsoal_BINARY_DIR}/version_witness.txt")
else()
    set(GIT_BRANCH "UNKNOWN")
    set(GIT_COMMIT_HASH "unknown")
    configure_file(
        "${dsoal_SOURCE_DIR}/version.h.in"
        "${dsoal_BINARY_DIR}/version.h")
endif()


if(MINGW)
    set(CMAKE_RC_COMPILER_INIT windres)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
endif(MINGW)
enable_language(RC)

set(SRC_OBJS
    src/AL/al.h
    src/AL/alc.h
    src/AL/alext.h
    src/AL/efx.h
    src/buffer.cpp
    src/buffer.h
    src/capture.cpp
    src/capture.h
    src/comhelpers.h
    src/comptr.h
    src/dsoal_global.h
    src/dsoal.cpp
    src/dsoal.h
    src/dsoundoal.cpp
    src/dsoundoal.h
    src/eax.cpp
    src/eax.h
    src/enumerate.h
    src/expected.h
    src/factory.cpp
    src/factory.h
    src/fullduplex.cpp
    src/fullduplex.h
    src/guidprinter.h
    src/logging.cpp
    src/logging.h
    src/primarybuffer.cpp
    src/primarybuffer.h
    src/propset.cpp
    src/propset.h
    src/vmanager.h)

set(TXT_FILES
    LICENSE
    README.md)

add_library(dsound SHARED ${SRC_OBJS} version.rc ${DEF_FILE} ${TXT_FILES})
set_target_properties(dsound PROPERTIES PREFIX "")
target_link_libraries(dsound PRIVATE dsoal::fmt ${DSOAL_LINK_FLAGS})
target_include_directories(dsound PRIVATE ${dsoal_BINARY_DIR})
target_compile_definitions(dsound PRIVATE DSOAL_LIBRARY ${DSOAL_CPPDEFS})
target_compile_options(dsound PRIVATE ${DSOAL_CFLAGS})
if(TARGET dsoal.build_version)
    add_dependencies(dsound dsoal.build_version)
endif()

install(TARGETS dsound
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(EXISTS "${dsoal_BINARY_DIR}/dsoal-aldrv.dll")
    install(FILES
        "${dsoal_BINARY_DIR}/dsoal-aldrv.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

target_sources(dsound PRIVATE ${TXT_FILES})
install(FILES ${TXT_FILES} TYPE DATA)


set(NEED_ANALYZE_SOURCE_FILES "")
foreach(obj ${SRC_OBJS})
    list(APPEND NEED_ANALYZE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${obj}")
endforeach()
set(CLANG_TIDY_EXECUTABLE "clang-tidy")
if(DEFINED ENV{CLANG_TIDY_EXECUTABLE})
    set(CLANG_TIDY_EXECUTABLE $ENV{CLANG_TIDY_EXECUTABLE})
endif()
add_custom_target(clang-tidy-check ${CLANG_TIDY_EXECUTABLE} -format-style=file
    -p ${CMAKE_BINARY_DIR}/compile_commands.json ${NEED_ANALYZE_SOURCE_FILES}
    DEPENDS ${NEED_ANALYZE_SOURCE_FILES} ${CMAKE_BINARY_DIR}/compile_commands.json)


set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_VENDOR "Chris Robinson")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_OUTPUT_FILE_PREFIX packages)

include(CPack)
