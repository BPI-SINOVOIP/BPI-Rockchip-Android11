#!/bin/bash -e

if [ -z $1 ]; then
    echo "usage: $0 <build number>"
    exit 1
fi

readonly BUILD_NUMBER=$1

cd "$(dirname $0)"

if ! git diff HEAD --quiet; then
    echo "must be run with a clean prebuilts/build-tools project"
    exit 1
fi

readonly tmpdir=$(mktemp -d)

function finish {
    if [ ! -z "${tmpdir}" ]; then
        rm -rf "${tmpdir}"
    fi
}
trap finish EXIT

function fetch_artifact() {
    /google/data/ro/projects/android/fetch_artifact --bid ${BUILD_NUMBER} --target $1 "$2" "$3"
}

fetch_artifact linux build-prebuilts.zip "${tmpdir}/linux.zip"
fetch_artifact linux build-common-prebuilts.zip "${tmpdir}/common.zip"
fetch_artifact linux manifest_${BUILD_NUMBER}.xml "${tmpdir}/manifest.xml"
fetch_artifact darwin_mac build-prebuilts.zip "${tmpdir}/darwin.zip"

function unzip_to() {
    rm -rf "$1"
    mkdir "$1"
    unzip -q -d "$1" "$2"
}

unzip_to linux-x86 "${tmpdir}/linux.zip"
unzip_to common "${tmpdir}/common.zip"
unzip_to darwin-x86 "${tmpdir}/darwin.zip"

cp -f "${tmpdir}/manifest.xml" manifest.xml

git add manifest.xml linux-x86 darwin-x86 common
git commit -m "Update build-tools to ab/${BUILD_NUMBER}

https://ci.android.com/builds/branches/aosp-build-tools/grid?head=${BUILD_NUMBER}&tail=${BUILD_NUMBER}

Test: treehugger"
