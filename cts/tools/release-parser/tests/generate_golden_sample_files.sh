#!/bin/bash

# This script generats expected parse results as protobuf files in text format.
# They will be used in unit tests to validate Parsers.
# A typical flow is as follows:
# 1. add the file name in targetFile below after adding a new object file in ./resources
# 2. run: ./generate_golden_sample_files.sh
# 3. manully validate the content of output protobuf files
# 4. update UnitTests & respective test code to use the new files
# 5. build: make release-parser release-parser-tests -j10
# 6. test: ./cts/tools/release-parser/tests/run_test.sh

echo Generating golden sample files for parser validation
TargetFiles="HelloActivity.apk CtsJniTestCases.apk Shell.apk"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.ApkParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="libEGL.so"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.SoParser -i resources/$file -pi -of resources/$file.pb.txt
done

TargetFiles="CtsAslrMallocTestCases32"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.SoParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="android.test.runner.vdex"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.VdexParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="android.test.runner.odex"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.OdexParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="boot-framework.oat"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.OatParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="boot-framework.art"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.ArtParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="platform.xml android.hardware.vulkan.version.xml"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.XmlParser -i resources/$file -of resources/$file.pb.txt
done

TargetFiles="build.prop"
for file in $TargetFiles; do
    echo Processing $file
    java -cp $ANDROID_HOST_OUT/framework/release-parser.jar com.android.cts.releaseparser.BuildPropParser -i resources/$file -of resources/$file.pb.txt
done
