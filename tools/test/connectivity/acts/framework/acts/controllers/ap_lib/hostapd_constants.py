#!/usr/bin/env python3
#
#   Copyright 2017 - Google
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import itertools

BAND_2G = '2g'
BAND_5G = '5g'
WEP = 0
WPA1 = 1
WPA2 = 2
WPA3 = 2  # same as wpa2, distinguished by wpa_key_mgmt
MIXED = 3
ENT = 4  # get the correct constant
MAX_WPA_PSK_LENGTH = 64
MIN_WPA_PSK_LENGTH = 8
MAX_WPA_PASSWORD_LENGTH = 63
WPA_STRICT_REKEY = 1
WPA_DEFAULT_CIPHER = 'TKIP'
WPA2_DEFAULT_CIPER = 'CCMP'
WPA_GROUP_KEY_ROTATION_TIME = 600
WPA_STRICT_REKEY_DEFAULT = True
WPA_STRING = 'wpa'
WPA2_STRING = 'wpa2'
WPA_MIXED_STRING = 'wpa/wpa2'
WPA3_STRING = 'wpa3'
WPA3_KEY_MGMT = 'SAE'
ENT_STRING = 'ent'
ENT_KEY_MGMT = 'WPA-EAP'
IEEE8021X = 1
WLAN0_STRING = 'wlan0'
WLAN1_STRING = 'wlan1'
WLAN2_STRING = 'wlan2'
WLAN3_STRING = 'wlan3'
WLAN0_GALE = 'wlan-2400mhz'
WLAN1_GALE = 'wlan-5000mhz'
WEP_STRING = 'wep'
WEP_DEFAULT_KEY = 0
WEP_HEX_LENGTH = [10, 26, 32, 58]
WEP_STR_LENGTH = [5, 13, 16]
AP_DEFAULT_CHANNEL_2G = 6
AP_DEFAULT_CHANNEL_5G = 36
AP_DEFAULT_MAX_SSIDS_2G = 8
AP_DEFAULT_MAX_SSIDS_5G = 8
AP_SSID_LENGTH_2G = 8
AP_SSID_MIN_LENGTH_2G = 1
AP_SSID_MAX_LENGTH_2G = 32
AP_PASSPHRASE_LENGTH_2G = 10
AP_SSID_LENGTH_5G = 8
AP_SSID_MIN_LENGTH_5G = 1
AP_SSID_MAX_LENGTH_5G = 32
AP_PASSPHRASE_LENGTH_5G = 10
INTERFACE_2G_LIST = [WLAN0_STRING, WLAN0_GALE]
INTERFACE_5G_LIST = [WLAN1_STRING, WLAN1_GALE]
HIGH_BEACON_INTERVAL = 300
LOW_BEACON_INTERVAL = 100
HIGH_DTIM = 3
LOW_DTIM = 1

# A mapping of frequency to channel number.  This includes some
# frequencies used outside the US.
CHANNEL_MAP = {
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
    # 12, 13 are only legitimate outside the US.
    # 2467: 12,
    # 2472: 13,
    # 14 is for Japan, DSSS and CCK only.
    # 2484: 14,
    # 34 valid in Japan.
    # 5170: 34,
    # 36-116 valid in the US, except 38, 42, and 46, which have
    # mixed international support.
    5180: 36,
    5190: 38,
    5200: 40,
    5210: 42,
    5220: 44,
    5230: 46,
    5240: 48,
    # DFS channels.
    5260: 52,
    5280: 56,
    5300: 60,
    5320: 64,
    5500: 100,
    5520: 104,
    5540: 108,
    5560: 112,
    5580: 116,
    # 120, 124, 128 valid in Europe/Japan.
    5600: 120,
    5620: 124,
    5640: 128,
    # 132+ valid in US.
    5660: 132,
    5680: 136,
    5700: 140,
    # 144 is supported by a subset of WiFi chips
    # (e.g. bcm4354, but not ath9k).
    5720: 144,
    # End DFS channels.
    5745: 149,
    5755: 151,
    5765: 153,
    5775: 155,
    5795: 159,
    5785: 157,
    5805: 161,
    5825: 165
}
FREQUENCY_MAP = {v: k for k, v in CHANNEL_MAP.items()}

MODE_11A = 'a'
MODE_11B = 'b'
MODE_11G = 'g'
MODE_11N_MIXED = 'n-mixed'
MODE_11N_PURE = 'n-only'
MODE_11AC_MIXED = 'ac-mixed'
MODE_11AC_PURE = 'ac-only'

