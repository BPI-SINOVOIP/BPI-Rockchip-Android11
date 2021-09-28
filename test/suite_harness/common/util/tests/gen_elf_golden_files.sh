#!/bin/bash
# This script generats readelf output file for compatibility-common-util-tests.
# A typical flow is as follows:
# 1. add the file name in targetFile if to add a new object test file in ./assets
# 2. run: ./gen_elf_golden_files.sh
# 3. update ReadElfTest to use the new files
# 4. build: m release-parser compatibility-common-util-tests -j8
# 5. test: ./run_tests.sh
echo Generating ELF golden sample file for test via readelf
targetFile="arm32_libdl.so arm64_libdl.so x86app_process32 x86app_process64"
for file in $targetFile; do
    echo Processing $file
    readelf -a -p .rodata ./assets/$file > ./assets/${file/.so/}.txt
done