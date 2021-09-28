#!/bin/bash -e

# Copyright 2017 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script to handle the various ways soong may need to strip binaries
# Inputs:
#  Environment:
#   CLANG_BIN: path to the clang bin directory
#   CROSS_COMPILE: prefix added to readelf, objcopy tools
#   XZ: path to the xz binary
#  Arguments:
#   -i ${file}: input file (required)
#   -o ${file}: output file (required)
#   -d ${file}: deps file (required)
#   -k symbols: Symbols to keep (optional)
#   --add-gnu-debuglink
#   --keep-mini-debug-info
#   --keep-symbols
#   --keep-symbols-and-debug-frame
#   --remove-build-id

set -o pipefail

OPTSTRING=d:i:o:k:-:

usage() {
    cat <<EOF
Usage: strip.sh [options] -k symbols -i in-file -o out-file -d deps-file
Options:
        --add-gnu-debuglink             Add a gnu-debuglink section to out-file
        --keep-mini-debug-info          Keep compressed debug info in out-file
        --keep-symbols                  Keep symbols in out-file
        --keep-symbols-and-debug-frame  Keep symbols and .debug_frame in out-file
        --remove-build-id               Remove the gnu build-id section in out-file
EOF
    exit 1
}

do_strip() {
    # GNU strip --strip-all does not strip .ARM.attributes,
    # so we tell llvm-strip to keep it too.
    "${CLANG_BIN}/llvm-strip" --strip-all --keep-section=.ARM.attributes "${infile}" -o "${outfile}.tmp"
}

do_strip_keep_symbols_and_debug_frame() {
    REMOVE_SECTIONS=`"${CLANG_BIN}/llvm-readelf" -S "${infile}" | awk '/.debug_/ {if ($2 != ".debug_frame") {print "--remove-section " $2}}' | xargs`
    "${CLANG_BIN}/llvm-objcopy" "${infile}" "${outfile}.tmp" ${REMOVE_SECTIONS}
}

do_strip_keep_symbols() {
    REMOVE_SECTIONS=`"${CLANG_BIN}/llvm-readelf" -S "${infile}" | awk '/.debug_/ {print "--remove-section " $2}' | xargs`
    "${CLANG_BIN}/llvm-objcopy" "${infile}" "${outfile}.tmp" ${REMOVE_SECTIONS}
}

do_strip_keep_symbol_list() {
    echo "${symbols_to_keep}" | tr ',' '\n' > "${outfile}.symbolList"

    KEEP_SYMBOLS="--strip-unneeded-symbol=* --keep-symbols="
    KEEP_SYMBOLS+="${outfile}.symbolList"
    "${CROSS_COMPILE}objcopy" -w "${infile}" "${outfile}.tmp" ${KEEP_SYMBOLS}
}

do_strip_keep_mini_debug_info() {
    rm -f "${outfile}.dynsyms" "${outfile}.funcsyms" "${outfile}.keep_symbols" "${outfile}.debug" "${outfile}.mini_debuginfo" "${outfile}.mini_debuginfo.xz"
    local fail=
    "${CLANG_BIN}/llvm-strip" --strip-all --keep-section=.ARM.attributes --remove-section=.comment "${infile}" -o "${outfile}.tmp" || fail=true

    if [ -z $fail ]; then
        # Current prebult llvm-objcopy does not support --only-keep-debug flag,
        # and cannot process object files that are produced with the flag. Use
        # GNU objcopy instead for now. (b/141010852)
        "${CROSS_COMPILE}objcopy" --only-keep-debug "${infile}" "${outfile}.debug"
        "${CLANG_BIN}/llvm-nm" -D "${infile}" --format=posix --defined-only 2> /dev/null | awk '{ print $1 }' | sort >"${outfile}.dynsyms"
        "${CLANG_BIN}/llvm-nm" "${infile}" --format=posix --defined-only | awk '{ if ($2 == "T" || $2 == "t" || $2 == "D") print $1 }' | sort > "${outfile}.funcsyms"
        comm -13 "${outfile}.dynsyms" "${outfile}.funcsyms" > "${outfile}.keep_symbols"
        echo >> "${outfile}.keep_symbols" # Ensure that the keep_symbols file is not empty.
        "${CROSS_COMPILE}objcopy" --rename-section .debug_frame=saved_debug_frame "${outfile}.debug" "${outfile}.mini_debuginfo"
        "${CROSS_COMPILE}objcopy" -S --remove-section .gdb_index --remove-section .comment --keep-symbols="${outfile}.keep_symbols" "${outfile}.mini_debuginfo"
        "${CROSS_COMPILE}objcopy" --rename-section saved_debug_frame=.debug_frame "${outfile}.mini_debuginfo"
        "${XZ}" "${outfile}.mini_debuginfo"

        "${CLANG_BIN}/llvm-objcopy" --add-section .gnu_debugdata="${outfile}.mini_debuginfo.xz" "${outfile}.tmp"
        rm -f "${outfile}.dynsyms" "${outfile}.funcsyms" "${outfile}.keep_symbols" "${outfile}.debug" "${outfile}.mini_debuginfo" "${outfile}.mini_debuginfo.xz"
    else
        cp -f "${infile}" "${outfile}.tmp"
    fi
}