N_CAPABILITY_LDPC = object()
N_CAPABILITY_HT20 = object()
N_CAPABILITY_HT40_PLUS = object()
N_CAPABILITY_HT40_MINUS = object()
N_CAPABILITY_GREENFIELD = object()
N_CAPABILITY_SGI20 = object()
N_CAPABILITY_SGI40 = object()
N_CAPABILITY_TX_STBC = object()
N_CAPABILITY_RX_STBC1 = object()
N_CAPABILITY_RX_STBC12 = object()
N_CAPABILITY_RX_STBC123 = object()
N_CAPABILITY_DSSS_CCK_40 = object()
N_CAPABILITY_LSIG_TXOP_PROT = object()
N_CAPABILITY_40_INTOLERANT = object()
N_CAPABILITY_MAX_AMSDU_7935 = object()
N_CAPABILITY_DELAY_BLOCK_ACK = object()
N_CAPABILITY_SMPS_STATIC = object()
N_CAPABILITY_SMPS_DYNAMIC = object()
N_CAPABILITIES_MAPPING = {
    N_CAPABILITY_LDPC: '[LDPC]',
    N_CAPABILITY_HT20: '[HT20]',
    N_CAPABILITY_HT40_PLUS: '[HT40+]',
    N_CAPABILITY_HT40_MINUS: '[HT40-]',
    N_CAPABILITY_GREENFIELD: '[GF]',
    N_CAPABILITY_SGI20: '[SHORT-GI-20]',
    N_CAPABILITY_SGI40: '[SHORT-GI-40]',
    N_CAPABILITY_TX_STBC: '[TX-STBC]',
    N_CAPABILITY_RX_STBC1: '[RX-STBC1]',
    N_CAPABILITY_RX_STBC12: '[RX-STBC12]',
    N_CAPABILITY_RX_STBC123: '[RX-STBC123]',
    N_CAPABILITY_DSSS_CCK_40: '[DSSS_CCK-40]',
    N_CAPABILITY_LSIG_TXOP_PROT: '[LSIG-TXOP-PROT]',
    N_CAPABILITY_40_INTOLERANT: '[40-INTOLERANT]',
    N_CAPABILITY_MAX_AMSDU_7935: '[MAX-AMSDU-7935]',
    N_CAPABILITY_DELAY_BLOCK_ACK: '[DELAYED-BA]',
    N_CAPABILITY_SMPS_STATIC: '[SMPS-STATIC]',
    N_CAPABILITY_SMPS_DYNAMIC: '[SMPS-DYNAMIC]'
}
N_CAPABILITIES_MAPPING_INVERSE = {
    v: k
    for k, v in N_CAPABILITIES_MAPPING.items()
}
N_CAPABILITY_HT40_MINUS_CHANNELS = object()
N_CAPABILITY_HT40_PLUS_CHANNELS = object()
AC_CAPABILITY_VHT160 = object()
AC_CAPABILITY_VHT160_80PLUS80 = object()
AC_CAPABILITY_RXLDPC = object()
AC_CAPABILITY_SHORT_GI_80 = object()
AC_CAPABILITY_SHORT_GI_160 = object()
AC_CAPABILITY_TX_STBC_2BY1 = object()
AC_CAPABILITY_RX_STBC_1 = object()
AC_CAPABILITY_RX_STBC_12 = object()
AC_CAPABILITY_RX_STBC_123 = object()
AC_CAPABILITY_RX_STBC_1234 = object()
AC_CAPABILITY_SU_BEAMFORMER = object()
AC_CAPABILITY_SU_BEAMFORMEE = object()
AC_CAPABILITY_BF_ANTENNA_2 = object()
AC_CAPABILITY_BF_ANTENNA_3 = object()
AC_CAPABILITY_BF_ANTENNA_4 = object()
AC_CAPABILITY_SOUNDING_DIMENSION_2 = object()
AC_CAPABILITY_SOUNDING_DIMENSION_3 = object()
AC_CAPABILITY_SOUNDING_DIMENSION_4 = object()
AC_CAPABILITY_MU_BEAMFORMER = object()
AC_CAPABILITY_MU_BEAMFORMEE = object()
AC_CAPABILITY_VHT_TXOP_PS = object()
AC_CAPABILITY_HTC_VHT = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP0 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP1 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP2 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP3 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP4 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP5 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP6 = object()
AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7 = object()
AC_CAPABILITY_VHT_LINK_ADAPT2 = object()
AC_CAPABILITY_VHT_LINK_ADAPT3 = object()
AC_CAPABILITY_RX_ANTENNA_PATTERN = object()
AC_CAPABILITY_TX_ANTENNA_PATTERN = object()
AC_CAPABILITY_MAX_MPDU_7991 = object()
AC_CAPABILITY_MAX_MPDU_11454 = object()
AC_CAPABILITIES_MAPPING = {
    AC_CAPABILITY_VHT160: '[VHT160]',
    AC_CAPABILITY_VHT160_80PLUS80: '[VHT160-80PLUS80]',
    AC_CAPABILITY_RXLDPC: '[RXLDPC]',
    AC_CAPABILITY_SHORT_GI_80: '[SHORT-GI-80]',
    AC_CAPABILITY_SHORT_GI_160: '[SHORT-GI-160]',
    AC_CAPABILITY_TX_STBC_2BY1: '[TX-STBC-2BY1]',
    AC_CAPABILITY_RX_STBC_1: '[RX-STBC-1]',
    AC_CAPABILITY_RX_STBC_12: '[RX-STBC-12]',
    AC_CAPABILITY_RX_STBC_123: '[RX-STBC-123]',
    AC_CAPABILITY_RX_STBC_1234: '[RX-STBC-1234]',
    AC_CAPABILITY_SU_BEAMFORMER: '[SU-BEAMFORMER]',
    AC_CAPABILITY_SU_BEAMFORMEE: '[SU-BEAMFORMEE]',
    AC_CAPABILITY_BF_ANTENNA_2: '[BF-ANTENNA-2]',
    AC_CAPABILITY_BF_ANTENNA_3: '[BF-ANTENNA-3]',
    AC_CAPABILITY_BF_ANTENNA_4: '[BF-ANTENNA-4]',
    AC_CAPABILITY_SOUNDING_DIMENSION_2: '[SOUNDING-DIMENSION-2]',
    AC_CAPABILITY_SOUNDING_DIMENSION_3: '[SOUNDING-DIMENSION-3]',
    AC_CAPABILITY_SOUNDING_DIMENSION_4: '[SOUNDING-DIMENSION-4]',
    AC_CAPABILITY_MU_BEAMFORMER: '[MU-BEAMFORMER]',
    AC_CAPABILITY_MU_BEAMFORMEE: '[MU-BEAMFORMEE]',
    AC_CAPABILITY_VHT_TXOP_PS: '[VHT-TXOP-PS]',
    AC_CAPABILITY_HTC_VHT: '[HTC-VHT]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP0: '[MAX-A-MPDU-LEN-EXP0]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP1: '[MAX-A-MPDU-LEN-EXP1]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP2: '[MAX-A-MPDU-LEN-EXP2]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP3: '[MAX-A-MPDU-LEN-EXP3]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP4: '[MAX-A-MPDU-LEN-EXP4]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP5: '[MAX-A-MPDU-LEN-EXP5]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP6: '[MAX-A-MPDU-LEN-EXP6]',
    AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7: '[MAX-A-MPDU-LEN-EXP7]',
    AC_CAPABILITY_VHT_LINK_ADAPT2: '[VHT-LINK-ADAPT2]',
    AC_CAPABILITY_VHT_LINK_ADAPT3: '[VHT-LINK-ADAPT3]',
    AC_CAPABILITY_RX_ANTENNA_PATTERN: '[RX-ANTENNA-PATTERN]',
    AC_CAPABILITY_TX_ANTENNA_PATTERN: '[TX-ANTENNA-PATTERN]',
    AC_CAPABILITY_MAX_MPDU_11454: '[MAX-MPDU-11454]',
    AC_CAPABILITY_MAX_MPDU_7991: '[MAX-MPDU-7991]'
}
AC_CAPABILITIES_MAPPING_INVERSE = {
    v: k
    for k, v in AC_CAPABILITIES_MAPPING.items()
}
VHT_CHANNEL_WIDTH_40 = 0
VHT_CHANNEL_WIDTH_80 = 1
VHT_CHANNEL_WIDTH_160 = 2
VHT_CHANNEL_WIDTH_80_80 = 3

