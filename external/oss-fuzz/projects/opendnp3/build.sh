#!/bin/bash -eu
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

# build project
cmake -DDNP3_FUZZING=ON -DSTATICLIBS=ON .
make -j$(nproc) all

cd cpp/tests/fuzz

TARGETS="fuzzdecoder \
  fuzzoutstation \
  fuzzmaster"

for target in $TARGETS; do
  # build corpus
  zip -r ${target}_seed_corpus.zip corpus/*.dnp
  cp ${target}_seed_corpus.zip $OUT/

  # export other associated stuff
  cp fuzzdnp3.options $OUT/${target}.options

  # build fuzz target
  $CXX $CXXFLAGS -I. -I ../../libs/include/ -I ../../tests/libs/src/ -I ../../libs/src/ -c ${target}.cpp -o ${target}.o
  $CXX $CXXFLAGS -std=c++14 ${target}.o -o $OUT/${target} ../../../libdnp3mocks.a ../../../libtestlib.a ../../../libasiodnp3.a ../../../libdnp3decode.a ../../../libasiopal.a ../../../libopendnp3.a ../../../libopenpal.a $LIB_FUZZING_ENGINE
done
