#!/bin/bash
rm -f version.h

#dangku, fix no repo compile error
#COMMIT_INFO=$(cd $(dirname $0) && git log -1 --oneline --date=short --pretty=format:"%h date: %cd author: %<|(20)%an")
COMMIT_INFO="e68d783 date: 2021-08-05 author: Meiyou Chen"
BUILD_TIME=$(date "+%G-%m-%d %H:%M:%S")
MPI_MMZ_VERSION="build:$BUILD_TIME   git-$COMMIT_INFO"

#Only when compiling with CMAKE in linux, will the current directory generate version.h.
if [ $TARGET_PRODUCT ];then
	cat $(dirname $0)/version.h.template | sed "s/\$FULL_VERSION/$MPI_MMZ_VERSION/g"
else
	cat $(dirname $0)/version.h.template | sed "s/\$FULL_VERSION/$MPI_MMZ_VERSION/g" > $(dirname $0)/version.h
	echo "Generated version.h"
fi
