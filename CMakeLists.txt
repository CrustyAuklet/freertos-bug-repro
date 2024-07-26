cmake_minimum_required(VERSION 3.24)
project(freeRTOS_thread_repro C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

############### add warning flags and sanitizers ###################################################
if (NOT MSVC AND NOT MINGW)
    add_compile_options(
            -Og
            # Linker related options
            -fdata-sections
            -ffunction-sections
            # Optimization
            #        -finline-limit=10000
            -fstack-usage
            #        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
            # warning and error flags
            -Wall
            -Wextra
            -Werror=return-type
            -Werror=double-promotion
            -Wfloat-conversion
            -Wno-unused-parameter
            -Wno-unused-macros
            -Wno-psabi  # don't warn about GCC ABI changes
            -fno-omit-frame-pointer
            -fno-common
            -Wall
            -Wextra
            $<$<COMPILE_LANGUAGE:CXX>:-fstrict-enums>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>
    )

    set(SANITIZE_OPTIONS -fsanitize=undefined,address,alignment,bounds,float-cast-overflow)
    add_compile_options(${SANITIZE_OPTIONS})
    add_link_options(${SANITIZE_OPTIONS})
endif ()

include(FetchContent)

############### FreeRTOS config library ############################################################
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE include)
target_compile_definitions(freertos_config INTERFACE
                           projCOVERAGE_TEST=0
                           configTOTAL_HEAP_SIZE=0x100000)

set(FREERTOS_HEAP "3" CACHE STRING "" FORCE)
if(WIN32)
    target_compile_definitions(freertos_config INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
    set(FREERTOS_PORT "MSVC_MINGW" CACHE STRING "" FORCE)
else()
    set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)
endif()

############### add the FreeRTOS kernel ############################################################
FetchContent_Declare(freertos_kernel
                     GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
                     GIT_TAG        main)
FetchContent_MakeAvailable(freertos_kernel)

############### add snitch test framework ##########################################################
FetchContent_Declare(snitch
                     GIT_REPOSITORY https://github.com/snitch-org/snitch.git
                     GIT_TAG        v1.2.5)
FetchContent_MakeAvailable(snitch)

############### our test program ###################################################################
add_executable(thread_tests
               main.cpp)
target_include_directories(thread_tests PUBLIC include)
target_link_libraries(thread_tests PRIVATE freertos_kernel)
