#!/bin/bash
# This script is used to run configure and generate all of the necessary
# files when updating to a new version of jemalloc.
# The NDK argument must be a NDK at r20 or newer so that it does not
# require installing the standalone tools.

ndk=${1}
if [[ "$ndk" == "" ]]; then
  echo "Requires two arguments."
  echo "usage: conf.sh NDK ARCH"
  exit 1
fi

arch=${2}
if [[ "$arch" == "" ]]; then
  echo "Requires two arguments."
  echo "usage: conf.sh NDK ARCH"
  exit 1
fi

if [[ ! -d ${ndk} ]]; then
  echo "NDK directory ${ndk} does not exist."
  exit 1
fi
toolchain=${ndk}/toolchains/llvm/prebuilt/linux-x86_64
if [[ ! -d ${toolchain} ]]; then
  echo "NDK ${ndk} cannot find toolchain directory."
  echo "  ${toolchain}"
  exit 1
fi

# The latest version of clang to use for compilation.
latest_api=29

case "$arch" in
  "arm")
    prefix="arm-linux-androideabi"
    clang_prefix="armv7a-linux-androideabi"
    target="arm-android-linux"
    ;;
  "arm64")
    prefix="aarch64-linux-android"
    target="aarch64-android-linux"
    ;;
  "x86")
    target="x86-android-linux"
    export CPPFLAGS="-m32"
    ;&
  "x86_64")
    prefix="x86_64-linux-android"
    if [[ "$target" == "" ]]; then
      target="x86_64-android-linux"
    fi
    ;;
  *)
    echo "Unknown arch $arch"
    exit 1
    ;;
esac

if [[ "${clang_prefix}" == "" ]]; then
  clang_prefix="${prefix}"
fi

tools=("AR" "ar"
       "AS" "as"
       "LD" "ld"
       "RANLIB" "ranlib"
       "STRIP" "strip")

for ((i = 0; i < ${#tools[@]}; i = i + 2)); do
  binary=${toolchain}/bin/${prefix}-${tools[$((i + 1))]}
  if [[ ! -e ${binary} ]]; then
    echo "${binary} does not exist."
    exit 1
  fi
  export ${tools[$i]}=${binary}
done

clang=("CC" "clang"
       "CXX" "clang++")

for ((i = 0; i < ${#clang[@]}; i = i + 2)); do
  binary=${toolchain}/bin/${clang_prefix}${latest_api}-${clang[$((i + 1))]}
  if [[ ! -e ${binary} ]]; then
    echo "${binary} does not exist."
    exit 1
  fi
  export ${clang[$i]}=${binary}
done

export CPP="${CC} -E"

./autogen.sh --with-jemalloc_prefix=je_ --host=${target}
