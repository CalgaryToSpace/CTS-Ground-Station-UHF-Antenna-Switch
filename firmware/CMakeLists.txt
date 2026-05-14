cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(cts_uhf_rxtx_switch C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# RP2040-Zero is an RP2040 board.
set(PICO_BOARD pico)

pico_sdk_init()

add_executable(cts_uhf_rxtx_switch
    main.c
)

target_link_libraries(cts_uhf_rxtx_switch
    pico_stdlib
    hardware_adc
    hardware_gpio
)

# USB-CDC for serial; disable UART stdio, apparently so we don't burn pins.
pico_enable_stdio_usb(cts_uhf_rxtx_switch  1)
pico_enable_stdio_uart(cts_uhf_rxtx_switch 0)

# Produce .uf2 etc.
pico_add_extra_outputs(cts_uhf_rxtx_switch)
