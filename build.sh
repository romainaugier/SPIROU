#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 - Present Romain Augier
# All rights reserved.

BUILDTYPE="Release"
RUNTESTS=0
REMOVEOLDDIR=0
EXPORTCOMPILECOMMANDS=0
VERSION="0.0.0"
INSTALL=0
INSTALLDIR="$PWD/install"

log_info()
{
    echo "[INFO] : $1"
}

log_error()
{
    echo "[ERROR] : $1"
}

parse_args()
{
    case "$1" in
        --debug) BUILDTYPE="Debug" ;;
        --reldebug) BUILDTYPE="RelWithDebInfo" ;;
        --tests) RUNTESTS=1 ;;
        --clean) REMOVEOLDDIR=1 ;;
        --install) INSTALL=1 ;;
        --export-compile-commands) EXPORTCOMPILECOMMANDS=1 ;;
        --version:*)
            VERSION="${1#*:}"
            log_info "Version specified by user: $VERSION"
            ;;
        --installdir:*)
            INSTALLDIR="${1#*:}"
            log_info "Install directory specified by user: $INSTALLDIR"
            ;;
    esac
}

log_info "Building spirou"

for arg in "$@"
do
    parse_args "$arg"
done

log_info "Build type: $BUILDTYPE"
log_info "Build version: $VERSION"

if [[ $REMOVEOLDDIR -eq 1 ]]; then
    log_info "Removing old build and install directories"
    rm -rf build install
fi

ROMANOFLAGS="--install"
ROMANOLIB="ext/libromano/install/lib/libromano"

if [[ "$BUILDTYPE" == "Debug" ]]; then
    ROMANOFLAGS="--install --debug"
    ROMANOLIB="${ROMANOLIB}d"
fi

if ! ls ${ROMANOLIB}.* > /dev/null 2>&1; then
    log_info "Building libromano ($BUILDTYPE)"

    if [[ ! -d "ext/libromano/ext/concurrentqueue/cqinstall" ]]; then
        (cd ext/libromano/ext/concurrentqueue && ./build_c_api.sh)
    fi

    (cd ext/libromano && rm -rf build && ./build.sh $ROMANOFLAGS)

    if [[ $? -ne 0 ]]; then
        log_error "Error while building libromano"
        exit 1
    fi
fi

cmake -S . -B build -DRUN_TESTS=$RUNTESTS -DCMAKE_EXPORT_COMPILE_COMMANDS=$EXPORTCOMPILECOMMANDS -DCMAKE_BUILD_TYPE=$BUILDTYPE -DVERSION=$VERSION

if [[ $? -ne 0 ]]; then
    log_error "Error during CMake configuration"
    exit 1
fi

cmake --build build -- -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)

if [[ $? -ne 0 ]]; then
    log_error "Error during CMake build"
    exit 1
fi

if [[ $RUNTESTS -eq 1 ]]; then
    (cd build && ctest --output-on-failure)

    if [[ $? -ne 0 ]]; then
        log_error "Error during CMake testing"
        exit 1
    fi
fi

if [[ $INSTALL -eq 1 ]]; then
    cmake --install build --config $BUILDTYPE --prefix $INSTALLDIR

    if [[ $? -ne 0 ]]; then
        log_error "Error during CMake installation"
        exit 1
    fi
fi

if [[ $EXPORTCOMPILECOMMANDS -eq 1 ]]; then
    cp ./build/compile_commands.json ./compile_commands.json
    log_info "Copied compile_commands.json to root directory"
fi
