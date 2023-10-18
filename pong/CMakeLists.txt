cmake_minimum_required(VERSION 3.13)
project(Pong
    VERSION 0.0.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 17)


set(WARNING_COMPILER_OPTIONS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

set(SRCS
    src/main.c
)

add_executable(pong ${SRCS})

