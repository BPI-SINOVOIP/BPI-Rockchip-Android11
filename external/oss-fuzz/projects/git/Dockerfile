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
MAINTAINER steadmon@google.com
RUN apt-get update && \
    apt-get install --no-install-recommends -y \
        cvs cvsps gettext libcgi-pm-perl libcurl4-gnutls-dev \
        libdbd-sqlite3-perl liberror-perl libexpat1-dev libhttp-date-perl \
        libio-pty-perl libmailtools-perl libpcre2-dev libpcre3-dev libsvn-perl \
        libtime-modules-perl libyaml-perl libz-dev python subversion tcl unzip \
        asciidoc docbook-xsl xmlto libssl-dev zip
RUN git clone https://github.com/git/git git
WORKDIR git
RUN git checkout origin/next
COPY build.sh $SRC/
