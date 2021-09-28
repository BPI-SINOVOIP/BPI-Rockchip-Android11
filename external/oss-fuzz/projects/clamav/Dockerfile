# Copyright (C) 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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
MAINTAINER clamav.fuzz@gmail.com
RUN apt-get update && apt-get install -y libssl-dev libcurl4-openssl-dev
RUN git clone --depth 1 https://github.com/Cisco-Talos/clamav-devel.git
RUN git clone --depth 1 https://github.com/Cisco-Talos/clamav-fuzz-corpus.git

WORKDIR clamav-devel
COPY build.sh $SRC/
