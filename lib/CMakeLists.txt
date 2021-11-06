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

target_include_directories(spectranstream PRIVATE .)

target_link_libraries(spectranstream ${RapidJSON} curl pthread)

install(TARGETS spectranstream
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})



    export(PACKAGE FooBar)