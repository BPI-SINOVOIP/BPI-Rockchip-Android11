#!/bin/bash -eu
#
# Copyright 2018 Google Inc.
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

mkdir -p ${WORK}/tidy-html5
cd ${WORK}/tidy-html5

cmake -GNinja ${SRC}/tidy-html5/
ninja

for fuzzer in tidy_config_fuzzer tidy_fuzzer; do
    ${CC} ${CFLAGS} -c -I${SRC}/tidy-html5/include \
        $SRC/${fuzzer}.c -o ${fuzzer}.o
    ${CXX} ${CXXFLAGS} -std=c++11 ${fuzzer}.o \
        -o $OUT/${fuzzer} \
        $LIB_FUZZING_ENGINE libtidys.a
done

cp ${SRC}/*.options ${OUT}/
