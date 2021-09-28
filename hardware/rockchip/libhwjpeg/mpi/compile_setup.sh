#!/bin/bash
local_path=$1
cd $local_path

git_commit=`git log -1 --pretty=format:"%h author: %an %s"`
echo "#define GIT_INFO \"$git_commit\"" > ./version.h
