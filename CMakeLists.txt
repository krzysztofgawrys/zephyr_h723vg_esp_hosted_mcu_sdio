# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(NO_BUILD_TYPE_WARNING true)
set(CMAKE_BUILD_TYPE Debug)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD h723)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(NANOPB_DIR ${ZEPHYR_BASE}/../modules/lib/nanopb)
set(NANOPB_GENERATOR ${NANOPB_DIR}/generator/nanopb_generator.py)

# 2. Zdefiniuj pliki
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/esp_hosted_rpc.proto)
set(GEN_C_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/esp_hosted_rpc.pb.c)
set(GEN_H_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/esp_hosted_rpc.pb.h)

# 3. Ręczne polecenie generowania (Custom Command)
# To sprawi, że pliki powstaną przed kompilacją, ale będą zwykłymi plikami C
add_custom_command(
    OUTPUT ${GEN_C_FILE} ${GEN_H_FILE}
    COMMAND ${PYTHON_EXECUTABLE} ${NANOPB_GENERATOR}
            -I${CMAKE_CURRENT_SOURCE_DIR}/proto
            --output-dir=${CMAKE_CURRENT_SOURCE_DIR}/proto
            ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generowanie plików Nanopb dla ${PROTO_FILE}"
)

project(cellular_modem)

target_sources(app PRIVATE
    src/main.c
    src/shell.c
    src/esp_hosted.c
    src/sdio_io.c
    ${GEN_C_FILE}
 )
 
target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/proto)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
