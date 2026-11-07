cmake_minimum_required(VERSION 3.18)

# build options
set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

# Include Platform common options
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

option(USE_SYSTEM_CURL "use system installed libcurl" ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
endif()

# toolchain
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# project info
project(SwitchFS)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_ALTER "0")
set(PROJECT_TITLEID "010ff000ffff0005")
set(PROJECT_AUTHOR "SwitchFS Team")
set(PACKAGE_NAME "fun.switchfs.app")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# setting src and include
file(GLOB_RECURSE MAIN_SRC "${PROJECT_SOURCE_DIR}/app/src/*.cpp")
if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
endif()

# build borealis library
add_subdirectory(library/borealis/library)

# libcurl
find_package(CURL REQUIRED)
list(APPEND APP_PLATFORM_INCLUDE ${CURL_INCLUDE_DIRS})
list(APPEND APP_PLATFORM_LIB ${CURL_LIBRARIES})

# libarchive (for extracting rar / zip on the fly)
if (PLATFORM_SWITCH)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(LIBARCHIVE REQUIRED libarchive)
    list(APPEND APP_PLATFORM_INCLUDE ${LIBARCHIVE_INCLUDE_DIRS})
    list(APPEND APP_PLATFORM_LIB ${LIBARCHIVE_LIBRARIES})
else()
    if (EXISTS "/opt/homebrew/opt/libarchive")
        list(APPEND APP_PLATFORM_INCLUDE "/opt/homebrew/opt/libarchive/include")
        list(APPEND APP_PLATFORM_LIB "/opt/homebrew/opt/libarchive/lib/libarchive.dylib")
    else()
        find_package(LibArchive)
        if (LibArchive_FOUND)
            list(APPEND APP_PLATFORM_INCLUDE ${LibArchive_INCLUDE_DIRS})
            list(APPEND APP_PLATFORM_LIB ${LibArchive_LIBRARIES})
        endif()
    endif()
endif()

# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/app/include
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/extern/nlohmann
    ${APP_PLATFORM_INCLUDE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    borealis
    ${APP_PLATFORM_LIB}
)

if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreWLAN")
endif()

if (PLATFORM_DESKTOP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_BINARY_DIR}/resources
    )

    # Unit Tests target
    enable_testing()
    add_executable(switchfs_tests
        tests/unit/test_main.cpp
        app/src/api/nswgf.cpp
        app/src/api/romm.cpp
        app/src/utils/http.cpp
        app/src/data/extractor/archive_extractor.cpp
        app/src/data/installer/nsp_installer.cpp
        app/src/data/settings_manager.cpp
        app/src/data/library_manager.cpp
        app/src/data/remote/host_resolver.cpp
    )
    target_include_directories(switchfs_tests PRIVATE
        app/include
        ${BOREALIS_LIBRARY}/include
        ${BOREALIS_LIBRARY}/extern/nlohmann
        ${APP_PLATFORM_INCLUDE}
    )
    target_link_libraries(switchfs_tests PRIVATE
        borealis
        ${APP_PLATFORM_LIB}
    )
    if (APPLE)
        target_link_libraries(switchfs_tests PRIVATE "-framework CoreWLAN")
    endif()
    add_test(NAME SwitchFSTests COMMAND switchfs_tests)
elseif (PLATFORM_SWITCH)
    find_program(NX_NACPTOOL_EXE NAMES nacptool HINTS "${DEVKITPRO}/tools/bin")
    find_program(NX_ELF2NRO_EXE NAMES elf2nro HINTS "${DEVKITPRO}/tools/bin")

    add_custom_target(${PROJECT_NAME}.nro ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
endif()
