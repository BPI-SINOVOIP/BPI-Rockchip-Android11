#!/bin/bash

if [ $# -ne 3 ]; then
  echo "Usage: mk_verified_boot_params.sh <vbmeta.img> <system-qemu.img> <VerifiedBootParams.textproto>"
#when building vendor.img only, this is expected
  exit 0
fi

# Example Output from 'avbtool calculate_kernel_cmdline --image vbmeta.img':
# (actual output is on a single line)
#
# dm="1 vroot none ro 1,0 4666872 verity 1 \
#     PARTUUID=$(ANDROID_SYSTEM_PARTUUID) \
#     PARTUUID=$(ANDROID_SYSTEM_PARTUUID) \
#     4096 4096 583359 583359 sha1 \
#     d3462e6b89750e3f6bca242551bc5ded22843c8f \
#     930e57fa675c2e9f4b8bbc960ee165cbabbe651c \
#     10 $(ANDROID_VERITY_MODE) ignore_zero_blocks \
#     use_fec_from_device PARTUUID=$(ANDROID_SYSTEM_PARTUUID) \
#     fec_roots 2 fec_blocks 587954 fec_start 587954" \
# root=/dev/dm-0
#
# The emulator can not use every parameter (for example fec args require a
# minimum kernel level).  Some parameters must also be substituted.  Therefore
# this script selects arguments from the tool's output to build the actual
# kernel commandline as a textproto file.

set -e

function die {
  echo $1 >&2
  echo "tools/mk_verified_boot_kernel_options.sh might need a fix"
  exit 1
}

# Incrementing major version causes emulator binaries that do not support the
# version to ignore this file.  This can be useful if there is a change
# not supported by older emulator binaries.
readonly MAJOR_VERSION=1

readonly SRCIMG=$1
readonly QEMU_IMG=$2
readonly TARGET=$3

# Use sgdisk to determine the partition UUID
[[ $(${SGDISK:-sgdisk} --info 1 $QEMU_IMG | grep "Partition name:" | awk '{print $3}') == "'system'" ]] || die "Partition 1 is not named 'system'."
readonly GUID=$(${SGDISK:-sgdisk} --info 1 $QEMU_IMG | grep "Partition unique GUID:" | awk '{print $4}')
[[ $GUID =~ [[:xdigit:]]{8}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{12} ]] || die "GUID looks incorrect: $GUID"

# Extract the commandline
readonly CMDLINE=$(${AVBTOOL:-avbtool} calculate_kernel_cmdline --image $SRCIMG)

# Extracts params from CMDLINE to create a commandline usable by the emulator.
#
# TODO: fec options do not work yet because they require a kernel of >=4.5.
# The emulator is running a 4.4 kernel.  This script ignores options
# for now...

dm_match_regex="dm=\"([^\"]*)\""
[[ "$CMDLINE" =~ $dm_match_regex ]]

[[ ${#BASH_REMATCH[*]} -eq 2 ]] || die "Missing dm section: $CMDLINE"

readonly DM_SECTION=${BASH_REMATCH[1]}
readonly DM_SPLIT=($(echo $DM_SECTION | tr ' ' '\n'))

# Capture everything into a named variable
readonly START_BLOCK=0
readonly SECTOR_COUNT=${DM_SPLIT[5]}
readonly VERITY_VERSION=${DM_SPLIT[7]}
readonly DATA_DEVICE="PARTUUID=$GUID"
readonly HASH_DEVICE="PARTUUID=$GUID"
readonly DATA_BLOCK_SIZE=${DM_SPLIT[10]}
readonly HASH_BLOCK_SIZE=${DM_SPLIT[11]}
readonly NUM_BLOCKS=${DM_SPLIT[12]}
readonly HASH_BLOCK_OFFSET=${DM_SPLIT[13]}
readonly HASH_ALGORITHM=${DM_SPLIT[14]}
readonly ROOT_DIGEST=${DM_SPLIT[15]}
readonly SALT=${DM_SPLIT[16]}
readonly NUM_OPTIONAL_PARAMS=1

# Sanity Checks
[[ $ROOT_DIGEST =~ [[:xdigit:]]{40} ]] || die "ROOT_DIGEST looks incorrect: $ROOT_DIGEST"
[[ $SALT =~ [[:xdigit:]]{40} ]] || die "SALT looks incorrect: $SALT"

HEADER_COMMENT="# dm=\"1 vroot none ro 1,$START_BLOCK $SECTOR_COUNT verity $VERITY_VERSION $DATA_DEVICE $HASH_DEVICE $DATA_BLOCK_SIZE $HASH_BLOCK_SIZE $NUM_BLOCKS $HASH_BLOCK_OFFSET $HASH_ALGORITHM $ROOT_DIGEST $SALT $NUM_OPTIONAL_PARAMS ignore_zero_blocks\" androidboot.veritymode=enforcing root=/dev/dm-0"

echo $HEADER_COMMENT > $TARGET
echo "major_version: $MAJOR_VERSION" >> $TARGET
echo "dm_param: \"1\"" >> $TARGET
echo "dm_param: \"vroot\"  # name" >> $TARGET
echo "dm_param: \"none\"  # UUID" >> $TARGET
echo "dm_param: \"ro\"  # Read-only" >> $TARGET
echo "dm_param: \"1,$START_BLOCK\"  # Start block" >> $TARGET
echo "dm_param: \"$SECTOR_COUNT\"  # Sector count" >> $TARGET
echo "dm_param: \"verity\"  # Type" >> $TARGET
echo "dm_param: \"$VERITY_VERSION\"  # Version" >> $TARGET
echo "dm_param: \"$DATA_DEVICE\"  # Data device" >> $TARGET
echo "dm_param: \"$HASH_DEVICE\"  # Hash device" >> $TARGET
echo "dm_param: \"$DATA_BLOCK_SIZE\"  # Data block size" >> $TARGET
echo "dm_param: \"$HASH_BLOCK_SIZE\"  # Hash block size" >> $TARGET
echo "dm_param: \"$NUM_BLOCKS\"  # Number of blocks" >> $TARGET
echo "dm_param: \"$HASH_BLOCK_OFFSET\"  # Hash block offset" >> $TARGET
echo "dm_param: \"$HASH_ALGORITHM\"  # Hash algorithm" >> $TARGET
echo "dm_param: \"$ROOT_DIGEST\"  # Root digest" >> $TARGET
echo "dm_param: \"$SALT\"  # Salt" >> $TARGET
echo "dm_param: \"$NUM_OPTIONAL_PARAMS\"  # Num optional params" >> $TARGET
echo "dm_param: \"ignore_zero_blocks\"" >> $TARGET

echo "param: \"androidboot.veritymode=enforcing\"" >> $TARGET
echo "param: \"androidboot.verifiedbootstate=orange\"" >> $TARGET
echo "param: \"root=/dev/dm-0\"" >> $TARGET



