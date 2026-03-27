cmake_minimum_required(VERSION 3.22)
project(robomaster VERSION 0.0.1 LANGUAGES C CXX ASM)

file(GLOB_RECURSE SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
)

add_library(robomaster STATIC ${SOURCES})
target_include_directories(robomaster
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

#target_compile_options(robomaster PRIVATE -O3)

target_link_libraries(robomaster PUBLIC bsp m)
target_link_libraries(robomaster PUBLIC components::utils)

set_target_properties(robomaster PROPERTIES
        C_STANDARD 17
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE NO
)

add_library(components::robomaster ALIAS robomaster)