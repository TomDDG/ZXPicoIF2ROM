cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(oled C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#set(PICO_BOARD pico_w)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(picoif2 picoif2.c)

pico_generate_pio_header(picoif2 ${CMAKE_CURRENT_LIST_DIR}/picoif2.pio)

target_link_libraries(picoif2 pico_stdlib hardware_pio hardware_dma)

pico_enable_stdio_usb(picoif2 1) 
pico_enable_stdio_uart(picoif2 0) 

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(picoif2)

