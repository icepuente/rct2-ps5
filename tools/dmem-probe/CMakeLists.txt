cmake_minimum_required(VERSION 3.10)
project(dmem_probe C)

add_executable(dmem_probe main.c)
set_target_properties(dmem_probe PROPERTIES OUTPUT_NAME dmem-probe.elf)
