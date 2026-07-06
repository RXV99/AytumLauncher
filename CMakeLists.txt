cmake_minimum_required(VERSION 3.10)
project(vita-java-me C CXX)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(NOT DEFINED ENV{VITASDK} AND NOT DEFINED VITASDK)
    message(FATAL_ERROR "VITASDK not set. Set env var or pass -DVITASDK=")
endif()
if(NOT DEFINED VITASDK)
    set(VITASDK $ENV{VITASDK})
endif()

set(CMAKE_TOOLCHAIN_FILE "${VITASDK}/share/vita.toolchain.cmake"
    CACHE PATH "Vita SDK toolchain")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

add_definitions(-DPSP2 -D__PSP2__ -DVITA -D_JNI_IMPLEMENTATION_)

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
    -mfloat-abi=hard -mfpu=neon-vfpv4
)

set_target_properties(vita-java-me PROPERTIES
    OUTPUT_NAME "vita-java-me"
    SUFFIX ".elf"
)

# Package: copy sce_sys next to ELF, then create VPK
add_custom_command(TARGET vita-java-me POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/sce_sys
        $<TARGET_FILE_DIR:vita-java-me>/sce_sys
    COMMENT "Copying sce_sys"
)

include("${VITASDK}/share/vita.cmake" OPTIONAL)

vita_create_vpk(vita-java-me.vpk "JAVAME00001" "Java ME Emulator" "01.00"
    FILE vita-java-me.elf
    DATA_DIR sce_sys
)
