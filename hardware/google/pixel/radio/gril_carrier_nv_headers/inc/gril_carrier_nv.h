/*
 * Copyright (C) 2020 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#ifndef GRIL_CARRIER_NV_H
#define GRIL_CARRIER_NV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GRIL_CARRIER_OPEN_MARKET = 0x00,
    GRIL_CARRIER_VZW = 0x01,
    GRIL_CARRIER_SPRINT = 0x02,
    GRIL_CARRIER_ATT = 0x03,
    GRIL_CARRIER_VODAFONE = 0x04,
    GRIL_CARRIER_TMOBILE = 0x05,
    GRIL_CARRIER_TELUS_V01 = 0x06,
    GRIL_CARRIER_KDDI = 0x07,
    GRIL_CARRIER_GEN_UMTS_EU = 0x08,
    GRIL_CARRIER_GEN_UMTS_NA = 0x09,
    GRIL_CARRIER_GEN_C2K = 0x0A,
    GRIL_CARRIER_ORANGE = 0x0B,
    GRIL_CARRIER_TELEFONICA = 0x0C,
    GRIL_CARRIER_DOCOMO = 0x0D,
    GRIL_CARRIER_TEL_ITALIA = 0x0E,
    GRIL_CARRIER_TELSTRA = 0x0F,
    GRIL_CARRIER_LUCACELL = 0x10,
    GRIL_CARRIER_BELL_MOB = 0x11,
    GRIL_CARRIER_TELCOM_NZ = 0x12,
    GRIL_CARRIER_CHINA_TEL = 0x13,
    GRIL_CARRIER_C2K_OMH = 0x14,
    GRIL_CARRIER_CHINA_UNI = 0x15,
    GRIL_CARRIER_AMX = 0x16,
    GRIL_CARRIER_NORX = 0x17,
    GRIL_CARRIER_US_CELLULAR = 0x18,
    GRIL_CARRIER_WONE = 0x19,
    GRIL_CARRIER_AIRTEL = 0x1A,
    GRIL_CARRIER_RELIANCE = 0x1B,
    GRIL_CARRIER_SOFTBANK = 0x1C,
    GRIL_CARRIER_DT = 0x1F,
    GRIL_CARRIER_CMCC = 0x20,
    GRIL_CARRIER_VIVO = 0x21,
    GRIL_CARRIER_EE = 0x22,
    GRIL_CARRIER_CHERRY = 0x23,
    GRIL_CARRIER_IMOBILE = 0x24,
    GRIL_CARRIER_SMARTFREN = 0x25,
    GRIL_CARRIER_LGU = 0x26,
    GRIL_CARRIER_SKT = 0x27,
    GRIL_CARRIER_TTA = 0x28,
    GRIL_CARRIER_BEELINE = 0x29,
    GRIL_CARRIER_H3G = 0x2A,
    GRIL_CARRIER_TIM = 0x2B,
    GRIL_CARRIER_MOV = 0x2C,
    GRIL_CARRIER_YTL = 0x2D,
    GRIL_CARRIER_AIS = 0x2E,
    GRIL_CARRIER_TRUEMOVE = 0x2F,
    GRIL_CARRIER_DTAC = 0x30,
    GRIL_CARRIER_HKT = 0x31,
    GRIL_CARRIER_M1 = 0x32,
    GRIL_CARRIER_STARHUB = 0x33,
    GRIL_CARRIER_SINGTEL = 0x34,
    GRIL_CARRIER_CSL = 0x35,
    GRIL_CARRIER_3HK = 0x36,
    GRIL_CARRIER_SMARTONE = 0x37,
    GRIL_CARRIER_SFR = 0x38,
    GRIL_CARRIER_PI = 0x39,
    GRIL_CARRIER_RUSSIA = 0x3A,
    GRIL_CARRIER_BOUYGUES = 0x3B,
    GRIL_CARRIER_MEGAFON = 0x3C,
    GRIL_CARRIER_UMOBILE = 0x3D,
    GRIL_CARRIER_SUNRISE = 0x3E,
    GRIL_CARRIER_OPTUS_ELISA = 0x44,
    GRIL_CARRIER_BELL = 0x47,
    GRIL_CARRIER_TELUS = 0x49,
    GRIL_CARRIER_FREEDOM = 0x51,
    GRIL_CARRIER_KOODO = 0x52,
    GRIL_CARRIER_FIRSTNET = 0x53,
    GRIL_CARRIER_ATT_5G = 0x57,
    GRIL_CARRIER_CSPIRE = 0x59,
    GRIL_CARRIER_CBRS = 0x64,
    GRIL_CARRIER_CRICKET_5G = 0xE2,
    GRIL_CARRIER_USCC_FI = 0xFB,
    GRIL_CARRIER_SPRINT_FI = 0xFC,
    GRIL_CARRIER_TMO_FI = 0xFD,
    GRIL_CARRIER_INVALID = 0xFFFFFFFF
} gril_carrier_nv_enum;

#ifdef __cplusplus
}
#endif

#endif
