cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/../modules/fm24cl64b"
                            "${CMAKE_SOURCE_DIR}/../modules/cc1125"
                            "${CMAKE_SOURCE_DIR}/../modules/bq76907")

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(SAHEL_CUBESAT)

target_sources(app PRIVATE
    src/main.c
)