VHT_CHANNEL = {
    40: VHT_CHANNEL_WIDTH_40,
    80: VHT_CHANNEL_WIDTH_80,
    160: VHT_CHANNEL_WIDTH_160
}

# This is a loose merging of the rules for US and EU regulatory
# domains as taken from IEEE Std 802.11-2012 Appendix E.  For instance,
# we tolerate HT40 in channels 149-161 (not allowed in EU), but also
# tolerate HT40+ on channel 7 (not allowed in the US).  We take the loose
# definition so that we don't prohibit testing in either domain.
HT40_ALLOW_MAP = {
    N_CAPABILITY_HT40_MINUS_CHANNELS:
    tuple(
        itertools.chain(range(6, 14), range(40, 65, 8), range(104, 145, 8),
                        [153, 161])),
    N_CAPABILITY_HT40_PLUS_CHANNELS:
    tuple(
        itertools.chain(range(1, 8), range(36, 61, 8), range(100, 141, 8),
                        [149, 157]))
}

PMF_SUPPORT_DISABLED = 0
PMF_SUPPORT_ENABLED = 1
PMF_SUPPORT_REQUIRED = 2
PMF_SUPPORT_VALUES = (PMF_SUPPORT_DISABLED, PMF_SUPPORT_ENABLED,
                      PMF_SUPPORT_REQUIRED)

