#!/bin/bash

# Run from Android root, e.g.:
#
#   device/google/atv/audio_proxy/interfaces/update-makefiles.sh

source $ANDROID_BUILD_TOP/system/tools/hidl/update-makefiles-helper.sh

do_makefiles_update \
  "device.google.atv.audio_proxy:device/google/atv/audio_proxy/interfaces"
