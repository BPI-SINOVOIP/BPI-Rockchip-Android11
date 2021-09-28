#!/bin/bash -eu
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
# Ensure SRC and WORK are set
test "${SRC}" != "" || exit 1
test "${WORK}" != "" || exit 1
test "${OUT}" != "" || exit 1

# Build libmpeg2
build_dir=$WORK/build
rm -rf ${build_dir}
mkdir -p ${build_dir}
pushd ${build_dir}

cmake $SRC/libmpeg2
make -j$(nproc)
popd

# build fuzzers
$CXX $CXXFLAGS -std=c++11 \
-I$SRC/libmpeg2 \
-I$SRC/libmpeg2/common \
-I$SRC/libmpeg2/decoder \
-I${build_dir} \
-Wl,--start-group \
$LIB_FUZZING_ENGINE \
$SRC/libmpeg2/fuzzer/mpeg2_dec_fuzzer.cpp -o $OUT/mpeg2_dec_fuzzer \
${build_dir}/libmpeg2dec.a \
-Wl,--end-group

cp $SRC/mpeg2_dec_fuzzer_seed_corpus.zip $OUT/mpeg2_dec_fuzzer_seed_corpus.zip
cp $SRC/libmpeg2/fuzzer/mpeg2_dec_fuzzer.dict $OUT/mpeg2_dec_fuzzer.dict
