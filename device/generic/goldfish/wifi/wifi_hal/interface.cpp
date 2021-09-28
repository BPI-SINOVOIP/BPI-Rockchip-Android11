/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "interface.h"

#include "log.h"
#include "netlink.h"
#include "netlinkmessage.h"

#include <linux/rtnetlink.h>

// Provide some arbitrary firmware and driver versions for now
static const char kFirmwareVersion[] = "1.0";
static const char kDriverVersion[] = "1.0";

// A list of supported channels in the 2.4 GHz band, values in MHz
static const wifi_channel k2p4Channels[] = {
    2412,
    2417,
    2422,
    2427,
    2432,
    2437,
    2442,
    2447,
    2452,
    2457,
    2462,
    2467,
    2472,
    2484
};

template<typename T, size_t N>
constexpr size_t arraySize(const T (&)[N]) {
    return N;
}

Interface::Interface(Netlink& netlink, const char* name)
    : mNetlink(netlink)
    , mName(name)
    , mInterfaceIndex(0) {
}

Interface::Interface(Interface&& other) noexcept
    : mNetlink(other.mNetlink)
    , mName(std::move(other.mName))
    , mInterfaceIndex(other.mInterfaceIndex) {
}

bool Interface::init() {
    mInterfaceIndex = if_nametoindex(mName.c_str());
    if (mInterfaceIndex == 0) {
        ALOGE("Unable to get interface index for %s", mName.c_str());
        return false;
    }
    return true;
}

