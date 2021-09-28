#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.


# WiFi Frequency and channel map.
wifi_channel_map = {
    2412: 1,
    2417: 2,
    2422: 3,
    2427: 4,
    2432: 5,
    2437: 6,
    2442: 7,
    2447: 8,
    2452: 9,
    2457: 10,
    2462: 11,
    2467: 12,
    2472: 13,
    2484: 14,
    5170: 34,
    5180: 36,
    5190: 38,
    5200: 40,
    5210: 42,
    5220: 44,
    5230: 46,
    5240: 48,
    5260: 52,
    5280: 56,
    5300: 60,
    5320: 64,
    5500: 100,
    5520: 104,
    5540: 108,
    5560: 112,
    5580: 116,
    5600: 120,
    5620: 124,
    5640: 128,
    5660: 132,
    5680: 136,
    5700: 140,
    5720: 144,
    5745: 149,
    5755: 151,
    5765: 153,
    5775: 155,
    5795: 159,
    5785: 157,
    5805: 161,
    5825: 165
}

# Supported lte band.
# TODO:(@sairamganesh) Make a common function to support different SKU's.

supported_lte_bands = ['OB1', 'OB2', 'OB3', 'OB4', 'OB5', 'OB7', 'OB8',
                       'OB12', 'OB13', 'OB14', 'OB17', 'OB18', 'OB19',
                       'OB20', 'OB25', 'OB26', 'OB28', 'OB30', 'OB38',
                       'OB39', 'OB40', 'OB41', 'OB46', 'OB48', 'OB66',
                       'OB71'
                       ]

# list of TDD Bands supported.
tdd_band_list = ['OB33', 'OB34', 'OB35', 'OB36', 'OB37', 'OB38', 'OB39', 'OB40',
                 'OB41', 'OB42', 'OB43', 'OB44']

# lte band channel map.
# For every band three channels are chosen(Low, Mid and High)
band_channel_map = {
    'OB1': [25, 300, 575],
    'OB2': [625, 900, 1175],
    'OB3': [1225, 1575, 1925],
    'OB4': [1975, 2175, 2375],
    'OB5': [2425, 2525, 2625],
    'OB7': [3100],
    'OB8': [3475, 3625, 3775],
    'OB12': [5035, 5095, 5155],
    'OB13': [5205, 5230, 5255],
    'OB14': [5310, 5330, 5355],
    'OB17': [5755, 5790, 5825],
    'OB18': [5875, 5925, 5975],
    'OB19': [6025, 6075, 6125],
    'OB20': [6180, 6300, 6425],
    'OB25': [8065, 8365, 8665],
    'OB26': [8715, 8865, 9010],
    'OB28': [9235, 9435, 9635],
    'OB30': [9795, 9820, 9840],
    'OB38': [37750, 38000, 38245],
    'OB39': [38250, 38450, 38645],
    'OB40': [38650, 39150, 39645],
    'OB41': [39650, 40620, 41585],
    'OB46': [46790, 50665, 54535],
    'OB48': [55240, 55990, 56735],
    'OB66': [66461, 66886, 67331],
    'OB71': [68611, 68761, 68906]
}
