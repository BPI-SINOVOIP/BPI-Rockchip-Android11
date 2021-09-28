#!/bin/bash -eu
# Copyright 2017 Google Inc.
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

UNRAR_DEFINES="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DRAR_SMP -DRARDLL -DSILENT -DNOVOLUME"
UNRAR_WNOS="-Wno-logical-op-parentheses -Wno-switch -Wno-dangling-else"
UNRAR_SRC_DIR="$SRC/unrar"

# build 'lib'. This builds libunrar.a and libunrar.so
# -fPIC is required for successful compilation.
make CXX=$CXX CXXFLAGS="$CXXFLAGS -fPIC $UNRAR_DEFINES $UNRAR_WNOS" \
  -C $UNRAR_SRC_DIR lib

# remove the .so file so that the linker links unrar statically.
rm -v $UNRAR_SRC_DIR/libunrar.so

# build fuzzer
$CXX $CXXFLAGS -I. $UNRAR_SRC_DIR/unrar_fuzzer.cc -o $OUT/unrar_fuzzer \
     $UNRAR_DEFINES $LIB_FUZZING_ENGINE -L$UNRAR_SRC_DIR -lunrar
