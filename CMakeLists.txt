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
