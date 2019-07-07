#!/bin/sh

set -e


# @see https://stackoverflow.com/a/45444278
ELVM_INPUT_FILE=`mktemp`
cp /dev/stdin "${ELVM_INPUT_FILE}"


# @see https://stackoverflow.com/a/1638397
RUN_CMAKE_SCRIPT=$(readlink -f "$0")
TOOLS_DIRECTORY=`dirname "$RUN_CMAKE_SCRIPT"`
OUT_DIRECTORY="$TOOLS_DIRECTORY/../out"
ELVM_PUTC_HELPER="$OUT_DIRECTORY/cmake_putc_helper"


cmake "-DELVM_INPUT_FILE:PATH=${ELVM_INPUT_FILE}" "-DELVM_PUTC_HELPER:PATH=${ELVM_PUTC_HELPER}" -P "$1"

