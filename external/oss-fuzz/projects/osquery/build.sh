#!/bin/bash -eu
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

PROJECT=osquery

# Move the project content into the current overlay.
# CMake builtin 'rename' will attempt a hardlink.
( cd / &&\
  mv "${SRC}/${PROJECT}" "${SRC}/${PROJECT}-dev" &&\
  mv "${SRC}/${PROJECT}-dev" "${SRC}/${PROJECT}" )

pushd "${SRC}/${PROJECT}"

# Prefer shared libs
sed -i 's/CMAKE_LINK_SEARCH_START_STATIC ON/CMAKE_LINK_SEARCH_START_STATIC OFF/g' cmake/flags.cmake
sed -i 's/CMAKE_LINK_SEARCH_END_STATIC ON/CMAKE_LINK_SEARCH_END_STATIC OFF/g' cmake/flags.cmake

mkdir build && pushd build

cmake \
  -DOSQUERY_VERSION:string=0.0.0-fuzz \
  -DOSQUERY_ENABLE_ADDRESS_SANITIZER:BOOL=ON \
  -DOSQUERY_ENABLE_FUZZER_SANITIZERS:BOOL=ON \
  ..
cmake \
  "-DCMAKE_EXE_LINKER_FLAGS=${LIB_FUZZING_ENGINE} -Wl,-rpath,'\$ORIGIN/lib'" \
  ..

# Build harnesses
cmake --build . -j$(nproc) --target osqueryfuzz-config
cmake --build . -j$(nproc) --target osqueryfuzz-sqlquery

# Cleanup
find . -type f -name '*.o' -delete
rm -rf "${SRC}/${PROJECT}/libraries/cmake/source/libudev/src/test"
rm -rf libs/src/patched-source/libudev/src/test

# Move libunwind to output path
mkdir -p "${OUT}/lib"
cp /usr/lib/x86_64-linux-gnu/libunwind.so.8 "${OUT}/lib"

# Move harnesses to output path
cp osquery/main/harnesses/osqueryfuzz-config "${OUT}/osqueryfuzz-config"
cp osquery/main/harnesses/osqueryfuzz-sqlquery "${OUT}/osqueryfuzz-sqlquery"

# Build supporting files
popd
tools/harnesses/osqueryfuzz_config_corpus.sh "${OUT}/osqueryfuzz-config_seed_corpus.zip"
tools/harnesses/osqueryfuzz_config_dict.sh "${OUT}/osqueryfuzz-config.dict"
tools/harnesses/osqueryfuzz_sqlquery_corpus.sh "${OUT}/osqueryfuzz-sqlquery_seed_corpus.zip"
cp tools/harnesses/osqueryfuzz_sqlquery.dict "${OUT}/osqueryfuzz-sqlquery.dict"
