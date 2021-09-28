#--------------------------------------------------------------------------
#  Copyright (C) 2014 Fuzhou Rockchip Electronics Co. Ltd. All rights reserved.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  #  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of The Linux Foundation nor
#      the names of its contributors may be used to endorse or promote
#      products derived from this software without specific prior written
#      permission.

#!/bin/bash
local_path=$1
# 生成 svn 状态信息
cd $local_path
git_info_txt=`git log \-1`; ret_info=$?
if [ $ret_info -eq 0 ]
then {
    git_commit=`git log \-1 | grep commit | head -n 1`;
} fi
sf_author=$LOGNAME
sf_date=`date -R`
# 处理生成的状态信息
echo "#define OMX_COMPILE_INFO      \"author:  $sf_author\n time: $sf_date git $git_commit \"" > ./include/rockchip/git_info.h

# pre-commit 搬运

# install git hooks

src_dir=tools/hooks
dst_dir=.git/hooks

echo $src_dir
echo $dst_dir
if test ! \( -x $dst_dir/pre-commit -a -L $dst_dir/pre-commit \);
then
    rm -f $dst_dir/pre-commit
    cp tools/hooks/pre-commit .git/hooks/pre-commit
fi
