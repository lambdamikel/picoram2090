project(sram)

cmake_minimum_required(VERSION 3.25)

add_executable(sram
        sram.c
        )

# pull in common dependencies
target_link_libraries(sram pico_stdlib)

# create map/bin/hex file etc.
pico_add_extra_outputs(sram)

# add url via pico_set_program_url
example_auto_set_url(sram)