DRIVER_NAME = 'nl80211'

CENTER_CHANNEL_MAP = {
    VHT_CHANNEL_WIDTH_40: {
        'delta':
        2,
        'channels': ((36, 40), (44, 48), (52, 56), (60, 64), (100, 104),
                     (108, 112), (116, 120), (124, 128), (132, 136),
                     (140, 144), (149, 153), (157, 161))
    },
    VHT_CHANNEL_WIDTH_80: {
        'delta':
        6,
        'channels':
        ((36, 48), (52, 64), (100, 112), (116, 128), (132, 144), (149, 161))
    },
    VHT_CHANNEL_WIDTH_160: {
        'delta': 14,
        'channels': ((36, 64), (100, 128))
    }
}

OFDM_DATA_RATES = {'supported_rates': '60 90 120 180 240 360 480 540'}

CCK_DATA_RATES = {'supported_rates': '10 20 55 110'}

CCK_AND_OFDM_DATA_RATES = {
    'supported_rates': '10 20 55 110 60 90 120 180 240 360 480 540'
}

OFDM_ONLY_BASIC_RATES = {'basic_rates': '60 120 240'}

CCK_AND_OFDM_BASIC_RATES = {'basic_rates': '10 20 55 110'}

WEP_AUTH = {
    'open': {
        'auth_algs': 1
    },
    'shared': {
        'auth_algs': 2
    },
    'open_and_shared': {
        'auth_algs': 3
    }
}

WMM_11B_DEFAULT_PARAMS = {
    'wmm_ac_bk_cwmin': 5,
    'wmm_ac_bk_cwmax': 10,
    'wmm_ac_bk_aifs': 7,
    'wmm_ac_bk_txop_limit': 0,
    'wmm_ac_be_aifs': 3,
    'wmm_ac_be_cwmin': 5,
    'wmm_ac_be_cwmax': 7,
    'wmm_ac_be_txop_limit': 0,
    'wmm_ac_vi_aifs': 2,
    'wmm_ac_vi_cwmin': 4,
    'wmm_ac_vi_cwmax': 5,
    'wmm_ac_vi_txop_limit': 188,
    'wmm_ac_vo_aifs': 2,
    'wmm_ac_vo_cwmin': 3,
    'wmm_ac_vo_cwmax': 4,
    'wmm_ac_vo_txop_limit': 102
}

WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS = {
    'wmm_ac_bk_cwmin': 4,
    'wmm_ac_bk_cwmax': 10,
    'wmm_ac_bk_aifs': 7,
    'wmm_ac_bk_txop_limit': 0,
    'wmm_ac_be_aifs': 3,
    'wmm_ac_be_cwmin': 4,
    'wmm_ac_be_cwmax': 10,
    'wmm_ac_be_txop_limit': 0,
    'wmm_ac_vi_aifs': 2,
    'wmm_ac_vi_cwmin': 3,
    'wmm_ac_vi_cwmax': 4,
    'wmm_ac_vi_txop_limit': 94,
    'wmm_ac_vo_aifs': 2,
    'wmm_ac_vo_cwmin': 2,
    'wmm_ac_vo_cwmax': 3,
    'wmm_ac_vo_txop_limit': 47
}

WMM_NON_DEFAULT_PARAMS = {
    'wmm_ac_bk_cwmin': 5,
    'wmm_ac_bk_cwmax': 9,
    'wmm_ac_bk_aifs': 3,
    'wmm_ac_bk_txop_limit': 94,
    'wmm_ac_be_aifs': 2,
    'wmm_ac_be_cwmin': 2,
    'wmm_ac_be_cwmax': 8,
    'wmm_ac_be_txop_limit': 0,
    'wmm_ac_vi_aifs': 1,
    'wmm_ac_vi_cwmin': 7,
    'wmm_ac_vi_cwmax': 10,
    'wmm_ac_vi_txop_limit': 47,
    'wmm_ac_vo_aifs': 1,
    'wmm_ac_vo_cwmin': 6,
    'wmm_ac_vo_cwmax': 10,
    'wmm_ac_vo_txop_limit': 94
}

WMM_ACM_BK = {'wmm_ac_bk_acm': 1}
WMM_ACM_BE = {'wmm_ac_be_acm': 1}
WMM_ACM_VI = {'wmm_ac_vi_acm': 1}
WMM_ACM_VO = {'wmm_ac_vo_acm': 1}

UAPSD_ENABLED = {'uapsd_advertisement_enabled': 1}

UTF_8_SSID = {'utf8_ssid': 1}

ENABLE_RRM_BEACON_REPORT = {'rrm_beacon_report': 1}
ENABLE_RRM_NEIGHBOR_REPORT = {'rrm_neighbor_report': 1}

