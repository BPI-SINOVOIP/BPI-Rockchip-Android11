#!/bin/bash

readonly OUT_DIR="$1"
readonly DIST_DIR="$2"
readonly BUILD_NUMBER="$3"

readonly SCRIPT_DIR="$(dirname "$0")"

readonly FAILURE_DIR=layoutlib-test-failures
readonly FAILURE_ZIP=layoutlib-test-failures.zip

STUDIO_JDK=${SCRIPT_DIR}"/../../../../prebuilts/jdk/jdk11/linux-x86"
MISC_COMMON=${SCRIPT_DIR}"/../../../../prebuilts/misc/common"
OUT_INTERMEDIATES=${SCRIPT_DIR}"/../../../../out/soong/.intermediates"

# Run layoutlib tests
${STUDIO_JDK}/bin/java -ea \
    -Dtest_res.dir=${SCRIPT_DIR}/res \
    -Dtest_failure.dir=${OUT_DIR}/${FAILURE_DIR} \
    -cp ${MISC_COMMON}/tools-common/tools-common-prebuilt.jar:${MISC_COMMON}/ninepatch/ninepatch-prebuilt.jar:${MISC_COMMON}/sdk-common/sdk-common.jar:${MISC_COMMON}/kxml2/kxml2-2.3.0.jar:${MISC_COMMON}/layoutlib_api/layoutlib_api-prebuilt.jar:${OUT_INTERMEDIATES}/prebuilts/tools/common/m2/trove-prebuilt/linux_glibc_common/combined/trove-prebuilt.jar:${OUT_INTERMEDIATES}/external/junit/junit/linux_glibc_common/javac/junit.jar:${OUT_INTERMEDIATES}/external/guava/guava-jre/linux_glibc_common/javac/guava-jre.jar:${OUT_INTERMEDIATES}/external/hamcrest/hamcrest-core/hamcrest/linux_glibc_common/javac/hamcrest.jar:${OUT_INTERMEDIATES}/external/mockito/mockito/linux_glibc_common/combined/mockito.jar:${OUT_INTERMEDIATES}/external/objenesis/objenesis/linux_glibc_common/javac/objenesis.jar:${OUT_INTERMEDIATES}/frameworks/layoutlib/bridge/layoutlib/linux_glibc_common/withres/layoutlib.jar:${OUT_INTERMEDIATES}/frameworks/layoutlib/temp_layoutlib/linux_glibc_common/gen/temp_layoutlib.jar:${OUT_INTERMEDIATES}/frameworks/layoutlib/bridge/tests/layoutlib-tests/linux_glibc_common/withres/layoutlib-tests.jar \
    org.junit.runner.JUnitCore \
    com.android.layoutlib.bridge.intensive.Main

test_exit_code=$?

# Create zip of all failure screenshots
if [[ -d "${OUT_DIR}/${FAILURE_DIR}" ]]; then
    zip -q -j -r ${OUT_DIR}/${FAILURE_ZIP} ${OUT_DIR}/${FAILURE_DIR}
fi

# Move failure zip to dist directory if specified
if [[ -d "${DIST_DIR}" ]] && [[ -e "${OUT_DIR}/${FAILURE_ZIP}" ]]; then
    mv ${OUT_DIR}/${FAILURE_ZIP} ${DIST_DIR}
fi

exit ${test_exit_code}
