# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

function(set_target_options target_name)
    set_target_properties(${target_name} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(COMPILE_OPTIONS -Wall
                            -Wextra
                            -Wno-unused-parameter
                            -pedantic-errors
                            $<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address -fsanitize=undefined>
                            $<$<CONFIG:Release,RelWithDebInfo>:-O3>)

        target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:C>:${COMPILE_OPTIONS}>)
        target_link_options(${target_name} PRIVATE $<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address -fsanitize=undefined>)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        # 4710 is "Function not inlined"
        # 5045 is "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"
        # 4201 is "nonstandard extension used: nameless struct/union"
        # 4206 is "nonstandard extension used: translation unit is empty"
        set(COMPILE_OPTIONS /W4
                            /wd4710
                            /wd5045
                            /wd4201
                            /wd4206
                            $<$<CONFIG:Debug,RelWithDebInfo>:/fsanitize=address>
                            $<$<CONFIG:Release,RelWithDebInfo>:/O2 /GF /Ot /Oy /GT /GL /Oi /Zi /Gm- /Zc:inline>)

        target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:C>:${COMPILE_OPTIONS}>)
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "/ignore:4300")
    endif()

    target_compile_definitions(${target_name} PRIVATE $<$<CONFIG:Debug>:DEBUG_BUILD>)
endfunction()
