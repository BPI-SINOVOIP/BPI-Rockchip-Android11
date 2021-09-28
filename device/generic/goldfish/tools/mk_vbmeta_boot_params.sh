#!/bin/bash

if [ $# -ne 3 ]; then
  echo "Usage: $0 <vbmeta.img> <system.img> <VbmetaBootParams.textproto>"
  exit 0
fi

# Example Output from 'avbtool calculate_vbmeta_digest --image $OUT/vbmeta.img':
# 3254db8a232946c712b5c6f8c1a80b31f2a200bab98553d86f5915d06bfd5436
#
# Example Output from 'avbtool info_image --image $OUT/vbmeta.img':
#
# Minimum libavb version:   1.0
# Header Block:             256 bytes
# Authentication Block:     576 bytes
# Auxiliary Block:          1600 bytes
# Algorithm:                SHA256_RSA4096
# Rollback Index:           0
# Flags:                    0
# Release String:           'avbtool 1.1.0'
# Descriptors:
# ...
#
#

set -e

function die {
  echo $1 >&2
  exit 1
}

# Incrementing major version causes emulator binaries that do not support the
# version to ignore this file.  This can be useful if there is a change
# not supported by older emulator binaries.
readonly MAJOR_VERSION=2

readonly VBMETAIMG=$1
readonly SYSIMG=$2
readonly TARGET=$3

# Extract the digest
readonly VBMETA_DIGEST=$(${AVBTOOL:-avbtool} calculate_vbmeta_digest --image $VBMETAIMG)


readonly INFO_OUTPUT=$(${AVBTOOL:-avbtool} info_image --image $VBMETAIMG | grep "^Algorithm:")

# Extract the algorithm
readonly ALG_OUTPUT=$(echo $INFO_OUTPUT | grep "Algorithm:")
readonly ALG_SPLIT=($(echo $ALG_OUTPUT | tr ' ' '\n'))
readonly ORG_ALGORITHM=${ALG_SPLIT[1]}

if [[ $ORG_ALGORITHM == "SHA256_RSA4096" ]]; then
VBMETA_HASH_ALG=sha256
else
die "Don't know anything about $ORG_ALGORITHM"
fi


# extract the size

function get_bytes {
    MY_OUTPUT=$(${AVBTOOL:-avbtool} info_image --image $1 | grep "$2" )
    MY_SPLIT=($(echo $MY_OUTPUT | tr ' ' '\n'))
    MY_BYTES=${MY_SPLIT[2]}
    echo $MY_BYTES
}

HEADER_SIZE=$(get_bytes $VBMETAIMG "Header Block:")
AUTHEN_SIZE=$(get_bytes $VBMETAIMG "Authentication Block:")
AUX_SIZE=$(get_bytes $VBMETAIMG "Auxiliary Block:")
SYSMETA_SIZE=$(get_bytes $SYSIMG "VBMeta size:")

VBMETA_SIZE=$(expr $HEADER_SIZE + $AUTHEN_SIZE + $AUX_SIZE + $SYSMETA_SIZE)


HEADER_COMMENT="# androidboot.vbmeta.size=$VBMETA_SIZE androidboot.vbmeta.hash_alg=$VBMETA_HASH_ALG androidboot.vbmeta.digest=$VBMETA_DIGEST"

echo $HEADER_COMMENT > $TARGET
echo "major_version: $MAJOR_VERSION" >> $TARGET
#echo "param: \"androidboot.slot_suffix=_a\"" >> $TARGET
echo "param: \"androidboot.vbmeta.size=$VBMETA_SIZE\"" >> $TARGET
echo "param: \"androidboot.vbmeta.hash_alg=$VBMETA_HASH_ALG\"" >> $TARGET
echo "param: \"androidboot.vbmeta.digest=$VBMETA_DIGEST\"" >> $TARGET
