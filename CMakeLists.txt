cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_display_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico2_display_test
    main.c
)

target_include_directories(pico2_display_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(pico2_display_test PRIVATE
    -Wall
    -Wextra
)

target_link_libraries(pico2_display_test
    pico_stdlib
    hardware_spi
    tinyusb_host
    tinyusb_board
)

pico_enable_stdio_usb(pico2_display_test 0)
pico_enable_stdio_uart(pico2_display_test 0)

pico_add_extra_outputs(pico2_display_test)
