# micron

cmake_minimum_required(VERSION 3.12)
set(PROJECT micron)

# We're currently building for the "old" RP2040 microprocessor.

set(PICO_BOARD pico_w)
set(PICO_PLATFORM rp2040)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico SDK
pico_sdk_init()

# Build only for the tinyusb_device
if (NOT TARGET tinyusb_device)
	message(FATAL_ERROR "Not compiling for tinyusb")
endif()

file(GLOB SOURCES
    ../src/*.c
    ../src/net/*.c
    ../src/drv/*.c
)

add_executable(${PROJECT} ${SOURCES})

target_compile_options(${PROJECT} PRIVATE -Wall -Wextra)
target_link_libraries(${PROJECT} pico_cyw43_arch_lwip_poll
    pico_stdlib pico_multicore pico_util)
target_include_directories(${PROJECT} PRIVATE ../inc
    ../build/include ../build)

pico_enable_stdio_usb(${PROJECT} 1)

# This needs to be last!
pico_add_extra_outputs(${PROJECT})
