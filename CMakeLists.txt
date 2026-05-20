# Copyright 2026 University of Turin
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.14)
project(vpoline C)

option(BUILD_TESTS "Build tests" ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -pipe -rdynamic -Wall -Wno-unused-function -Wno-unused-result")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DDEBUG")
endif()

set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

set(CAPSTONE_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_STATIC ON CACHE BOOL "" FORCE)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86(_64)?)$")
    set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(riscv(32|64))$")
    set(CAPSTONE_RISCV_SUPPORT ON CACHE BOOL "" FORCE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CAPSTONE_AARCH64_SUPPORT ON CACHE BOOL "" FORCE)
endif ()

include(FetchContent)
FetchContent_Declare(capstone
        GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
        GIT_TAG 5.0.7
)

message(STATUS "Fetching Capstone v5.0.7...")
FetchContent_MakeAvailable(capstone)

set(VPOLINE_SOURCES
        src/main.c
        src/utils.c
        src/patcher.c
        src/sigsegv_handler.c
        src/gp_fault_handler.c
)

add_library(vpoline SHARED ${VPOLINE_SOURCES})

target_include_directories(vpoline PRIVATE
        ${capstone_SOURCE_DIR}/include
)

target_link_libraries(vpoline PRIVATE
        dl
        capstone
)

set_property(TARGET vpoline APPEND PROPERTY BUILD_RPATH "${capstone_BINARY_DIR}")
set_property(TARGET vpoline APPEND PROPERTY INSTALL_RPATH "${capstone_BINARY_DIR}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(syscall_tracker MODULE test/syscall_tracker.c)
set_target_properties(syscall_tracker PROPERTIES PREFIX "")

######################
# Test Configuration #
######################

if (BUILD_TESTS)

    enable_testing()

    # you should add test names here; CMake expects test source files to be
    # named <test_name>_test.c and hook libraries to be named hook_<test_name>_test.c
    set(TESTS
            fcntl
            write
    )

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

    foreach(T ${TESTS})
        set(TEST_EXE "${T}_test")
        set(HOOK_LIB "hook_${T}_test")

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/${TEST_EXE}.c")
            add_executable(${TEST_EXE} test/${TEST_EXE}.c)
        endif ()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/${HOOK_LIB}.c")
            add_library(${HOOK_LIB} MODULE test/${HOOK_LIB}.c)
            set_target_properties(${HOOK_LIB} PROPERTIES PREFIX "")
        endif()

        if(TARGET ${TEST_EXE} AND TARGET ${HOOK_LIB})
            add_test(NAME ${T} COMMAND ${TEST_EXE})
            set_tests_properties(${T} PROPERTIES ENVIRONMENT
                    "LD_PRELOAD=$<TARGET_FILE:vpoline>;LIBVPHOOK=$<TARGET_FILE:${HOOK_LIB}>")
        endif()
    endforeach()

    set(GP_TEST_EXE "gp_test")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/${GP_TEST_EXE}.c")
        add_executable(${GP_TEST_EXE} test/${GP_TEST_EXE}.c)
        target_link_libraries(${GP_TEST_EXE} PRIVATE m)

        add_test(NAME gp_test COMMAND ${GP_TEST_EXE})

        set_tests_properties(gp_test PROPERTIES ENVIRONMENT
                "LD_PRELOAD=$<TARGET_FILE:vpoline>")
    endif()

endif ()
