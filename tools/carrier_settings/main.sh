#!/bin/bash
#
# Copyright (C) 2020 Google LLC
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

###############################################################################
# Input files and output directory: can be customized.
INPUT_CARRIERCONFIG_XML_FILE="packages/apps/CarrierConfig/res/xml/vendor.xml"
INPUT_CARRIERCONFIG_ASSETS_DIR="packages/apps/CarrierConfig/assets"
INPUT_APNS_XML_FILE="device/sample/etc/apns-full-conf.xml"
OUTPUT_DIR="/tmp/CarrierSettings/etc"
###############################################################################

( # start sub-shell so can use 'set -e' to abort on any failure
set -e

# 1. Build tools
echo 'step 1. Building tools ...'
m CarrierConfigConverterV2 update_apn update_carrier_data GenCarrierList GenDeviceSettings
echo 'Done.'

echo 'step 2. Converting config files ...'
# 2a. Create a temp directory as workspace
TMP_DIR=$(mktemp -d -t cs-XXXXXXX)
DATA_SETTING_DIR=$TMP_DIR/data/setting
DATA_DEVICE_DIR=$TMP_DIR/data/device
ASSETS_DIR=$TMP_DIR/assets
INNER_TMP_DIR=$TMP_DIR/tmp
TIER1_CARRIERS_FILE=$TMP_DIR/data/tier1_carriers.textpb
DEVICE_FILE=$DATA_DEVICE_DIR/device.textpb
mkdir -p "$DATA_SETTING_DIR" > /dev/null
mkdir -p "$DATA_DEVICE_DIR" > /dev/null
mkdir -p "$ASSETS_DIR" > /dev/null
mkdir -p "$INNER_TMP_DIR" > /dev/null
touch "$TIER1_CARRIERS_FILE" > /dev/null
touch "$DEVICE_FILE" > /dev/null

# 2b. Copy input files to workspace
cp $INPUT_CARRIERCONFIG_XML_FILE "$TMP_DIR"/vendor.xml > /dev/null
cp $INPUT_CARRIERCONFIG_ASSETS_DIR/* "$ASSETS_DIR"/ > /dev/null
cp $INPUT_APNS_XML_FILE "$TMP_DIR"/apns-full-conf.xml > /dev/null

# 2c. Convert XMLs to TEXTPB

# DO NOT change the EPOCH date. It's used by CarrierSettings server.
EPOCH=$(date -d '2018-06-01T00:00:00Z' +%s)
NOW=$(date +%s)
TIMESTAMP="$((NOW-EPOCH))"

SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
UPDATE_APN=$SCRIPT_DIR/bin/update_apn
[ -x "$UPDATE_APN" ] || UPDATE_APN=out/host/linux-x86/bin/update_apn
UPDATE_CARRIER_DATA=$SCRIPT_DIR/bin/update_carrier_data
[ -x "$UPDATE_CARRIER_DATA" ] || UPDATE_CARRIER_DATA=out/host/linux-x86/bin/update_carrier_data

# To use multiple vendor.xml files, just provide multiple `--vendor_xml=___.xml`
# lines in the command below. The order decides config precedence: a file is
# overwritten by files AFTER it.
out/host/linux-x86/bin/CarrierConfigConverterV2 \
  --output_dir="$TMP_DIR"/data \
  --vendor_xml="$TMP_DIR"/vendor.xml \
  --assets="$ASSETS_DIR"/ \
  --version=$TIMESTAMP > /dev/null
"$UPDATE_APN" \
  --apn_file="$TMP_DIR"/apns-full-conf.xml \
  --data_dir="$TMP_DIR"/data \
  --out_file="$INNER_TMP_DIR"/apns.textpb
"$UPDATE_CARRIER_DATA" \
  --data_dir="$TMP_DIR"/data \
  --in_file="$INNER_TMP_DIR"/apns.textpb

# 2d. Convert TEXTPB to PB
mkdir -p "$INNER_TMP_DIR"/pb > /dev/null
mkdir -p "$INNER_TMP_DIR"/textpb > /dev/null

out/host/linux-x86/bin/GenCarrierList \
  --version_offset=0 \
  --with_version_number \
  --out_pb="$INNER_TMP_DIR"/carrier_list.pb \
  --in_textpbs="$TIER1_CARRIERS_FILE","$TMP_DIR"/data/other_carriers.textpb \
  > /dev/null
mv "$INNER_TMP_DIR"/carrier_list*.pb "$INNER_TMP_DIR"/pb > /dev/null
mv "$INNER_TMP_DIR"/carrier_list.textpb "$INNER_TMP_DIR"/textpb > /dev/null

for device in "$DATA_DEVICE_DIR"/*.textpb; do
  [[ -e "$device" ]] || break
  device=${device%.*} && device=${device##*/} \
  && device_dir="${INNER_TMP_DIR}/${device}" && mkdir -p "${INNER_TMP_DIR}" \
  && mkdir -p "${INNER_TMP_DIR}/textpb/${device}" > /dev/null \
  && out/host/linux-x86/bin/GenDeviceSettings \
  --device_overlay="$DATA_DEVICE_DIR/${device}.textpb" \
  --base_setting_dir="$DATA_SETTING_DIR" \
  --device_setting_dir="${device_dir}" \
  --version_offset=0 \
  --with_device_name="${device}" \
  --with_version_number > /dev/null \
  && mv "${device_dir}"/*.pb "${INNER_TMP_DIR}/pb" > /dev/null \
  && mv "${device_dir}"/*.textpb "${INNER_TMP_DIR}/textpb/${device}" > /dev/null \
  && rmdir "${device_dir}"
done

echo 'Done.'

echo 'step 3. Copy generated files to output directory ...'

mkdir -p $OUTPUT_DIR > /dev/null
rm -rf "${OUTPUT_DIR:?}"/* > /dev/null
cp -r "$INNER_TMP_DIR"/pb  $OUTPUT_DIR > /dev/null
cp -r "$INNER_TMP_DIR"/textpb  $OUTPUT_DIR > /dev/null
rm -rf "${TMP_DIR:?}" > /dev/null

echo 'Generated files:'
find $OUTPUT_DIR -type f

echo 'Done.'
) # end sub-shell
