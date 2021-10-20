cmake_minimum_required(VERSION 3.9)
set(mLIB_VER "0.0.2")
set (CMAKE_CXX_STANDARD 17)

project(spectrantransfer VERSION ${mLIB_VER} DESCRIPTION "spectrantransfer description")
include(GNUInstallDirs)

add_executable(spectrantransfer main.cpp)

include_directories(${spectranstream_SOURCE_DIR})

target_link_libraries(spectrantransfer spectranstream)


install(TARGETS spectrantransfer DESTINATION bin)