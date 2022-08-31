#!/bin/bash

BIN_DIR=/Users/michaelbuch/Git/lldb-build-main-modules/bin

pushd ${BIN_DIR}/..
[[ $(ninja | grep -c "ninja: build stopped") -ge 1 ]] && exit 1
popd

LLDB_BIN=${BIN_DIR}/lldb 

${LLDB_BIN} \
    -o "br se -p return -X main" \
    -o "r" \
    -o "var" \
    -o "q" \
    ${BIN_DIR}/astcontext-crashing-reduced
