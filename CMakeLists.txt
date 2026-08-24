cmake_minimum_required(VERSION 3.20.0)

project(p4est_demo LANGUAGES C CXX)

set (P4EST_DEMO_BUGS    jfonseca@aneo.fr)
set (P4EST_DEMO_SRCDIR  "${PROJECT_SOURCE_DIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

set(CMAKE_CXX_STANDARD 17)

include(CheckCXXCompilerFlag)

if (${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In place build not allowed; please use a separate build directory")
endif()

find_package(Git)

include(GetVersionFromGitTag)

set(P4EST_DEMO VERSION ${P4EST_DEMO_VERSION_STRING})

message(STATUS "Configuring version : ${P4EST_DEMO_VERSION_STRING_FULL}")

add_library(P4EST_DEMO INTERFACE)

target_include_directories(P4EST_DEMO
  INTERFACE
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

find_package(MPI REQUIRED)
find_package(TIFF REQUIRED)
find_package(P4EST REQUIRED)

include_directories(${P4EST_INCLUDE_DIRS})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MPI_CXX_COMPILE_OPTIONS}")
set (CMAKE_CXX_COMPILER "${MPI_CXX_COMPILER}")

target_link_libraries(P4EST_DEMO
  INTERFACE ${P4EST_LIBRARIES} TIFF::TIFF
)

add_subdirectory (demo_exe)

install (TARGETS P4EST_DEMO DESTINATION lib)

add_custom_target(copy-files ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/test
  ${CMAKE_CURRENT_BINARY_DIR}/demo_exe)