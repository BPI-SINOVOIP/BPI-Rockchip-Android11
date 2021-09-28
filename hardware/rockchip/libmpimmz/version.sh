#!/bin/bash
rm -f version.h

COMMIT_INFO=$(cd $(dirname $0) && git log -1 --oneline --date=short --pretty=format:"%h date: %cd author: %<|(20)%an")
BUILD_TIME=$(date "+%G-%m-%d %H:%M:%S")
MPI_MMZ_VERSION="build:$BUILD_TIME   git-$COMMIT_INFO"

#Only when compiling with CMAKE in linux, will the current directory generate version.h.
if [ $TARGET_PRODUCT ];then
	cat $(dirname $0)/version.h.template | sed "s/\$FULL_VERSION/$MPI_MMZ_VERSION/g"
else
	cat $(dirname $0)/version.h.template | sed "s/\$FULL_VERSION/$MPI_MMZ_VERSION/g" > $(dirname $0)/version.h
	echo "Generated version.h"
fi
