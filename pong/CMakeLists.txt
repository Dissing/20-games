cmake_minimum_required(VERSION 3.13)
project(Pong
    VERSION 0.0.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/../cmake")
set(DEPENDS_DIR ${CMAKE_SOURCE_DIR}/../depends)

set(WARNING_COMPILER_OPTIONS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

set(HANDMADE_INCLUDE_DIR ${DEPENDS_DIR}/HandmadeMath/)
set(SOKOL_INCLUDE_DIR ${DEPENDS_DIR}/sokol/)
set(SOKOL_SHDC_BIN ${DEPENDS_DIR}/sokol-tools-bin/bin/linux/sokol-shdc)
set(STB_INCLUDE_DIR ${DEPENDS_DIR}/stb/)

function(target_add_shader TARGET SHADER_FILENAME)
    get_filename_component(SHADER_NAME ${SHADER_FILENAME} NAME)

    set(SHADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILENAME})
    set(SHADER_OUTPUT_PATH ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.h)

    get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT_PATH} DIRECTORY)
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT_PATH}
        COMMAND ${SOKOL_SHDC_BIN} --input ${SHADER_PATH} --output ${SHADER_OUTPUT_PATH} --slang glsl330
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling ${SHADER_PATH}"
        VERBATIM)
    add_custom_target(${SHADER_NAME} DEPENDS ${SHADER_OUTPUT_PATH})
    add_dependencies(${TARGET} ${SHADER_NAME})

    set_source_files_properties(${SHADER_OUTPUT_PATH} PROPERTIES GENERATED TRUE)
endfunction(target_add_shader)

function(add_resource PATH)
    cmake_path(GET PATH FILENAME FILENAME)
    configure_file(${PATH} ${CMAKE_BINARY_DIR}/${FILENAME} COPYONLY)
endfunction(add_resource PATH)

set(SRCS
    src/error.c
    src/main.c
)

add_executable(pong ${SRCS})
target_add_shader(pong src/quad.glsl)
add_resource(../resources/monogram.png)
target_include_directories(pong SYSTEM PRIVATE ${SOKOL_INCLUDE_DIR} ${HANDMADE_INCLUDE_DIR} ${STB_INCLUDE_DIR})
target_include_directories(pong PRIVATE ${CMAKE_BINARY_DIR}/shaders)
target_link_libraries(pong X11 Xcursor Xi GL dl pthread m)
