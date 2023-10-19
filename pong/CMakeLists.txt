cmake_minimum_required(VERSION 3.13)
project(Pong
    VERSION 0.0.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/../cmake")
set(DEPENDS_DIR ${CMAKE_SOURCE_DIR}/../depends)

set(WARNING_COMPILER_OPTIONS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

include(sokol)

set(SRCS
    src/main.c
)

add_executable(pong ${SRCS})
target_include_directories(pong SYSTEM PRIVATE ${SOKOL_INCLUDE_DIR})
target_link_libraries(pong X11 Xcursor Xi GL dl pthread m)
