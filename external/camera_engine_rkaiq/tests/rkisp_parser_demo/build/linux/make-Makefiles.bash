#!/bin/bash
# Run this from within a bash shell
# x86_64 is for simulation do not enable RK platform
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=debug \
    -DRKPLATFORM=OFF \
    ../../  && make
