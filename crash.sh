#!/bin/bash

BIN_DIR=~/Git/lldb-build-main-modules/bin
#BIN_DIR=~/Git/lldb-build-main/bin

CRASH_TARGET=astcontext-crashing-reduced

pushd ${BIN_DIR}/..
ninja
ninja ${CRASH_TARGET}
popd

LLDB_BIN=${BIN_DIR}/lldb 
#LLDB_BIN=lldb
#LLDB_BIN=~/Git/lldb-build-main-release/bin/lldb 

# lldb \
~/Git/lldb-build-main-release/bin/lldb \
    -o "settings set prompt \"(outer) \"" \
    -o "br se -f clang/lib/AST/RecordLayoutBuilder.cpp -l 414" \
    -o "br se -f clang/lib/AST/DeclBase.cpp -l 1563" \
    -o "br dis 2" \
    -- \
    ${LLDB_BIN} \
        -o "settings set prompt \"(inner) \"" \
        -o "br se -p return -X main" \
        -o "r" \
        -o "var" \
        ${BIN_DIR}/${CRASH_TARGET}

        #-o "log enable dwarf all -f a.log" \
        #-o "log enable lldb expr -f a.log" \