VENDOR_IE = {
    'correct_length_beacon': {
        'vendor_elements': 'dd0411223301'
    },
    'too_short_length_beacon': {
        'vendor_elements': 'dd0311223301'
    },
    'too_long_length_beacon': {
        'vendor_elements': 'dd0511223301'
    },
    'zero_length_beacon_with_data': {
        'vendor_elements': 'dd0011223301'
    },
    'zero_length_beacon_without_data': {
        'vendor_elements': 'dd00'
    },
    'simliar_to_wpa': {
        'vendor_elements': 'dd040050f203'
    },
    'correct_length_association_response': {
        'assocresp_elements': 'dd0411223301'
    },
    'too_short_length_association_response': {
        'assocresp_elements': 'dd0311223301'
    },
    'too_long_length_association_response': {
        'assocresp_elements': 'dd0511223301'
    },
    'zero_length_association_response_with_data': {
        'assocresp_elements': 'dd0011223301'
    },
    'zero_length_association_response_without_data': {
        'assocresp_elements': 'dd00'
    }
}

ENABLE_IEEE80211D = {'ieee80211d': 1}

COUNTRY_STRING = {
    'ALL': {
        'country3': '0x20'
    },
    'OUTDOOR': {
        'country3': '0x4f'
    },
    'INDOOR': {
        'country3': '0x49'
    },
    'NONCOUNTRY': {
        'country3': '0x58'
    },
    'GLOBAL': {
        'country3': '0x04'
    }
}

