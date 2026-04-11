## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# BinaryToHeader.cmake — Convert a binary file to a C header with a uint8_t array.
# Usage: cmake -DINPUT=file.bin -DOUTPUT=file.h -DVAR=VARNAME -P BinaryToHeader.cmake

file(READ "${INPUT}" content HEX)
string(LENGTH "${content}" hex_length)
math(EXPR byte_count "${hex_length} / 2")

# Format as comma-separated hex bytes
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " hex_formatted "${content}")
string(REGEX REPLACE ", $" "" hex_formatted "${hex_formatted}")

file(WRITE "${OUTPUT}"
    "// Auto-generated from ${INPUT} — do not edit\n"
    "#pragma once\n"
    "#include <cstddef>\n"
    "#include <cstdint>\n\n"
    "// NOLINTNEXTLINE\n"
    "static const uint8_t ${VAR}_DATA[] = {\n"
    "    ${hex_formatted}\n"
    "};\n"
    "static const size_t ${VAR}_SIZE = ${byte_count};\n"
)
