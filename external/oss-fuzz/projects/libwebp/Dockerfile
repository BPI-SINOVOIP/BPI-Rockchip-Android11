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
MAINTAINER yguyon@google.com
RUN apt-get update && apt-get install -y autoconf make libtool zip
RUN git clone https://chromium.googlesource.com/webm/libwebp
RUN git clone https://chromium.googlesource.com/webm/libwebp-test-data
ADD https://storage.googleapis.com/downloads.webmproject.org/webp/testdata/fuzzer/fuzz_seed_corpus.zip $SRC/
RUN unzip fuzz_seed_corpus.zip -d libwebp-test-data/
RUN rm fuzz_seed_corpus.zip
COPY build.sh fuzz.h fuzz.dict $SRC/
COPY img_alpha.h img_grid.h img_peak.h $SRC/
COPY fuzz_simple_api.c $SRC/
COPY fuzz_advanced_api.c $SRC/
COPY fuzz_animation_api.c $SRC/
COPY fuzz_webp_animencoder.cc $SRC/
COPY fuzz_demux_api.c $SRC/
COPY fuzz_webp_enc_dec.cc $SRC/
WORKDIR libwebp