COUNTRY_CODE = {
    'AFGHANISTAN': {
        'country_code': 'AF'
    },
    'ALAND_ISLANDS': {
        'country_code': 'AX'
    },
    'ALBANIA': {
        'country_code': 'AL'
    },
    'ALGERIA': {
        'country_code': 'DZ'
    },
    'AMERICAN_SAMOA': {
        'country_code': 'AS'
    },
    'ANDORRA': {
        'country_code': 'AD'
    },
    'ANGOLA': {
        'country_code': 'AO'
    },
    'ANGUILLA': {
        'country_code': 'AI'
    },
    'ANTARCTICA': {
        'country_code': 'AQ'
    },
    'ANTIGUA_AND_BARBUDA': {
        'country_code': 'AG'
    },
    'ARGENTINA': {
        'country_code': 'AR'
    },
    'ARMENIA': {
        'country_code': 'AM'
    },
    'ARUBA': {
        'country_code': 'AW'
    },
    'AUSTRALIA': {
        'country_code': 'AU'
    },
    'AUSTRIA': {
        'country_code': 'AT'
    },
    'AZERBAIJAN': {
        'country_code': 'AZ'
    },
    'BAHAMAS': {
        'country_code': 'BS'
    },
    'BAHRAIN': {
        'country_code': 'BH'
    },
    'BANGLADESH': {
        'country_code': 'BD'
    },
    'BARBADOS': {
        'country_code': 'BB'
    },
    'BELARUS': {
        'country_code': 'BY'
    },
    'BELGIUM': {
        'country_code': 'BE'
    },
    'BELIZE': {
        'country_code': 'BZ'
    },
    'BENIN': {
        'country_code': 'BJ'
    },
    'BERMUDA': {
        'country_code': 'BM'
    },
    'BHUTAN': {
        'country_code': 'BT'
    },
    'BOLIVIA': {
        'country_code': 'BO'
    },
    'BONAIRE': {
        'country_code': 'BQ'
    },
    'BOSNIA_AND_HERZEGOVINA': {
        'country_code': 'BA'
    },
    'BOTSWANA': {
        'country_code': 'BW'
    },
    'BOUVET_ISLAND': {
        'country_code': 'BV'
    },
    'BRAZIL': {
        'country_code': 'BR'
    },
    'BRITISH_INDIAN_OCEAN_TERRITORY': {
        'country_code': 'IO'
    },
    'BRUNEI_DARUSSALAM': {
        'country_code': 'BN'
    },
    'BULGARIA': {
        'country_code': 'BG'
    },
    'BURKINA_FASO': {
        'country_code': 'BF'
    },
    'BURUNDI': {
        'country_code': 'BI'
    },
    'CAMBODIA': {
        'country_code': 'KH'
    },
    'CAMEROON': {
        'country_code': 'CM'
    },
    'CANADA': {
        'country_code': 'CA'
    },
    'CAPE_VERDE': {
        'country_code': 'CV'
    },
    'CAYMAN_ISLANDS': {
        'country_code': 'KY'
    },
    'CENTRAL_AFRICAN_REPUBLIC': {
        'country_code': 'CF'
    },
    'CHAD': {
        'country_code': 'TD'
    },
    'CHILE': {
        'country_code': 'CL'
    },
    'CHINA': {
        'country_code': 'CN'
    },
    'CHRISTMAS_ISLAND': {
        'country_code': 'CX'
    },
    'COCOS_ISLANDS': {
        'country_code': 'CC'
    },
    'COLOMBIA': {
        'country_code': 'CO'
    },
    'COMOROS': {
        'country_code': 'KM'
    },
    'CONGO': {
        'country_code': 'CG'
    },
    'DEMOCRATIC_REPUBLIC_CONGO': {
        'country_code': 'CD'
    },
    'COOK_ISLANDS': {
        'country_code': 'CK'
    },
    'COSTA_RICA': {
        'country_code': 'CR'
    },
    'COTE_D_IVOIRE': {
        'country_code': 'CI'
    },
    'CROATIA': {
        'country_code': 'HR'
    },
    'CUBA': {
        'country_code': 'CU'
    },
    'CURACAO': {
        'country_code': 'CW'
    },
    'CYPRUS': {
        'country_code': 'CY'
    },
    'CZECH_REPUBLIC': {
        'country_code': 'CZ'
    },
    'DENMARK': {
        'country_code': 'DK'
    },
    'DJIBOUTI': {
        'country_code': 'DJ'
    },
    'DOMINICA': {
        'country_code': 'DM'
    },
    'DOMINICAN_REPUBLIC': {
        'country_code': 'DO'
    },
    'ECUADOR': {
        'country_code': 'EC'
    },
    'EGYPT': {
        'country_code': 'EG'
    },
    'EL_SALVADOR': {
        'country_code': 'SV'
    },
    'EQUATORIAL_GUINEA': {
        'country_code': 'GQ'
    },
    'ERITREA': {
        'country_code': 'ER'
    },
    'ESTONIA': {
        'country_code': 'EE'
    },
    'ETHIOPIA': {
        'country_code': 'ET'
    },
    'FALKLAND_ISLANDS_(MALVINAS)': {
        'country_code': 'FK'
    },
    'FAROE_ISLANDS': {
        'country_code': 'FO'
    },
    'FIJI': {
        'country_code': 'FJ'
    },
    'FINLAND': {
        'country_code': 'FI'
    },
    'FRANCE': {
        'country_code': 'FR'
    },
    'FRENCH_GUIANA': {
        'country_code': 'GF'
    },
    'FRENCH_POLYNESIA': {
        'country_code': 'PF'
    },
    'FRENCH_SOUTHERN_TERRITORIES': {
        'country_code': 'TF'
    },
    'GABON': {
        'country_code': 'GA'
    },
    'GAMBIA': {
        'country_code': 'GM'
    },
    'GEORGIA': {
        'country_code': 'GE'
    },
    'GERMANY': {
        'country_code': 'DE'
    },
    'GHANA': {
        'country_code': 'GH'
    },
    'GIBRALTAR': {
        'country_code': 'GI'
    },
    'GREECE': {
        'country_code': 'GR'
    },
    'GREENLAND': {
        'country_code': 'GL'
    },
    'GRENADA': {
        'country_code': 'GD'
    },
    'GUADELOUPE': {
        'country_code': 'GP'
    },
    'GUAM': {
        'country_code': 'GU'
    },
    'GUATEMALA': {
        'country_code': 'GT'
    },
    'GUERNSEY': {
        'country_code': 'GG'
    },
    'GUINEA': {
        'country_code': 'GN'
    },
    'GUINEA-BISSAU': {
        'country_code': 'GW'
    },
    'GUYANA': {
        'country_code': 'GY'
    },
    'HAITI': {
        'country_code': 'HT'
    },
    'HEARD_ISLAND_AND_MCDONALD_ISLANDS': {
        'country_code': 'HM'
    },
    'VATICAN_CITY_STATE': {
        'country_code': 'VA'
    },
    'HONDURAS': {
        'country_code': 'HN'
    },
    'HONG_KONG': {
        'country_code': 'HK'
    },
    'HUNGARY': {
        'country_code': 'HU'
    },
    'ICELAND': {
        'country_code': 'IS'
    },
    'INDIA': {
        'country_code': 'IN'
    },
    'INDONESIA': {
        'country_code': 'ID'
    },
    'IRAN': {
        'country_code': 'IR'
    },
    'IRAQ': {
        'country_code': 'IQ'
    },
    'IRELAND': {
        'country_code': 'IE'
    },
    'ISLE_OF_MAN': {
        'country_code': 'IM'
    },
    'ISRAEL': {
        'country_code': 'IL'
    },
    'ITALY': {
        'country_code': 'IT'
    },
    'JAMAICA': {
        'country_code': 'JM'
    },
    'JAPAN': {
        'country_code': 'JP'
    },
    'JERSEY': {
        'country_code': 'JE'
    },
    'JORDAN': {
        'country_code': 'JO'
    },
    'KAZAKHSTAN': {
        'country_code': 'KZ'
    },
    'KENYA': {
        'country_code': 'KE'
    },
    'KIRIBATI': {
        'country_code': 'KI'
    },
    'DEMOCRATIC_PEOPLE_S_REPUBLIC_OF_KOREA': {
        'country_code': 'KP'
    },
    'REPUBLIC_OF_KOREA': {
        'country_code': 'KR'
    },
    'KUWAIT': {
        'country_code': 'KW'
    },
    'KYRGYZSTAN': {
        'country_code': 'KG'
    },
    'LAO': {
        'country_code': 'LA'
    },
    'LATVIA': {
        'country_code': 'LV'
    },
    'LEBANON': {
        'country_code': 'LB'
    },
    'LESOTHO': {
        'country_code': 'LS'
    },
    'LIBERIA': {
        'country_code': 'LR'
    },
    'LIBYA': {
        'country_code': 'LY'
    },
    'LIECHTENSTEIN': {
        'country_code': 'LI'
    },
    'LITHUANIA': {
        'country_code': 'LT'
    },
    'LUXEMBOURG': {
        'country_code': 'LU'
    },
    'MACAO': {
        'country_code': 'MO'
    },
    'MACEDONIA': {
        'country_code': 'MK'
    },
    'MADAGASCAR': {
        'country_code': 'MG'
    },
    'MALAWI': {
        'country_code': 'MW'
    },
    'MALAYSIA': {
        'country_code': 'MY'
    },
    'MALDIVES': {
        'country_code': 'MV'
    },
    'MALI': {
        'country_code': 'ML'
    },
    'MALTA': {
        'country_code': 'MT'
    },
    'MARSHALL_ISLANDS': {
        'country_code': 'MH'
    },
    'MARTINIQUE': {
        'country_code': 'MQ'
    },
    'MAURITANIA': {
        'country_code': 'MR'
    },
    'MAURITIUS': {
        'country_code': 'MU'
    },
    'MAYOTTE': {
        'country_code': 'YT'
    },
    'MEXICO': {
        'country_code': 'MX'
    },
    'MICRONESIA': {
        'country_code': 'FM'
    },
    'MOLDOVA': {
        'country_code': 'MD'
    },
    'MONACO': {
        'country_code': 'MC'
    },
    'MONGOLIA': {
        'country_code': 'MN'
    },
    'MONTENEGRO': {
        'country_code': 'ME'
    },
    'MONTSERRAT': {
        'country_code': 'MS'
    },
    'MOROCCO': {
        'country_code': 'MA'
    },
    'MOZAMBIQUE': {
        'country_code': 'MZ'
    },
    'MYANMAR': {
        'country_code': 'MM'
    },
    'NAMIBIA': {
        'country_code': 'NA'
    },
    'NAURU': {
        'country_code': 'NR'
    },
    'NEPAL': {
        'country_code': 'NP'
    },
    'NETHERLANDS': {
        'country_code': 'NL'
    },
    'NEW_CALEDONIA': {
        'country_code': 'NC'
    },
    'NEW_ZEALAND': {
        'country_code': 'NZ'
    },
    'NICARAGUA': {
        'country_code': 'NI'
    },
    'NIGER': {
        'country_code': 'NE'
    },
    'NIGERIA': {
        'country_code': 'NG'
    },
    'NIUE': {
        'country_code': 'NU'
    },
    'NORFOLK_ISLAND': {
        'country_code': 'NF'
    },
    'NORTHERN_MARIANA_ISLANDS': {
        'country_code': 'MP'
    },
    'NORWAY': {
        'country_code': 'NO'
    },
    'OMAN': {
        'country_code': 'OM'
    },
    'PAKISTAN': {
        'country_code': 'PK'
    },
    'PALAU': {
        'country_code': 'PW'
    },
    'PALESTINE': {
        'country_code': 'PS'
    },
    'PANAMA': {
        'country_code': 'PA'
    },
    'PAPUA_NEW_GUINEA': {
        'country_code': 'PG'
    },
    'PARAGUAY': {
        'country_code': 'PY'
    },
    'PERU': {
        'country_code': 'PE'
    },
    'PHILIPPINES': {
        'country_code': 'PH'
    },
    'PITCAIRN': {
        'country_code': 'PN'
    },
    'POLAND': {
        'country_code': 'PL'
    },
    'PORTUGAL': {
        'country_code': 'PT'
    },
    'PUERTO_RICO': {
        'country_code': 'PR'
    },
    'QATAR': {
        'country_code': 'QA'
    },
    'RÉUNION': {
        'country_code': 'RE'
    },
    'ROMANIA': {
        'country_code': 'RO'
    },
    'RUSSIAN_FEDERATION': {
        'country_code': 'RU'
    },
    'RWANDA': {
        'country_code': 'RW'
    },
    'SAINT_BARTHELEMY': {
        'country_code': 'BL'
    },
    'SAINT_KITTS_AND_NEVIS': {
        'country_code': 'KN'
    },
    'SAINT_LUCIA': {
        'country_code': 'LC'
    },
    'SAINT_MARTIN': {
        'country_code': 'MF'
    },
    'SAINT_PIERRE_AND_MIQUELON': {
        'country_code': 'PM'
    },
    'SAINT_VINCENT_AND_THE_GRENADINES': {
        'country_code': 'VC'
    },
    'SAMOA': {
        'country_code': 'WS'
    },
    'SAN_MARINO': {
        'country_code': 'SM'
    },
    'SAO_TOME_AND_PRINCIPE': {
        'country_code': 'ST'
    },
    'SAUDI_ARABIA': {
        'country_code': 'SA'
    },
    'SENEGAL': {
        'country_code': 'SN'
    },
    'SERBIA': {
        'country_code': 'RS'
    },
    'SEYCHELLES': {
        'country_code': 'SC'
    },
    'SIERRA_LEONE': {
        'country_code': 'SL'
    },
    'SINGAPORE': {
        'country_code': 'SG'
    },
    'SINT_MAARTEN': {
        'country_code': 'SX'
    },
    'SLOVAKIA': {
        'country_code': 'SK'
    },
    'SLOVENIA': {
        'country_code': 'SI'
    },
    'SOLOMON_ISLANDS': {
        'country_code': 'SB'
    },
    'SOMALIA': {
        'country_code': 'SO'
    },
    'SOUTH_AFRICA': {
        'country_code': 'ZA'
    },
    'SOUTH_GEORGIA': {
        'country_code': 'GS'
    },
    'SOUTH_SUDAN': {
        'country_code': 'SS'
    },
    'SPAIN': {
        'country_code': 'ES'
    },
    'SRI_LANKA': {
        'country_code': 'LK'
    },
    'SUDAN': {
        'country_code': 'SD'
    },
    'SURINAME': {
        'country_code': 'SR'
    },
    'SVALBARD_AND_JAN_MAYEN': {
        'country_code': 'SJ'
    },
    'SWAZILAND': {
        'country_code': 'SZ'
    },
    'SWEDEN': {
        'country_code': 'SE'
    },
    'SWITZERLAND': {
        'country_code': 'CH'
    },
    'SYRIAN_ARAB_REPUBLIC': {
        'country_code': 'SY'
    },
    'TAIWAN': {
        'country_code': 'TW'
    },
    'TAJIKISTAN': {
        'country_code': 'TJ'
    },
    'TANZANIA': {
        'country_code': 'TZ'
    },
    'THAILAND': {
        'country_code': 'TH'
    },
    'TIMOR-LESTE': {
        'country_code': 'TL'
    },
    'TOGO': {
        'country_code': 'TG'
    },
    'TOKELAU': {
        'country_code': 'TK'
    },
    'TONGA': {
        'country_code': 'TO'
    },
    'TRINIDAD_AND_TOBAGO': {
        'country_code': 'TT'
    },
    'TUNISIA': {
        'country_code': 'TN'
    },
    'TURKEY': {
        'country_code': 'TR'
    },
    'TURKMENISTAN': {
        'country_code': 'TM'
    },
    'TURKS_AND_CAICOS_ISLANDS': {
        'country_code': 'TC'
    },
    'TUVALU': {
        'country_code': 'TV'
    },
    'UGANDA': {
        'country_code': 'UG'
    },
    'UKRAINE': {
        'country_code': 'UA'
    },
    'UNITED_ARAB_EMIRATES': {
        'country_code': 'AE'
    },
    'UNITED_KINGDOM': {
        'country_code': 'GB'
    },
    'UNITED_STATES': {
        'country_code': 'US'
    },
    'UNITED_STATES_MINOR_OUTLYING_ISLANDS': {
        'country_code': 'UM'
    },
    'URUGUAY': {
        'country_code': 'UY'
    },
    'UZBEKISTAN': {
        'country_code': 'UZ'
    },
    'VANUATU': {
        'country_code': 'VU'
    },
    'VENEZUELA': {
        'country_code': 'VE'
    },
    'VIETNAM': {
        'country_code': 'VN'
    },
    'VIRGIN_ISLANDS_BRITISH': {
        'country_code': 'VG'
    },
    'VIRGIN_ISLANDS_US': {
        'country_code': 'VI'
    },
    'WALLIS_AND_FUTUNA': {
        'country_code': 'WF'
    },
    'WESTERN_SAHARA': {
        'country_code': 'EH'
    },
    'YEMEN': {
        'country_code': 'YE'
    },
    'ZAMBIA': {
        'country_code': 'ZM'
    },
    'ZIMBABWE': {
        'country_code': 'ZW'
    },
    'NON_COUNTRY': {
        'country_code': 'XX'
    }
}
