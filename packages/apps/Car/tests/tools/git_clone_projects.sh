#!/bin/bash

# Copyright (C) 2020 The Android Open Source Project
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

echo "An example to clone minimal git projects for the tests app development by Android Studio."

if [[ -z $GIT_REPO_URL ]]; then
    echo 'Error: you need to specify GIT_REPO_URL="target-url"'
    exit
fi
echo "GIT_REPO_URL=$GIT_REPO_URL"

if [[ -z $BRANCH ]]; then
    echo 'Error: you need to specify BRANCH="target-branch"'
    exit
fi
echo "BRANCH=$BRANCH"

if [[ -z $WORK_DIR ]]; then
    export WORK_DIR="$PWD/Car"
fi
echo "WORK_DIR=$WORK_DIR"

mkdir -p $WORK_DIR
cd $WORK_DIR

PROJECTS=0
SECONDS=0
echo "Cloning Car/libs"
git clone -b $BRANCH "$GIT_REPO_URL/platform/packages/apps/Car/libs"
let "PROJECTS++"
cd "$WORK_DIR/libs"
f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
cd $WORK_DIR
echo

echo "Cloning Car/libs"
git clone -b $BRANCH "$GIT_REPO_URL/platform/packages/apps/Car/tests"
let "PROJECTS++"
cd "$WORK_DIR/tests"
f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
cd $WORK_DIR
echo

ls -l "$WORK_DIR"

echo "

Cloning $PROJECTS projects takes: $SECONDS sec.

Do your magic and then get the change pushed for review, e.g.:
git add -u
git commit
git push origin HEAD:refs/for/$BRANCH
"
