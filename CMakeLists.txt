cmake_minimum_required(VERSION 3.24)
project(freeRTOS_thread_repro C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
include(FetchContent)
include(cmake/get_cpm.cmake)

option(USE_EXCEPTIONS "compile with exceptions for FreeRTOS tests" OFF)

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

    if (NOT USE_EXCEPTIONS)
        message(STATUS "Compiling *WITHOUT* exceptions...")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
    else()
        message(STATUS "Compiling *WITH* exceptions...")
    endif ()

    set(SANITIZE_OPTIONS -fsanitize=undefined,address,alignment,bounds,float-cast-overflow)
    add_compile_options(${SANITIZE_OPTIONS})
    add_link_options(${SANITIZE_OPTIONS})
endif ()

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
CPMAddPackage("gh:FreeRTOS/FreeRTOS-Kernel#main")

############### add snitch test framework ##########################################################
CPMAddPackage(
        NAME snitch
        GITHUB_REPOSITORY CrustyAuklet/snitch
        VERSION 1.2.5
        GIT_TAG embedded-head
        SYSTEM ON
        OPTIONS
        # Maximum Lengths for fixed allocation structures
        "SNITCH_MAX_TEST_CASES 64"                    # "Maximum number of test cases in a test application.
        "SNITCH_MAX_NESTED_SECTIONS 8"                # "Maximum depth of nested sections in a test case.
        "SNITCH_MAX_EXPR_LENGTH 256"                  # "Maximum length of a printed expression when reporting failure.
        "SNITCH_MAX_MESSAGE_LENGTH 256"               # "Maximum length of error or status messages.
        "SNITCH_MAX_TEST_NAME_LENGTH 64"              # "Maximum length of a test case name.
        "SNITCH_MAX_TAG_LENGTH 64"                    # "Maximum length of a test tag.
        "SNITCH_MAX_CAPTURES 8"                       # "Maximum number of captured expressions in a test case.
        "SNITCH_MAX_CAPTURE_LENGTH 256"               # "Maximum length of a captured expression.
        "SNITCH_MAX_UNIQUE_TAGS 64"                   # "Maximum number of unique tags in a test application.
        "SNITCH_MAX_COMMAND_LINE_ARGS 32"             # "Maximum number of command line arguments to a test application.
        "SNITCH_MAX_REGISTERED_REPORTERS 8"           # "Maximum number of registered reporter that can be selected from the command line.
        "SNITCH_MAX_PATH_LENGTH 64"                   # "Maximum length of a file path when writing output to file.
        # Feature toggles.
        "SNITCH_DEFINE_MAIN OFF"                      # use our own main for host/target differences
        # exceptions will be automatically detected, on for host and off if the target has them off
        "SNITCH_APPEND_TO_CHARS OFF"                  # to_char for float uses an algorithm with a MASSIVE lookup table, use the basic constexpr functions
        "SNITCH_WITH_MULTITHREADING OFF"              # disables the use of thread_local, which triggers a dynamic allocation
        "SNITCH_DEFAULT_WITH_COLOR OFF"               # Enable terminal colors by default -- can also be controlled by command line interface.
        "SNITCH_WITH_ALL_REPORTERS OFF"               # Allow all built-in reporters to be selected from the command line -- disable for faster compilation.
        "SNITCH_WITH_TEAMCITY_REPORTER OFF"           # Allow the TeamCity reporter to be selected from the command line -- enable if needed.
        "SNITCH_WITH_CATCH2_XML_REPORTER ON"          # Allow the Catch2 XML reporter to be selected from the command line -- enable if needed.
)

############### Simple test program ################################################################
add_executable(thread_abstraction_test
               static_allocation.c
               include/seal/thread.cpp
               test_main.cpp
               tests.cpp)
target_include_directories(thread_abstraction_test PUBLIC include)
target_link_libraries(thread_abstraction_test PRIVATE freertos_kernel snitch::snitch)
