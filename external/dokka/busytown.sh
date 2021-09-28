#!/bin/bash
set -e

SCRIPT_DIR="$(cd $(dirname $0) && pwd)"

"$SCRIPT_DIR"/gradlew -p "$SCRIPT_DIR" -I "$SCRIPT_DIR"/busytown.gradle --no-daemon :core:build :runners:android-gradle-plugin:build :runners:gradle-integration-tests:build zipMaven copyRepository