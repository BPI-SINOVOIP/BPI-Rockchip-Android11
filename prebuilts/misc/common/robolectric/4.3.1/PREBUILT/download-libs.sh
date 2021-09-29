#! /bin/bash

set -e

printusage() {
    echo "Usage: ./download-libs.sh <robolectric-version> " >&2
    echo "    -f <old-directory-to-copy-from>" >&2
    exit 1
}

oldVersion=""
roboVersion="$1"
shift

while getopts "f:h" opt; do
    case "$opt" in
        f)
            oldVersion="$OPTARG"
            ;;
        h)
            printusage
            ;;
    esac
done

if [[ -z $roboVersion ]] || [[ -z $oldVersion ]]; then
    printusage
fi

mkdir -p ../"$roboVersion"/PREBUILT
# Copy the scripts into the versioned directory for record
cp download-libs.sh ../"$roboVersion"/PREBUILT/download-libs.sh
cp download-libs.gradle ../"$roboVersion"/PREBUILT/download-libs.gradle

cd ../"$roboVersion"
gradle -b PREBUILT/download-libs.gradle \
    -ProbolectricVersion="$roboVersion" \
    -PshadowsVersion="$roboVersion" \
    -PbuildDir="`pwd`"

COPY_FROM_OLD_VERSION=(
    "java-timeout"
    "list_failed.sh"
    "report-internal.mk"
    "robotest-internal.mk"
    "robotest.sh"
    "run_robotests.mk"
    "wrapper.sh"
    "wrapper_test.sh"
)

for file in "${COPY_FROM_OLD_VERSION[@]}"; do
    cp -n ../"$oldVersion"/$file ./$file
done

cat <<EOF > Android.bp
java_import {
    name: "platform-robolectric-${roboVersion}-prebuilt",
    sdk_version: "current",
    jars: [
        "lib/annotations-${roboVersion}.jar",
        "lib/asm-6.0.jar",
        "lib/junit-${roboVersion}.jar",
        "lib/resources-${roboVersion}.jar",
        "lib/sandbox-${roboVersion}.jar",
        "lib/shadowapi-${roboVersion}.jar",
        "lib/shadows-framework-${roboVersion}.jar",
        "lib/shadows-httpclient-${roboVersion}.jar",
        "lib/shadows-multidex-${roboVersion}.jar",
        "lib/shadows-supportv4-${roboVersion}.jar",
        "lib/robolectric-${roboVersion}.jar",
        "lib/utils-${roboVersion}.jar",
    ],
}

EOF

set +e
