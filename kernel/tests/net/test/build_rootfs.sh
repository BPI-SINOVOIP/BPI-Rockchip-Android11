#!/bin/bash
#
# Copyright (C) 2018 The Android Open Source Project
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
#

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

# Make sure we're in C locale so build inside chroot does not complain
# about missing files
unset LANG LANGUAGE \
  LC_ADDRESS LC_ALL LC_COLLATE LC_CTYPE LC_IDENTIFICATION LC_MEASUREMENT \
  LC_MESSAGES LC_MONETARY LC_NAME LC_NUMERIC LC_PAPER LC_TELEPHONE LC_TIME
export LC_ALL=C

usage() {
  echo -n "usage: $0 [-h] [-s bullseye] [-a i386|amd64|armhf|arm64] "
  echo "[-m http://mirror/debian] [-n net_test.rootfs.`date +%Y%m%d`]"
  exit 1
}

mirror=http://ftp.debian.org/debian
debootstrap=debootstrap
suite=bullseye
arch=amd64

while getopts ":hs:a:m:n:" opt; do
  case $opt in
    h)
      usage
      ;;
    s)
      if [[ "$OPTARG" != "bullseye" ]]; then
        echo "Invalid suite: $OPTARG" >&2
        usage
      fi
      suite="${OPTARG}"
      ;;
    a)
      case "${OPTARG}" in
        i386|amd64|armhf|arm64)
          arch="${OPTARG}"
          ;;
        *)
          echo "Invalid arch: ${OPTARG}" >&2
          usage
          ;;
      esac
      ;;
    m)
      mirror=$OPTARG
      ;;
    n)
      name=$OPTARG
      ;;
    \?)
      echo "Invalid option: $OPTARG" >&2
      usage
      ;;
    :)
      echo "Invalid option: $OPTARG requires an argument" >&2
      usage
      ;;
  esac
done

if [[ -z "${name}" ]]; then
  name=net_test.rootfs.${arch}.${suite}.`date +%Y%m%d`
fi

# Switch to qemu-debootstrap for incompatible architectures
if [ "$arch" = "arm64" ]; then
  debootstrap=qemu-debootstrap
fi

# Sometimes it isn't obvious when the script fails
failure() {
  echo "Filesystem generation process failed." >&2
}
trap failure ERR

# Import the package list for this release
packages=`cat $SCRIPT_DIR/rootfs/$suite.list | xargs | tr -s ' ' ','`

# For the debootstrap intermediates
tmpdir=`mktemp -d`
tmpdir_remove() {
  echo "Removing temporary files.." >&2
  sudo rm -rf "${tmpdir}"
}
trap tmpdir_remove EXIT

workdir="${tmpdir}/_"

mkdir "${workdir}"
chmod 0755 "${workdir}"
sudo chown root:root "${workdir}"

# Run the debootstrap first
cd $workdir
sudo $debootstrap --arch=$arch --variant=minbase --include=$packages \
                  $suite . $mirror
# Workarounds for bugs in the debootstrap suite scripts
for mount in `cat /proc/mounts | cut -d' ' -f2 | grep -e ^$workdir`; do
  echo "Unmounting mountpoint $mount.." >&2
  sudo umount $mount
done
# Copy the chroot preparation scripts, and enter the chroot
for file in $suite.sh common.sh net_test.sh; do
  sudo cp -a $SCRIPT_DIR/rootfs/$file root/$file
  sudo chown root:root root/$file
done
sudo chroot . /root/$suite.sh

# Leave the workdir, to build the filesystem
cd -

# For the final image mount
mount=`mktemp -d`
mount_remove() {
 rmdir $mount
 tmpdir_remove
}
trap mount_remove EXIT

# Create a 1G empty ext3 filesystem
truncate -s 1G $name
mke2fs -F -t ext3 -L ROOT $name

# Mount the new filesystem locally
sudo mount -o loop -t ext3 $name $mount
image_unmount() {
  sudo umount $mount
  mount_remove
}
trap image_unmount EXIT

# Copy the patched debootstrap results into the new filesystem
sudo cp -a $workdir/* $mount

# Fill the rest of the space with zeroes, to optimize compression
sudo dd if=/dev/zero of=$mount/sparse bs=1M 2>/dev/null || true
sudo rm -f $mount/sparse

echo "Debian $suite for $arch filesystem generated at '$name'."
