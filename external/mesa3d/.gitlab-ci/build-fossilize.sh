#!/bin/bash

set -ex

git clone https://github.com/ValveSoftware/Fossilize.git
cd Fossilize
git checkout 6b5b570008c9ab5269e341f04c811fe49a1bb72c
git submodule update --init
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja -C . install
cd ../..
rm -rf Fossilize