do_add_gnu_debuglink() {
    "${CLANG_BIN}/llvm-objcopy" --add-gnu-debuglink="${infile}" "${outfile}.tmp"
}

do_remove_build_id() {
    "${CLANG_BIN}/llvm-strip" --remove-section=.note.gnu.build-id "${outfile}.tmp" -o "${outfile}.tmp.no-build-id"
    rm -f "${outfile}.tmp"
    mv "${outfile}.tmp.no-build-id" "${outfile}.tmp"
}

while getopts $OPTSTRING opt; do
    case "$opt" in
        d) depsfile="${OPTARG}" ;;
        i) infile="${OPTARG}" ;;
        o) outfile="${OPTARG}" ;;
        k) symbols_to_keep="${OPTARG}" ;;
        -)
            case "${OPTARG}" in
                add-gnu-debuglink) add_gnu_debuglink=true ;;
                keep-mini-debug-info) keep_mini_debug_info=true ;;
                keep-symbols) keep_symbols=true ;;
                keep-symbols-and-debug-frame) keep_symbols_and_debug_frame=true ;;
                remove-build-id) remove_build_id=true ;;
                *) echo "Unknown option --${OPTARG}"; usage ;;
            esac;;
        ?) usage ;;
        *) echo "'${opt}' '${OPTARG}'"
    esac
done

if [ -z "${infile}" ]; then
    echo "-i argument is required"
    usage
fi

if [ -z "${outfile}" ]; then
    echo "-o argument is required"
    usage
fi

if [ -z "${depsfile}" ]; then
    echo "-d argument is required"
    usage
fi

if [ ! -z "${keep_symbols}" -a ! -z "${keep_mini_debug_info}" ]; then
    echo "--keep-symbols and --keep-mini-debug-info cannot be used together"
    usage
fi

if [ ! -z "${keep_symbols}" -a ! -z "${keep_symbols_and_debug_frame}" ]; then
    echo "--keep-symbols and --keep-symbols-and-debug-frame cannot be used together"
    usage
fi

if [ ! -z "${keep_mini_debug_info}" -a ! -z "${keep_symbols_and_debug_frame}" ]; then
    echo "--keep-symbols-mini-debug-info and --keep-symbols-and-debug-frame cannot be used together"
    usage
fi

if [ ! -z "${symbols_to_keep}" -a ! -z "${keep_symbols}" ]; then
    echo "--keep-symbols and -k cannot be used together"
    usage
fi

if [ ! -z "${add_gnu_debuglink}" -a ! -z "${keep_mini_debug_info}" ]; then
    echo "--add-gnu-debuglink cannot be used with --keep-mini-debug-info"
    usage
fi

rm -f "${outfile}.tmp"

if [ ! -z "${keep_symbols}" ]; then
    do_strip_keep_symbols
elif [ ! -z "${symbols_to_keep}" ]; then
    do_strip_keep_symbol_list
elif [ ! -z "${keep_mini_debug_info}" ]; then
    do_strip_keep_mini_debug_info
elif [ ! -z "${keep_symbols_and_debug_frame}" ]; then
    do_strip_keep_symbols_and_debug_frame
else
    do_strip
fi

if [ ! -z "${add_gnu_debuglink}" ]; then
    do_add_gnu_debuglink
fi

if [ ! -z "${remove_build_id}" ]; then
    do_remove_build_id
fi

rm -f "${outfile}"
mv "${outfile}.tmp" "${outfile}"

cat <<EOF > "${depsfile}"
${outfile}: \
  ${infile} \
  ${CROSS_COMPILE}objcopy \
  ${CLANG_BIN}/llvm-nm \
  ${CLANG_BIN}/llvm-objcopy \
  ${CLANG_BIN}/llvm-readelf \
  ${CLANG_BIN}/llvm-strip

EOF
