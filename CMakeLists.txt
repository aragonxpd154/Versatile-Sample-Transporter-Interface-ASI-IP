# CMakeLists.txt - VSTI (Versatile Sample Transporter Interface)
#
# Equivalente ao Makefile, para quem prefere CMake ou precisa integrar o
# projeto como subdiretorio de outro build.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.13)

project(vsti
    VERSION 1.0.0
    DESCRIPTION "Encapsulamento de MPEG-2 Transport Stream sobre IP"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(VSTI_SANITIZERS "Compilar com AddressSanitizer e UBSan" OFF)
option(VSTI_WERROR     "Tratar avisos como erros"              OFF)

# ---------------------------------------------------------------------
# Biblioteca do nucleo
#
# Separar o nucleo da CLI permite que os testes liguem direto contra a
# biblioteca, sem passar pelo binario, e abre caminho para reaproveitar o
# parser em outros programas.
# ---------------------------------------------------------------------

add_library(vsti_core STATIC
    src/ts.c
    src/psi.c
    src/desc.c
    src/rtp.c
    src/net.c
    src/pacing.c
    src/reader.c
    src/log.c)

target_include_directories(vsti_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(vsti_core PRIVATE _DEFAULT_SOURCE)

set(VSTI_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow -Wcast-align
    -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
    -Wwrite-strings -Wno-unused-parameter)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vsti_core PRIVATE ${VSTI_WARNINGS})
    if(VSTI_WERROR)
        target_compile_options(vsti_core PRIVATE -Werror)
    endif()
endif()

if(VSTI_SANITIZERS)
    target_compile_options(vsti_core PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(vsti_core PUBLIC -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------
# Executavel
# ---------------------------------------------------------------------

add_executable(vsti
    src/cli.c
    src/cmd_stream.c
    src/cmd_receive.c
    src/cmd_analyze.c
    src/main.c)

target_link_libraries(vsti PRIVATE vsti_core)
target_include_directories(vsti PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(vsti PRIVATE _DEFAULT_SOURCE)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vsti PRIVATE ${VSTI_WARNINGS})
endif()

install(TARGETS vsti RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------
# Testes
# ---------------------------------------------------------------------

include(CTest)

if(BUILD_TESTING)
    foreach(t crc32 ts psi rtp desc)
        add_executable(test_${t} tests/test_${t}.c)
        target_link_libraries(test_${t} PRIVATE vsti_core)
        target_include_directories(test_${t} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_test(NAME unit_${t} COMMAND test_${t})
    endforeach()

    # O teste de integracao roda a partir da raiz do projeto porque procura as
    # capturas em one-seg/.
    add_test(NAME integration
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.sh
                     $<TARGET_FILE:vsti>
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
