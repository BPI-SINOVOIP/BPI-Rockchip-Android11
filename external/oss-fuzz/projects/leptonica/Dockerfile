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
MAINTAINER taking@google.com
RUN apt-get update && apt-get install -y make autoconf automake libtool \
  pkg-config cmake nasm
RUN git clone --depth 1 https://github.com/DanBloomberg/leptonica.git leptonica
RUN git clone --depth 1 https://github.com/madler/zlib.git zlib
RUN git clone --depth 1 https://gitlab.com/libtiff/libtiff libtiff
RUN git clone --depth 1 https://github.com/glennrp/libpng.git libpng
RUN git clone --depth 1 https://chromium.googlesource.com/webm/libwebp libwebp
RUN git clone https://www.cl.cam.ac.uk/~mgk25/git/jbigkit jbigkit
RUN git clone --depth 1 https://github.com/libjpeg-turbo/libjpeg-turbo libjpeg-turbo
RUN git clone --depth 1 https://github.com/facebook/zstd zstd
WORKDIR leptonica
COPY build.sh pix_rotate_shear_fuzzer.cc $SRC/
