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

// mac80211_hwsim flags, from kernel drivers/net/wireless/mac80211_hwsim.h
#define BIT(num) (1UL << (num))
enum hwsim_tx_control_flags {
    HWSIM_TX_CTL_REQ_TX_STATUS = BIT(0),
    HWSIM_TX_CTL_NO_ACK        = BIT(1),
    HWSIM_TX_STAT_ACK          = BIT(2),
};

// mac80211_hwsim commands, from kernel drivers/net/wireless/mac80211_hwsim.h
enum HwSimCommand {
    HWSIM_CMD_UNSPEC,
    HWSIM_CMD_REGISTER,
    HWSIM_CMD_FRAME,
    HWSIM_CMD_TX_INFO_FRAME,
    HWSIM_CMD_NEW_RADIO,
    HWSIM_CMD_DEL_RADIO,
    HWSIM_CMD_GET_RADIO,
    __HWSIM_CMD_MAX,
};

// mac80211_hwsim attributes, from kernel drivers/net/wireless/mac80211_hwsim.h
enum HwSimAttribute {
        HWSIM_ATTR_UNSPEC,
        HWSIM_ATTR_ADDR_RECEIVER,
        HWSIM_ATTR_ADDR_TRANSMITTER,
        HWSIM_ATTR_FRAME,
        HWSIM_ATTR_FLAGS,
        HWSIM_ATTR_RX_RATE,
        HWSIM_ATTR_SIGNAL,
        HWSIM_ATTR_TX_INFO,
        HWSIM_ATTR_COOKIE,
        HWSIM_ATTR_CHANNELS,
        HWSIM_ATTR_RADIO_ID,
        HWSIM_ATTR_REG_HINT_ALPHA2,
        HWSIM_ATTR_REG_CUSTOM_REG,
        HWSIM_ATTR_REG_STRICT_REG,
        HWSIM_ATTR_SUPPORT_P2P_DEVICE,
        HWSIM_ATTR_USE_CHANCTX,
        HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
        HWSIM_ATTR_RADIO_NAME,
        HWSIM_ATTR_NO_VIF,
        HWSIM_ATTR_FREQ,
        __HWSIM_ATTR_MAX,
};
#define HWSIM_ATTR_MAX (__HWSIM_ATTR_MAX - 1)

