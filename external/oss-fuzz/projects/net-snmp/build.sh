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
./configure --with-openssl=/usr --with-defaults --with-logfile="/dev/null" --with-persistent-directory="/dev/null"
# net-snmp build is not parallel-make safe; do not add -j
make

# build fuzzers (remember to link statically)
$CC $CFLAGS -c -Iinclude $SRC/snmp_pdu_parse_fuzzer.c -o $WORK/snmp_pdu_parse_fuzzer.o
$CXX $CXXFLAGS $WORK/snmp_pdu_parse_fuzzer.o \
      $LIB_FUZZING_ENGINE snmplib/.libs/libnetsnmp.a \
      -Wl,-Bstatic -lcrypto -Wl,-Bdynamic -lm \
      -o $OUT/snmp_pdu_parse_fuzzer

$CC $CFLAGS -c -Iinclude -Iagent/mibgroup/agentx $SRC/agentx_parse_fuzzer.c -o $WORK/agentx_parse_fuzzer.o
$CXX $CXXFLAGS $WORK/agentx_parse_fuzzer.o \
      $LIB_FUZZING_ENGINE snmplib/.libs/libnetsnmp.a \
      agent/.libs/libnetsnmpagent.a \
      -Wl,-Bstatic -lcrypto -Wl,-Bdynamic -lm \
      -o $OUT/agentx_parse_fuzzer
