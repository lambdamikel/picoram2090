project(sram)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

cmake_minimum_required(VERSION 3.25)

pico_sdk_init()

# Tell CMake where to find other source code
add_subdirectory(libs/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)

add_executable(sram
        sram.c
        hw_config.c
        ) 

# pull in common dependencies
target_link_libraries(sram pico_stdlib hardware_i2c pico_multicore FatFs_SPI hardware_adc hardware_pwm hardware_spi)

# create map/bin/hex file etc.
pico_add_extra_outputs(sram)

# add url via pico_set_program_url
example_auto_set_url(sram)

