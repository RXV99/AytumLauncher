cmake_minimum_required(VERSION 3.10)
project(vita-java-me C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

add_definitions(-DPSP2 -D__PSP2__ -DVITA)

set(SOURCES
    src/main.c
    src/launcher.c
    src/jad_parser.c
    src/jvm_core.c
    src/midp_api.c
    src/graphics.c
    src/input.c
    src/fileio.c
    src/network.c
    src/audio.c
    src/decoder.c
    src/pvf_stubs.c
)

add_executable(vita-java-me ${SOURCES})

target_link_libraries(vita-java-me PRIVATE
    vita2d
    SceNet_stub SceNetCtl_stub
    SceAudio_stub SceLibKernel_stub
    SceCtrl_stub SceTouch_stub
    SceDisplay_stub SceGxm_stub
    SceAppMgr_stub SceCommonDialog_stub
    m
)

target_compile_options(vita-java-me PRIVATE
    -Wall -Wextra -Wno-unused-parameter -O2
)

set_target_properties(vita-java-me PROPERTIES
    OUTPUT_NAME "vita-java-me"
    SUFFIX ".elf"
)
