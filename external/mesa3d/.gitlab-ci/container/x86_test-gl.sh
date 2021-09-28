#!/bin/bash

set -e
set -o xtrace

export DEBIAN_FRONTEND=noninteractive

# Ephemeral packages (installed for this script and removed again at the end)
STABLE_EPHEMERAL=" \
      autoconf \
      automake \
      ccache \
      clang-10 \
      cmake \
      g++ \
      libclang-cpp10-dev \
      libgbm-dev \
      libgles2-mesa-dev \
      libpcre3-dev \
      libpciaccess-dev \
      libpng-dev \
      libvulkan-dev \
      libwaffle-dev \
      libxcb-keysyms1-dev \
      libxkbcommon-dev \
      libxrender-dev \
      llvm-10-dev \
      make \
      meson \
      ocl-icd-opencl-dev \
      patch \
      pkg-config \
      python3-distutils \
      python3.7-dev \
      wget \
      xz-utils \
      "

apt-get install -y --no-remove \
      clinfo \
      libclang-common-10-dev \
      libclang-cpp10 \
      libxcb-shm0 \
      ocl-icd-libopencl1 \
      python3-lxml \
      python3-simplejson \
      $STABLE_EPHEMERAL


. .gitlab-ci/container/container_pre_build.sh


############### Build spirv-tools (debian too old)

. .gitlab-ci/build-spirv-tools.sh

############### Build libclc

. .gitlab-ci/build-libclc.sh

############### Build virglrenderer

. .gitlab-ci/build-virglrenderer.sh

############### Build piglit

INCLUDE_OPENCL_TESTS=1 . .gitlab-ci/build-piglit.sh

############### Build dEQP runner (and install rust temporarily for it)
. .gitlab-ci/build-rust.sh
. .gitlab-ci/build-deqp-runner.sh
rm -rf /root/.rustup /root/.cargo

############### Build dEQP GL

DEQP_TARGET=surfaceless . .gitlab-ci/build-deqp.sh

############### Build apitrace

. .gitlab-ci/build-apitrace.sh

############### Build renderdoc

. .gitlab-ci/build-renderdoc.sh

############### Build libdrm

. .gitlab-ci/build-libdrm.sh

############### Uninstall the build software

ccache --show-stats

apt-get purge -y \
      $STABLE_EPHEMERAL

apt-get autoremove -y --purge
