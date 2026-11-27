cmake_minimum_required(VERSION 3.20)
project(toolchain C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SANITIZERS "Enable ASan and UBSan" ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Shared headers (isa.h, as.h, cc.h)
add_library(common INTERFACE)
target_include_directories(common INTERFACE include)

# Assembler library + standalone CLI
file(GLOB_RECURSE AS_SOURCES CONFIGURE_DEPENDS "src/as/*.c")
list(REMOVE_ITEM AS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/as/main.c")

add_library(libas STATIC ${AS_SOURCES})
target_link_libraries(libas PUBLIC common)

add_executable(as src/as/main.c)
target_link_libraries(as PRIVATE libas)

# Compiler library + CLI (embeds libas)
file(GLOB_RECURSE CC_SOURCES CONFIGURE_DEPENDS "src/cc/*.c")
list(REMOVE_ITEM CC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cc/main.c")

add_library(libcc STATIC ${CC_SOURCES})
target_link_libraries(libcc PUBLIC common libas)

add_executable(cc src/cc/main.c)
target_link_libraries(cc PRIVATE libcc)