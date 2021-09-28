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

export ASAN_OPTIONS="detect_leaks=0"

if [[ $CFLAGS = *sanitize=address* ]]
then
    export CXXFLAGS="$CXXFLAGS -DASAN"
fi

if [[ $CFLAGS = *sanitize=memory* ]]
then
    export CXXFLAGS="$CXXFLAGS -DMSAN"
fi

export CXXFLAGS="$CXXFLAGS -D_GLIBCXX_DEBUG"

# Build libogg
mkdir $SRC/libogg-install
cd $SRC/ogg
./autogen.sh
./configure --prefix="$SRC/libogg-install"
make -j$(nproc)
make install

# Build libflac
cd $SRC/flac/
./autogen.sh
if [[ $CFLAGS = *sanitize=memory* ]]
then
    LD_LIBRARY_PATH="$SRC/libogg-install/lib" ./configure --with-ogg="$SRC/libogg-install" --enable-static --disable-oggtest --disable-examples --disable-xmms-plugin --disable-asm-optimizations --disable-sse
else
    LD_LIBRARY_PATH="$SRC/libogg-install/lib" ./configure --with-ogg="$SRC/libogg-install" --enable-static --disable-oggtest --disable-examples --disable-xmms-plugin
fi
make -j$(nproc)

cd $SRC/fuzzing-headers
./install.sh

# Build fuzzers
cd $SRC/flac-fuzzers/
$CXX $CXXFLAGS -I $SRC/flac/include/ fuzzer_decoder.cpp $SRC/flac/src/libFLAC++/.libs/libFLAC++.a $SRC/flac/src/libFLAC/.libs/libFLAC.a $SRC/libogg-install/lib/libogg.a $LIB_FUZZING_ENGINE -o $OUT/fuzzer_decoder
$CXX $CXXFLAGS -I $SRC/flac/include/ fuzzer_encoder.cpp $SRC/flac/src/libFLAC++/.libs/libFLAC++.a $SRC/flac/src/libFLAC/.libs/libFLAC.a $SRC/libogg-install/lib/libogg.a $LIB_FUZZING_ENGINE -o $OUT/fuzzer_encoder
cp fuzzer_encoder.dict $OUT/
