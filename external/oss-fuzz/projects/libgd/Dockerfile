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

FROM gcr.io/oss-fuzz-base/base-builder
MAINTAINER ossfuzz@tds.xyz
RUN apt-get update && \
    apt-get install -y make autoconf automake libtool pkg-config libz-dev
RUN git clone --depth 1 https://github.com/libgd/libgd
ADD http://lcamtuf.coredump.cx/afl/demo/afl_testcases.tgz $SRC/afl_testcases.tgz
WORKDIR libgd
COPY build.sh parser_target.cc $SRC/
