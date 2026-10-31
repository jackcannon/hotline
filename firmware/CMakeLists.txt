cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD adafruit_qtpy_rp2040)

# Use PICO_SDK_PATH if it is set. If not, download this SDK release.
set(PICO_SDK_FETCH_FROM_GIT ON)
set(PICO_SDK_FETCH_FROM_GIT_TAG 2.3.1)
include(pico_sdk_import.cmake)

project(hotline C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(hotline src/main.c src/keyboard.c src/mic.c src/sidetone.c src/usb_descriptors.c src/usb_device.c)
target_include_directories(hotline PRIVATE src)
target_link_libraries(hotline pico_stdlib pico_unique_id hardware_adc hardware_pwm tinyusb_device)

# The default UART uses GPIO5 (RX), which is a mode toggle input.
pico_enable_stdio_uart(hotline 0)
pico_enable_stdio_usb(hotline 0)

pico_add_extra_outputs(hotline)
