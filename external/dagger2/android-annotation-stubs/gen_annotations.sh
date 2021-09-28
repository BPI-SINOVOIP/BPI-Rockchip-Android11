#!/bin/bash

declare -A INNER
declare -A PARAMETER
declare -A IMPORT

ANNOTATIONS=(
    org.checkerframework.checker.nullness.compatqual.NullableDecl
    net.ltgt.gradle.incap.IncrementalAnnotationProcessor
)

PARAMETER["net.ltgt.gradle.incap.IncrementalAnnotationProcessor"]="IncrementalAnnotationProcessorType"
IMPORT["net.ltgt.gradle.incap.IncrementalAnnotationProcessor"]="net.ltgt.gradle.incap.IncrementalAnnotationProcessorType"

for a in ${ANNOTATIONS[@]}; do
    package=${a%.*}
    class=${a##*.}
    dir=$(dirname $0)/src/${package//.//}
    file=${class}.java
    inner=${INNER[$a]}
    parameter=${PARAMETER[$a]}
    import=

    if [ -n "${parameter}" ]; then
	parameter="${parameter} value();"
    fi

    for i in ${IMPORT[$a]}; do
	import="${import}import ${i};"
    done

    mkdir -p ${dir}
    sed -e"s/__PACKAGE__/${package}/" \
	-e"s/__CLASS__/${class}/" \
	-e"s/__INNER__/${inner}/" \
	-e"s/__PARAMETER__/${parameter}/" \
	-e"s/__IMPORT__/${import}/" \
	$(dirname $0)/tmpl.java > ${dir}/${file}
    google-java-format -i ${dir}/${file}
done

f=$(dirname $0)/src/net/ltgt/gradle/incap/IncrementalAnnotationProcessorType.java
cat > ${f} <<EOF
/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package net.ltgt.gradle.incap;
public enum IncrementalAnnotationProcessorType {
  DYNAMIC
}
EOF