wifi_error Interface::getSupportedFeatureSet(feature_set* set) {
    if (set == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    *set = 0;
    return WIFI_SUCCESS;
}

wifi_error Interface::getName(char* name, size_t size) {
    if (size < mName.size() + 1) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    strlcpy(name, mName.c_str(), size);
    return WIFI_SUCCESS;
}

// Wifi legacy HAL implicitly assumes getLinkStats is blocking and
// handler will be set to nullptr immediately after invocation.
// Therefore, this function will wait until onLinkStatsReply is called.
wifi_error Interface::getLinkStats(wifi_request_id requestId,
                                   wifi_stats_result_handler handler) {
    NetlinkMessage message(RTM_GETLINK, mNetlink.getSequenceNumber());

    ifinfomsg* info = message.payload<ifinfomsg>();
    info->ifi_family = AF_UNSPEC;
    info->ifi_type = 1;
    info->ifi_index = mInterfaceIndex;
    info->ifi_flags = 0;
    info->ifi_change = 0xFFFFFFFF;

    std::condition_variable condition;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool stopped = false;
    auto callback = [this, requestId, &handler,
        &mutex, &condition, &stopped] (const NetlinkMessage& message) {
        stopped = true;
        std::unique_lock<std::mutex> lock(mutex);
        onLinkStatsReply(requestId, handler, message);
        condition.notify_all();
    };
    bool success = mNetlink.sendMessage(message, callback);
    while (!stopped) {
        condition.wait(lock);
    }
    return success ? WIFI_SUCCESS : WIFI_ERROR_UNKNOWN;
}

wifi_error Interface::setLinkStats(wifi_link_layer_params /*params*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::setAlertHandler(wifi_request_id /*id*/,
                                         wifi_alert_handler /*handler*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::resetAlertHandler(wifi_request_id /*id*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::getFirmwareVersion(char* buffer, size_t size) {
    if (size < sizeof(kFirmwareVersion)) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    strcpy(buffer, kFirmwareVersion);
    return WIFI_SUCCESS;
}

wifi_error Interface::getDriverVersion(char* buffer, size_t size) {
    if (size < sizeof(kDriverVersion)) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    strcpy(buffer, kDriverVersion);
    return WIFI_SUCCESS;
}

wifi_error Interface::setScanningMacOui(oui /*scan_oui*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::clearLinkStats(u32 /*requestMask*/,
                                     u32* responseMask,
                                     u8 /*request*/,
                                     u8* response) {
    if (responseMask == nullptr || response == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    return WIFI_SUCCESS;
}

wifi_error Interface::getValidChannels(int band,
                                       int maxChannels,
                                       wifi_channel* channels,
                                       int* numChannels) {
    if (channels == nullptr || numChannels == nullptr || maxChannels < 0) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    switch (band) {
        case WIFI_BAND_BG: // 2.4 GHz
            *numChannels = std::min<int>(maxChannels,
                                         arraySize(k2p4Channels));
            memcpy(channels, k2p4Channels, *numChannels);

            return WIFI_SUCCESS;
        default:
            return WIFI_ERROR_NOT_SUPPORTED;
    }
}

wifi_error Interface::startLogging(u32 /*verboseLevel*/,
                                   u32 /*flags*/,
                                   u32 /*maxIntervalSec*/,
                                   u32 /*minDataSize*/,
                                   char* /*ringName*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::setCountryCode(const char* /*countryCode*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::setLogHandler(wifi_request_id /*id*/,
                                    wifi_ring_buffer_data_handler /*handler*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::getRingBuffersStatus(u32* numRings,
                                           wifi_ring_buffer_status* status) {
    if (numRings == nullptr || status == nullptr || *numRings == 0) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    memset(status, 0, sizeof(*status));
    strlcpy(reinterpret_cast<char*>(status->name),
            "ring0",
            sizeof(status->name));
    *numRings = 1;
    return WIFI_SUCCESS;
}

wifi_error Interface::getLoggerSupportedFeatureSet(unsigned int* support) {
    if (support == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    *support = 0;
    return WIFI_SUCCESS;
}

wifi_error Interface::getRingData(char* /*ringName*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::configureNdOffload(u8 /*enable*/) {
    return WIFI_SUCCESS;
}

wifi_error Interface::startPacketFateMonitoring() {
    return WIFI_SUCCESS;
}

wifi_error Interface::getTxPacketFates(wifi_tx_report* /*txReportBuffers*/,
                                       size_t /*numRequestedFates*/,
                                       size_t* numProvidedFates) {
    if (numProvidedFates == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    *numProvidedFates = 0;
    return WIFI_SUCCESS;
}

wifi_error Interface::getRxPacketFates(wifi_rx_report* /*rxReportBuffers*/,
                                       size_t /*numRequestedFates*/,
                                       size_t* numProvidedFates) {
    if (numProvidedFates == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    *numProvidedFates = 0;
    return WIFI_SUCCESS;
}

wifi_error Interface::getPacketFilterCapabilities(u32* version,
                                                  u32* maxLength) {
    if (version == nullptr || maxLength == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    *version = 0;
    *maxLength = 0;
    return WIFI_SUCCESS;
}

wifi_error
Interface::getWakeReasonStats(WLAN_DRIVER_WAKE_REASON_CNT* wakeReasonCount) {
    if (wakeReasonCount == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }
    return WIFI_SUCCESS;
}

wifi_error Interface::startSendingOffloadedPacket(wifi_request_id /*id*/,
                                                  u16 /*ether_type*/,
                                                  u8 * /*ip_packet*/,
                                                  u16 /*ip_packet_len*/,
                                                  u8 * /*src_mac_addr*/,
                                                  u8 * /*dst_mac_addr*/,
                                                  u32 /*period_msec*/) {
    // Drop the packet and pretend everything is fine. Currentlty this is only
    // used for keepalive packets to allow the CPU to go to sleep and let the
    // hardware send keepalive packets on its own. By dropping this we lose the
    // keepalive packets but networking will still be fine.
    return WIFI_SUCCESS;
}

wifi_error Interface::stopSendingOffloadedPacket(wifi_request_id /*id*/) {
    return WIFI_SUCCESS;
}

void Interface::onLinkStatsReply(wifi_request_id requestId,
                                 wifi_stats_result_handler handler,
                                 const NetlinkMessage& message) {
    if (message.size() < sizeof(nlmsghdr) + sizeof(ifinfomsg)) {
        ALOGE("Invalid link stats response, too small");
        return;
    }
    if (message.type() != RTM_NEWLINK) {
        ALOGE("Recieved invalid link stats reply type: %u",
              static_cast<unsigned int>(message.type()));
        return;
    }

    int numRadios = 1;
    wifi_radio_stat radioStats;
    memset(&radioStats, 0, sizeof(radioStats));

    wifi_iface_stat ifStats;
    memset(&ifStats, 0, sizeof(ifStats));
    ifStats.iface = reinterpret_cast<wifi_interface_handle>(this);

    rtnl_link_stats64 netlinkStats64;
    rtnl_link_stats netlinkStats;
    if (message.getAttribute(IFLA_STATS64, &netlinkStats64)) {
        ifStats.ac[WIFI_AC_BE].tx_mpdu = netlinkStats64.tx_packets;
        ifStats.ac[WIFI_AC_BE].rx_mpdu = netlinkStats64.rx_packets;
    } else if (message.getAttribute(IFLA_STATS, &netlinkStats)) {
        ifStats.ac[WIFI_AC_BE].tx_mpdu = netlinkStats.tx_packets;
        ifStats.ac[WIFI_AC_BE].rx_mpdu = netlinkStats.rx_packets;
    } else {
        return;
    }

    handler.on_link_stats_results(requestId, &ifStats, numRadios, &radioStats);
}
