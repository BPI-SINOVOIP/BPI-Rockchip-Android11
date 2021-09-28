/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <net/ethernet.h>
#include <stdint.h>

struct ieee80211_hdr {
        uint16_t frame_control;
        uint16_t duration_id;
        uint8_t addr1[ETH_ALEN];
        uint8_t addr2[ETH_ALEN];
        uint8_t addr3[ETH_ALEN];
        uint16_t seq_ctrl;
        uint8_t addr4[ETH_ALEN];
} __attribute__((__packed__));

