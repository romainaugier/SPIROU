# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

cmake_minimum_required(VERSION 3.15)

if(NOT VERSION)
    set(VERSION "0.0.0")
endif()

project(spirou VERSION ${VERSION} LANGUAGES C)

set(LIB_NAME "lib${PROJECT_NAME}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(RUN_TESTS EQUAL 1)
    message(STATUS "RUN_TESTS enabled, building and running tests")
    enable_testing()
endif()

set(libromano_DIR "${CMAKE_SOURCE_DIR}/ext/libromano/install/cmake")
find_package(libromano REQUIRED)

add_subdirectory(src)

if(RUN_TESTS EQUAL 1)
    add_subdirectory(tests)
endif()
