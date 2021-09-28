/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CHRE_WWAN_H_
#define _CHRE_WWAN_H_

/**
 * @file
 * Wireless Wide Area Network (WWAN, i.e. mobile/cellular network) API relevant
 * for querying cell tower identity and associated information that can be
 * useful in determining location.
 *
 * Based on Android N RIL definitions (located at this path as of the time of
 * this comment: hardware/ril/include/telephony/ril.h), version 12. Updated
 * based on Android radio HAL definition (hardware/interfaces/radio) for more
 * recent Android builds. Refer to those files and associated documentation for
 * further details.
 *
 * In general, the parts of this API that are taken from the RIL follow the
 * field naming conventions established in that interface rather than the CHRE
 * API conventions, in order to avoid confusion and enable code re-use where
 * applicable. Note that structure names include the chreWwan* prefix rather
 * than RIL_*, but field names are the same. If necessary to enable code
 * sharing, it is recommended to create typedefs that map from the CHRE
 * structures to the associated RIL type names, for example "typedef struct
 * chreWwanCellIdentityGsm RIL_CellIdentityGsm_v12", etc.
 */

#include <chre/common.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The set of flags returned by chreWwanGetCapabilities().
 * @defgroup CHRE_WWAN_CAPABILITIES
 * @{
 */

//! No WWAN APIs are supported
#define CHRE_WWAN_CAPABILITIES_NONE  UINT32_C(0)

//! Current cell information can be queried via chreWwanGetCellInfoAsync()
#define CHRE_WWAN_GET_CELL_INFO      UINT32_C(1 << 0)

/** @} */

/**
 * Produce an event ID in the block of IDs reserved for WWAN
 * @param offset  Index into WWAN event ID block; valid range [0,15]
 */
#define CHRE_WWAN_EVENT_ID(offset)  (CHRE_EVENT_WWAN_FIRST_EVENT + (offset))

/**
 * nanoappHandleEvent argument: struct chreWwanCellInfoResult
 *
 * Provides the result of an asynchronous request for cell info sent via
 * chreWwanGetCellInfoAsync().
 */
#define CHRE_EVENT_WWAN_CELL_INFO_RESULT  CHRE_WWAN_EVENT_ID(0)

// NOTE: Do not add new events with ID > 15; only values 0-15 are reserved
// (see chre/event.h)

/**
 * The current version of struct chreWwanCellInfoResult associated with this
 * API definition.
 */
#define CHRE_WWAN_CELL_INFO_RESULT_VERSION  UINT8_C(1)

//! Reference: RIL_CellIdentityGsm_v12
struct chreWwanCellIdentityGsm {
    //! 3-digit Mobile Country Code, 0..999, INT32_MAX if unknown
    int32_t mcc;

    //! 2 or 3-digit Mobile Network Code, 0..999, INT32_MAX if unknown
    int32_t mnc;

    //! 16-bit Location Area Code, 0..65535, INT32_MAX if unknown
    int32_t lac;

    //! 16-bit GSM Cell Identity described in TS 27.007, 0..65535,
    //! INT32_MAX if unknown
    int32_t cid;

    //! 16-bit GSM Absolute RF channel number, INT32_MAX if unknown
    int32_t arfcn;

    //! 6-bit Base Station Identity Code, UINT8_MAX if unknown
    uint8_t bsic;

    //! Reserved for future use; must be set to 0
    uint8_t reserved[3];
};

//! Reference: RIL_CellIdentityWcdma_v12
struct chreWwanCellIdentityWcdma {
    //! 3-digit Mobile Country Code, 0..999, INT32_MAX if unknown
    int32_t mcc;

    //! 2 or 3-digit Mobile Network Code, 0..999, INT32_MAX if unknown
    int32_t mnc;

    //! 16-bit Location Area Code, 0..65535, INT32_MAX if unknown
    int32_t lac;

    //! 28-bit UMTS Cell Identity described in TS 25.331, 0..268435455,
    //! INT32_MAX if unknown
    int32_t cid;

    //! 9-bit UMTS Primary Scrambling Code described in TS 25.331, 0..511,
    //! INT32_MAX if unknown
    int32_t psc;

    //! 16-bit UMTS Absolute RF Channel Number, INT32_MAX if unknown
    int32_t uarfcn;
};

//! Reference: RIL_CellIdentityCdma
struct chreWwanCellIdentityCdma {
    //! Network Id 0..65535, INT32_MAX if unknown
    int32_t networkId;

    //! CDMA System Id 0..32767, INT32_MAX if unknown
    int32_t systemId;

    //! Base Station Id 0..65535, INT32_MAX if unknown
    int32_t basestationId;

    //! Longitude is a decimal number as specified in 3GPP2 C.S0005-A v6.0.
    //! It is represented in units of 0.25 seconds and ranges from -2592000
    //! to 2592000, both values inclusive (corresponding to a range of -180
    //! to +180 degrees). INT32_MAX if unknown
    int32_t longitude;

    //! Latitude is a decimal number as specified in 3GPP2 C.S0005-A v6.0.
    //! It is represented in units of 0.25 seconds and ranges from -1296000
    //! to 1296000, both values inclusive (corresponding to a range of -90
    //! to +90 degrees). INT32_MAX if unknown
    int32_t latitude;
};

//! Reference: RIL_CellIdentityLte_v12
struct chreWwanCellIdentityLte {
    //! 3-digit Mobile Country Code, 0..999, INT32_MAX if unknown
    int32_t mcc;

    //! 2 or 3-digit Mobile Network Code, 0..999, INT32_MAX if unknown
    int32_t mnc;

    //! 28-bit Cell Identity described in TS ???, INT32_MAX if unknown
    int32_t ci;

    //! physical cell id 0..503, INT32_MAX if unknown
    int32_t pci;

    //! 16-bit tracking area code, INT32_MAX if unknown
    int32_t tac;

    //! 18-bit LTE Absolute RF Channel Number, INT32_MAX if unknown
    int32_t earfcn;
};

//! Reference: RIL_CellIdentityTdscdma
struct chreWwanCellIdentityTdscdma {
    //! 3-digit Mobile Country Code, 0..999, INT32_MAX if unknown
    int32_t mcc;

    //! 2 or 3-digit Mobile Network Code, 0..999, INT32_MAX if unknown
    int32_t mnc;

    //! 16-bit Location Area Code, 0..65535, INT32_MAX if unknown
    int32_t lac;

    //! 28-bit UMTS Cell Identity described in TS 25.331, 0..268435455,
    //! INT32_MAX if unknown
    int32_t cid;

    //! 8-bit Cell Parameters ID described in TS 25.331, 0..127, INT32_MAX if
    //! unknown
    int32_t cpid;
};

//! Reference: android.hardware.radio@1.4 CellIdentityNr
//! @since v1.4
struct chreWwanCellIdentityNr {
    //! 3-digit Mobile Country Code, in range [0, 999]. This value must be valid
    //! for registered or camped cells. INT32_MAX means invalid/unreported.
    int32_t mcc;

    //! 2 or 3-digit Mobile Network Code, in range [0, 999]. This value must be
    //! valid for registered or camped cells. INT32_MAX means
    //! invalid/unreported.
    int32_t mnc;

    //! NR Cell Identity in range [0, 68719476735] (36 bits), which
    //! unambiguously identifies a cell within a public land mobile network
    //! (PLMN). This value must be valid for registered or camped cells.
    //! Reference: TS 38.413 section 9.3.1.7.
    //!
    //! Note: for backward compatibility reasons, the nominally int64_t nci is
    //! split into two uint32_t values, with nci0 being the least significant 4
    //! bytes. If chreWwanUnpackNrNci returns INT64_MAX, it means nci is
    //! invalid/unreported.
    //!
    //! Users are recommended to use the helper accessor chreWwanUnpackNrNci to
    //! access the nci field.
    //!
    //! @see chreWwanUnpackNrNci
    uint32_t nci0;
    uint32_t nci1;

    //! Physical cell id in range [0, 1007]. This value must be valid.
    //! Reference: TS 38.331 section 6.3.2.
    int32_t pci;

    //! 24-bit tracking area code in range [0, 16777215]. INT32_MAX means
    //! invalid/unreported.
    //! Reference: TS 38.413 section 9.3.3.10 and TS 29.571 section 5.4.2.
    int32_t tac;

    //! NR Absolute Radio Frequency Channel Number, in range [0, 3279165]. This
    //! value must be valid.
    //! Reference: TS 38.101-1 section 5.4.2.1 and TS 38.101-2 section 5.4.2.1.
    int32_t nrarfcn;
};

//! Reference: RIL_GSM_SignalStrength_v12
struct chreWwanSignalStrengthGsm {
    //! Valid values are (0-31, 99) as defined in TS 27.007 8.5
    //! INT32_MAX means invalid/unreported.
    int32_t signalStrength;

    //! bit error rate (0-7, 99) as defined in TS 27.007 8.5
    //! INT32_MAX means invalid/unreported.
    int32_t bitErrorRate;

    //! Timing Advance in bit periods. 1 bit period = 48.13 us.
    //! INT32_MAX means invalid/unreported.
    int32_t timingAdvance;
};

//! Reference: RIL_SignalStrengthWcdma
struct chreWwanSignalStrengthWcdma {
    //! Valid values are (0-31, 99) as defined in TS 27.007 8.5
    //! INT32_MAX means invalid/unreported.
    int32_t signalStrength;

    //! bit error rate (0-7, 99) as defined in TS 27.007 8.5
    //! INT32_MAX means invalid/unreported.
    int32_t bitErrorRate;
};

//! Reference: RIL_CDMA_SignalStrength
struct chreWwanSignalStrengthCdma {
    //! Valid values are positive integers.  This value is the actual RSSI value
    //! multiplied by -1.  Example: If the actual RSSI is -75, then this
    //! response value will be 75.
    //! INT32_MAX means invalid/unreported.
    int32_t dbm;

    //! Valid values are positive integers.  This value is the actual Ec/Io
    //! multiplied by -10.  Example: If the actual Ec/Io is -12.5 dB, then this
    //! response value will be 125.
    //! INT32_MAX means invalid/unreported.
    int32_t ecio;
};

//! Reference: RIL_EVDO_SignalStrength
struct chreWwanSignalStrengthEvdo {
    //! Valid values are positive integers.  This value is the actual RSSI value
    //! multiplied by -1.  Example: If the actual RSSI is -75, then this
    //! response value will be 75.
    //! INT32_MAX means invalid/unreported.
    int32_t dbm;

    //! Valid values are positive integers.  This value is the actual Ec/Io
    //! multiplied by -10.  Example: If the actual Ec/Io is -12.5 dB, then this
    //! response value will be 125.
    //! INT32_MAX means invalid/unreported.
    int32_t ecio;

    //! Valid values are 0-8.  8 is the highest signal to noise ratio.
    //! INT32_MAX means invalid/unreported.
    int32_t signalNoiseRatio;
};

//! Reference: RIL_LTE_SignalStrength_v8
struct chreWwanSignalStrengthLte {
    //! Valid values are (0-31, 99) as defined in TS 27.007 8.5
    int32_t signalStrength;

    //! The current Reference Signal Receive Power in dBm multipled by -1.
    //! Range: 44 to 140 dBm
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP TS 36.133 9.1.4
    int32_t rsrp;

    //! The current Reference Signal Receive Quality in dB multiplied by -1.
    //! Range: 3 to 20 dB.
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP TS 36.133 9.1.7
    int32_t rsrq;

    //! The current reference signal signal-to-noise ratio in 0.1 dB units.
    //! Range: -200 to +300 (-200 = -20.0 dB, +300 = 30dB).
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP TS 36.101 8.1.1
    int32_t rssnr;

    //! The current Channel Quality Indicator.
    //! Range: 0 to 15.
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP TS 36.101 9.2, 9.3, A.4
    int32_t cqi;

    //! timing advance in micro seconds for a one way trip from cell to device.
    //! Approximate distance can be calculated using 300m/us * timingAdvance.
    //! Range: 0 to 0x7FFFFFFE
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP 36.321 section 6.1.3.5
    //! also: http://www.cellular-planningoptimization.com/2010/02/timing-advance-with-calculation.html
    int32_t timingAdvance;
};

//! Reference: RIL_TD_SCDMA_SignalStrength
struct chreWwanSignalStrengthTdscdma {
    //! The Received Signal Code Power in dBm multipled by -1.
    //! Range : 25 to 120
    //! INT32_MAX means invalid/unreported.
    //! Reference: 3GPP TS 25.123, section 9.1.1.1
    int32_t rscp;
};

//! Reference: android.hardware.radio@1.4 NrSignalStrength
//! @since v1.4
struct chreWwanSignalStrengthNr {
    //! SS (second synchronization) reference signal received power in dBm
    //! multiplied by -1.
    //! Range [44, 140], INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.1 and TS 38.133 section 10.1.6.
    int32_t ssRsrp;

    //! SS reference signal received quality in 0.5 dB units.
    //! Range [-86, 41] with -86 = -43.0 dB and 41 = 20.5 dB.
    //! INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.3 and TS 38.133 section 10.1.11.1.
    int32_t ssRsrq;

    //! SS signal-to-noise and interference ratio in 0.5 dB units.
    //! Range [-46, 81] with -46 = -23.0 dB and 81 = 40.5 dB.
    //! INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.5 and TS 38.133 section 10.1.16.1.
    int32_t ssSinr;

    //! CSI reference signal received power in dBm multiplied by -1.
    //! Range [44, 140], INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.2 and TS 38.133 section 10.1.6.
    int32_t csiRsrp;

    //! CSI reference signal received quality in 0.5 dB units.
    //! Range [-86, 41] with -86 = -43.0 dB and 41 = 20.5 dB.
    //! INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.4 and TS 38.133 section 10.1.11.1.
    int32_t csiRsrq;

    //! CSI signal-to-noise and interference ratio in 0.5 dB units.
    //! Range [-46, 81] with -46 = -23.0 dB and 81 = 40.5 dB.
    //! INT32_MAX means invalid/unreported.
    //! Reference: TS 38.215 section 5.1.6 and TS 38.133 section 10.1.16.1.
    int32_t csiSinr;
};

//! Reference: RIL_CellInfoGsm_v12
struct chreWwanCellInfoGsm {
    struct chreWwanCellIdentityGsm    cellIdentityGsm;
    struct chreWwanSignalStrengthGsm  signalStrengthGsm;
};

//! Reference: RIL_CellInfoWcdma_v12
struct chreWwanCellInfoWcdma {
    struct chreWwanCellIdentityWcdma    cellIdentityWcdma;
    struct chreWwanSignalStrengthWcdma  signalStrengthWcdma;
};

//! Reference: RIL_CellInfoCdma
struct chreWwanCellInfoCdma {
    struct chreWwanCellIdentityCdma    cellIdentityCdma;
    struct chreWwanSignalStrengthCdma  signalStrengthCdma;
    struct chreWwanSignalStrengthEvdo  signalStrengthEvdo;
};

//! Reference: RIL_CellInfoLte_v12
struct chreWwanCellInfoLte {
    struct chreWwanCellIdentityLte    cellIdentityLte;
    struct chreWwanSignalStrengthLte  signalStrengthLte;
};

//! Reference: RIL_CellInfoTdscdma
struct chreWwanCellInfoTdscdma {
    struct chreWwanCellIdentityTdscdma    cellIdentityTdscdma;
    struct chreWwanSignalStrengthTdscdma  signalStrengthTdscdma;
};

//! Reference: android.hardware.radio@1.4 CellInfoNr
//! @since v1.4
struct chreWwanCellInfoNr {
    struct chreWwanCellIdentityNr    cellIdentityNr;
    struct chreWwanSignalStrengthNr  signalStrengthNr;
};

//! Reference: RIL_CellInfoType
//! All other values are reserved and should be ignored by nanoapps.
enum chreWwanCellInfoType {
    CHRE_WWAN_CELL_INFO_TYPE_GSM      = 1,
    CHRE_WWAN_CELL_INFO_TYPE_CDMA     = 2,
    CHRE_WWAN_CELL_INFO_TYPE_LTE      = 3,
    CHRE_WWAN_CELL_INFO_TYPE_WCDMA    = 4,
    CHRE_WWAN_CELL_INFO_TYPE_TD_SCDMA = 5,
    CHRE_WWAN_CELL_INFO_TYPE_NR       = 6,  //! @since v1.4
};

//! Reference: RIL_TimeStampType
enum chreWwanCellTimeStampType {
    CHRE_WWAN_CELL_TIMESTAMP_TYPE_UNKNOWN  = 0,
    CHRE_WWAN_CELL_TIMESTAMP_TYPE_ANTENNA  = 1,
    CHRE_WWAN_CELL_TIMESTAMP_TYPE_MODEM    = 2,
    CHRE_WWAN_CELL_TIMESTAMP_TYPE_OEM_RIL  = 3,
    CHRE_WWAN_CELL_TIMESTAMP_TYPE_JAVA_RIL = 4,
};

//! Reference: RIL_CellInfo_v12
struct chreWwanCellInfo {
    //! Timestamp in nanoseconds; must be in the same time base as chreGetTime()
    uint64_t timeStamp;

    //! A value from enum {@link #CellInfoType} indicating the radio access
    //! technology of the cell, and which field in union CellInfo can be used
    //! to retrieve additional information
    uint8_t cellInfoType;

    //! A value from enum {@link #CellTimeStampType} that identifies the source
    //! of the value in timeStamp. This is typically set to
    //! CHRE_WWAN_CELL_TIMESTAMP_TYPE_OEM_RIL, and indicates the time given by
    //! chreGetTime() that an intermediate module received the data from the
    //! modem and forwarded it to the requesting CHRE client.
    uint8_t timeStampType;

    //! !0 if this cell is registered, 0 if not registered
    uint8_t registered;

    //! Reserved for future use; must be set to 0
    uint8_t reserved;

    //! The value in cellInfoType indicates which field in this union is valid
    union chreWwanCellInfoPerRat {
        struct chreWwanCellInfoGsm     gsm;
        struct chreWwanCellInfoCdma    cdma;
        struct chreWwanCellInfoLte     lte;
        struct chreWwanCellInfoWcdma   wcdma;
        struct chreWwanCellInfoTdscdma tdscdma;
        struct chreWwanCellInfoNr      nr;  //! @since v1.4
    } CellInfo;
};

/**
 * Data structure provided with events of type CHRE_EVENT_WWAN_CELL_INFO_RESULT.
 */
struct chreWwanCellInfoResult {
    //! Indicates the version of the structure, for compatibility purposes.
    //! Clients do not normally need to worry about this field; the CHRE
    //! implementation guarantees that the client only receives the structure
    //! version it expects.
    uint8_t version;

    //! Populated with a value from enum {@link #chreError}, indicating whether
    //! the request failed, and if so, provides the cause of the failure
    uint8_t errorCode;

    //! The number of valid entries in cells[]
    uint8_t cellInfoCount;

    //! Reserved for future use; must be set to 0
    uint8_t reserved;

    //! Set to the cookie parameter given to chreWwanGetCellInfoAsync()
    const void *cookie;

    //! Pointer to an array of cellInfoCount elements containing information
    //! about serving and neighbor cells
    const struct chreWwanCellInfo *cells;
};


/**
 * Retrieves a set of flags indicating the WWAN features supported by the
 * current CHRE implementation. The value returned by this function must be
 * consistent for the entire duration of the Nanoapp's execution.
 *
 * The client must allow for more flags to be set in this response than it knows
 * about, for example if the implementation supports a newer version of the API
 * than the client was compiled against.
 *
 * @return A bitmask with zero or more CHRE_WWAN_CAPABILITIES_* flags set
 *
 * @since v1.1
 */
uint32_t chreWwanGetCapabilities(void);

/**
 * Query information about the current serving cell and its neighbors. This does
 * not perform a network scan, but should return state from the current network
 * registration data stored in the cellular modem. This is effectively the same
 * as a request for RIL_REQUEST_GET_CELL_INFO_LIST in the RIL.
 *
 * The requested cellular information is returned asynchronously via
 * CHRE_EVENT_WWAN_CELL_INFO_RESULT. The implementation must send this event,
 * either with successful data or an error status, within
 * CHRE_ASYNC_RESULT_TIMEOUT_NS.
 *
 * If the airplane mode setting is enabled at the Android level, the CHRE
 * implementation is expected to return a successful asynchronous result with an
 * empty cell info list.
 *
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 *
 * @return true if the request was accepted for processing, false otherwise
 *
 * @since v1.1
 */
bool chreWwanGetCellInfoAsync(const void *cookie);

/**
 * Helper accessor for nci in the chreWwanCellIdentityNr struct.
 *
 * @return nci or INT64_MAX if invalid/unreported.
 *
 * @see chreWwanCellIdentityNr
 *
 * @since v1.4
 */
static inline int64_t chreWwanUnpackNrNci(
    const struct chreWwanCellIdentityNr *nrCellId) {
  return (int64_t) (((uint64_t) nrCellId->nci1 << 32) | nrCellId->nci0);
}

#ifdef __cplusplus
}
#endif

#endif  /* _CHRE_WWAN_H_ */
