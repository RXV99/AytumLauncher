cmake_minimum_required(VERSION 3.10)
project(vita-java-me C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(DEFINED ENV{VITASDK})
    set(VITASDK $ENV{VITASDK})
else()
    set(VITASDK "/usr/local/vitasdk")
endif()

set(CMAKE_TOOLCHAIN_FILE "${VITASDK}/share/vita.toolchain.cmake"
    CACHE PATH "Vita SDK toolchain")

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
)

add_executable(vita-java-me ${SOURCES})

target_link_libraries(vita-java-me PRIVATE
    SceNet SceNetCtl SceAppMgr SceCommonDialog
    SceLibKernel SceLibc SceSysmodule ScePgf
    SceAudio
    vita2d m
)

target_compile_options(vita-java-me PRIVATE
    -Wall -Wextra -Wno-unused-parameter -O2
)

set_target_properties(vita-java-me PROPERTIES
    OUTPUT_NAME "vita-java-me"
    SUFFIX ".elf"
)
