# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

include(target_options)

file(GLOB TEST_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)

foreach(test_file ${TEST_FILES})
    get_filename_component(TESTNAME ${test_file} NAME_WLE)
    message(STATUS "Adding spirou test : ${TESTNAME}")

    add_executable(${TESTNAME} ${test_file})
    set_target_options(${TESTNAME})
    target_include_directories(${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TESTNAME} ${LIB_NAME})

    add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
endforeach()
