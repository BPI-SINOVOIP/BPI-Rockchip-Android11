#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

from acts.test_utils.instrumentation.device.command.adb_command_types \
    import DeviceBinaryCommandSeries
from acts.test_utils.instrumentation.device.command.adb_command_types \
    import DeviceGServices
from acts.test_utils.instrumentation.device.command.adb_command_types \
    import DeviceState

"""Google-internal device settings for power testing."""

# TODO: add descriptions to each setting

# Location

location_collection = DeviceGServices(
    'location:collection_enabled', on_val='1', off_val='0')

location_opt_in = DeviceBinaryCommandSeries(
    [
        DeviceState('content insert --uri content://com.google.settings/'
                    'partner --bind name:s:use_location_for_services '
                    '--bind value:s:%s'),
        DeviceState('content insert --uri content://com.google.settings/'
                    'partner --bind name:s:network_location_opt_in '
                    '--bind value:s:%s')
    ]
)

# Cast

cast_broadcast = DeviceGServices('gms:cast:mdns_device_scanner:is_enabled')


# Apps

disable_playstore = 'pm disable-user com.android.vending'


# Volta

disable_volta = 'pm disable-user com.google.android.volta'


# CHRE

disable_chre = 'setprop ctl.stop vendor.chre'


# MusicIQ

disable_musiciq = 'pm disable-user com.google.intelligence.sense'


# Hotword

disable_hotword = (
    'am start -a com.android.intent.action.MANAGE_VOICE_KEYPHRASES '
    '--ei com.android.intent.extra.VOICE_KEYPHRASE_ACTION 2 '
    '--es com.android.intent.extra.VOICE_KEYPHRASE_HINT_TEXT "demo" '
    '--es com.android.intent.extra.VOICE_KEYPHRASE_LOCALE "en-US" '
    'com.android.hotwordenrollment.okgoogle/'
    'com.android.hotwordenrollment.okgoogle.EnrollmentActivity')
