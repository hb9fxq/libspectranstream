cmake_minimum_required(VERSION 3.9)
set(mLIB_VER "0.0.2")

set (CMAKE_CXX_STANDARD 17)

project(spectranstream VERSION ${mLIB_VER} DESCRIPTION "spectranstream description")
include(GNUInstallDirs)

find_package(RapidJSON REQUIRED)

add_library(spectranstream SHARED spectranstream.cpp ArbitraryLengthCircularBuffer.cpp)

set_target_properties(spectranstream PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${mLIB_VER}
    PUBLIC_HEADER "spectranstream.h")

target_include_directories(spectranstream PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(spectranstream PUBLIC ${RapidJSON} curl pthread)

install(TARGETS spectranstream EXPORT spectranstreamTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/spectranstreamConfigVersion.cmake"
    VERSION ${mLIB_VER}
    COMPATIBILITY AnyNewerVersion)

configure_file(spectranstreamConfig.cmake.in spectranstreamConfig.cmake @ONLY)

install(FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(EXPORT spectranstreamTargets
    FILE spectranstreamTargets.cmake
    NAMESPACE spectranstream::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})