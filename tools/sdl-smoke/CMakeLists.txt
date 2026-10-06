cmake_minimum_required(VERSION 3.10)
project(sdl_smoke C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(sdl_smoke main.c)
target_link_libraries(sdl_smoke PkgConfig::SDL2 m)
set_target_properties(sdl_smoke PROPERTIES OUTPUT_NAME sdl-smoke.elf)

add_custom_target(deploy
    USES_TERMINAL
    COMMAND ${PS5_DEPLOY} sdl-smoke.elf
    DEPENDS sdl_smoke
)
