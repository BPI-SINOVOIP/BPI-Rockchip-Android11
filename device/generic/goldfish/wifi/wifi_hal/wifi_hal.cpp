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

#include <memory>
#include "halstate.h"
#include "info.h"
#include "interface.h"

template<typename>
struct NotSupportedFunction;

template<typename R, typename... Args>
struct NotSupportedFunction<R (*)(Args...)> {
    static constexpr R invoke(Args...) { return WIFI_ERROR_NOT_SUPPORTED; }
};

template<typename... Args>
struct NotSupportedFunction<void (*)(Args...)> {
    static constexpr void invoke(Args...) { }
};

template<typename T>
void notSupported(T& val) {
    val = &NotSupportedFunction<T>::invoke;
}

HalState* asHalState(wifi_handle h) {
    return reinterpret_cast<HalState*>(h);
}

Info* asInfo(wifi_handle h) {
    return asHalState(h)->info();
}

Interface* asInterface(wifi_interface_handle h) {
    return reinterpret_cast<Interface*>(h);
}

wifi_error wifi_initialize(wifi_handle* handle) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    // Make the HAL state static inside the function for lazy construction. When
    // stopping we want to keep track of the current HAL state because if the
    // HAL starts again we need to know if we're in a state where we can start
    // or not. If we're stopping with the intention of never starting back up
    // again we could destroy the HAL state. Unfortunately there is no
    // distinction between these two events so the safe choice is to leak this
    // memory and always keep track of the HAL state. This is allocated on the
    // heap instead of the stack to prevent any destructors being called when
    // the dynamic library is being unloaded since the program state could be
    // unreliable at this point.
    static HalState* sHalState = new HalState();

    if (!sHalState->init()) {
        return WIFI_ERROR_UNKNOWN;
    }
    *handle = reinterpret_cast<wifi_handle>(sHalState);

    return WIFI_SUCCESS;
}

void wifi_cleanup(wifi_handle handle, wifi_cleaned_up_handler handler) {
    if (handle == nullptr) {
        return;
    }

    std::condition_variable condition;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool stopped = false;
    // This lambda will be called when the stop completes. That will notify the
    // condition variable and this function will wake up and exit. This ensures
    // that this function is synchronous. The boolean is to ensure that when
    // waiting we're protected against spurious wakeups, we only exit once the
    // callback has signaled that it's been called.
    auto callback = [&mutex, &stopped, &condition] {
        std::unique_lock<std::mutex> lock(mutex);
        stopped = true;
        condition.notify_all();
    };
    if (asHalState(handle)->stop(callback)) {
        // The handler succeeded and will call our callback, wait for it. If the
        // stop call did not succeed we can't wait for this condition since our
        // callback will never call notify on it.
        while (!stopped) {
            condition.wait(lock);
        }
    }
    // The HAL seems to expect this callback to happen on the same thread, or at
    // least that's what happens in other WiFi HALs. This is why this method has
    // to be synchronous.
    handler(handle);
}

void wifi_event_loop(wifi_handle handle) {
    if (handle == nullptr) {
        return;
    }

    asHalState(handle)->eventLoop();
}

wifi_error wifi_get_supported_feature_set(wifi_interface_handle handle,
                                          feature_set* set) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getSupportedFeatureSet(set);
}

wifi_error wifi_get_ifaces(wifi_handle handle,
                           int* num,
                           wifi_interface_handle** interfaces) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInfo(handle)->getInterfaces(num, interfaces);
}

wifi_error wifi_get_iface_name(wifi_interface_handle handle,
                               char* name,
                               size_t size) {
    if (handle == nullptr || (name == nullptr && size > 0)) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getName(name, size);
}

wifi_error wifi_get_link_stats(wifi_request_id id,
                               wifi_interface_handle handle,
                               wifi_stats_result_handler handler) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getLinkStats(id, handler);
}

wifi_error wifi_set_link_stats(wifi_interface_handle handle,
                               wifi_link_layer_params params) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->setLinkStats(params);
}

wifi_error wifi_set_alert_handler(wifi_request_id id,
                                  wifi_interface_handle handle,
                                  wifi_alert_handler handler) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->setAlertHandler(id, handler);
}

wifi_error wifi_reset_alert_handler(wifi_request_id id,
                                    wifi_interface_handle handle) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->resetAlertHandler(id);
}

wifi_error wifi_get_firmware_version(wifi_interface_handle handle,
                                     char* buffer,
                                     int buffer_size) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getFirmwareVersion(buffer, buffer_size);
}

wifi_error wifi_get_driver_version(wifi_interface_handle handle,
                                   char* buffer,
                                   int buffer_size) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getDriverVersion(buffer, buffer_size);
}

wifi_error wifi_set_scanning_mac_oui(wifi_interface_handle handle,
                                     oui scan_oui) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->setScanningMacOui(scan_oui);
}

wifi_error wifi_clear_link_stats(wifi_interface_handle handle,
                                 u32 stats_clear_req_mask,
                                 u32 *stats_clear_rsp_mask,
                                 u8 stop_req,
                                 u8 *stop_rsp) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->clearLinkStats(stats_clear_req_mask,
                                               stats_clear_rsp_mask,
                                               stop_req,
                                               stop_rsp);
}

wifi_error wifi_get_valid_channels(wifi_interface_handle handle,
                                   int band,
                                   int max_channels,
                                   wifi_channel *channels,
                                   int *num_channels)
{
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getValidChannels(band,
                                                 max_channels,
                                                 channels,
                                                 num_channels);
}

wifi_error wifi_start_logging(wifi_interface_handle handle,
                              u32 verbose_level,
                              u32 flags,
                              u32 max_interval_sec,
                              u32 min_data_size,
                              char *ring_name) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->startLogging(verbose_level,
                                             flags,
                                             max_interval_sec,
                                             min_data_size,
                                             ring_name);
}

wifi_error wifi_set_country_code(wifi_interface_handle handle,
                                 const char *country_code) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->setCountryCode(country_code);
}

wifi_error wifi_set_log_handler(wifi_request_id id,
                                wifi_interface_handle handle,
                                wifi_ring_buffer_data_handler handler) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->setLogHandler(id, handler);
}

wifi_error wifi_get_ring_buffers_status(wifi_interface_handle handle,
                                        u32 *num_rings,
                                        wifi_ring_buffer_status *status) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getRingBuffersStatus(num_rings, status);
}

wifi_error wifi_get_logger_supported_feature_set(wifi_interface_handle handle,
                                                 unsigned int *support) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getLoggerSupportedFeatureSet(support);
}

wifi_error wifi_get_ring_data(wifi_interface_handle handle, char *ring_name) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getRingData(ring_name);
}

wifi_error wifi_configure_nd_offload(wifi_interface_handle handle, u8 enable) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->configureNdOffload(enable);
}

wifi_error wifi_start_pkt_fate_monitoring(wifi_interface_handle handle) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->startPacketFateMonitoring();
}

wifi_error wifi_get_tx_pkt_fates(wifi_interface_handle handle,
                                 wifi_tx_report *tx_report_bufs,
                                 size_t n_requested_fates,
                                 size_t *n_provided_fates) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getTxPacketFates(tx_report_bufs,
                                                 n_requested_fates,
                                                 n_provided_fates);
}

wifi_error wifi_get_rx_pkt_fates(wifi_interface_handle handle,
                                 wifi_rx_report *rx_report_bufs,
                                 size_t n_requested_fates,
                                 size_t *n_provided_fates) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getRxPacketFates(rx_report_bufs,
                                                 n_requested_fates,
                                                 n_provided_fates);
}

wifi_error wifi_get_packet_filter_capabilities(wifi_interface_handle handle,
                                               u32 *version,
                                               u32 *max_len) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getPacketFilterCapabilities(version, max_len);
}

wifi_error
wifi_get_wake_reason_stats(wifi_interface_handle handle,
                           WLAN_DRIVER_WAKE_REASON_CNT *wifi_wake_reason_cnt) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->getWakeReasonStats(wifi_wake_reason_cnt);
}
wifi_error wifi_start_sending_offloaded_packet(wifi_request_id id,
                                               wifi_interface_handle handle,
                                               u16 ether_type,
                                               u8 *ip_packet,
                                               u16 ip_packet_len,
                                               u8 *src_mac_addr,
                                               u8 *dst_mac_addr,
                                               u32 period_msec) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->startSendingOffloadedPacket(id,
                                                            ether_type,
                                                            ip_packet,
                                                            ip_packet_len,
                                                            src_mac_addr,
                                                            dst_mac_addr,
                                                            period_msec);
}

wifi_error wifi_stop_sending_offloaded_packet(wifi_request_id id,
                                              wifi_interface_handle handle) {
    if (handle == nullptr) {
        return WIFI_ERROR_INVALID_ARGS;
    }

    return asInterface(handle)->stopSendingOffloadedPacket(id);
}

wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn* fn)
{
    if (fn == NULL) {
        return WIFI_ERROR_UNKNOWN;
    }
    fn->wifi_initialize = wifi_initialize;
    fn->wifi_cleanup = wifi_cleanup;
    fn->wifi_event_loop = wifi_event_loop;
    fn->wifi_get_supported_feature_set = wifi_get_supported_feature_set;

    fn->wifi_get_ifaces = wifi_get_ifaces;
    fn->wifi_get_iface_name = wifi_get_iface_name;
    fn->wifi_get_link_stats = wifi_get_link_stats;
    fn->wifi_set_link_stats = wifi_set_link_stats;
    fn->wifi_clear_link_stats = wifi_clear_link_stats;

    fn->wifi_set_alert_handler = wifi_set_alert_handler;
    fn->wifi_reset_alert_handler = wifi_reset_alert_handler;
    fn->wifi_get_firmware_version = wifi_get_firmware_version;
    fn->wifi_get_driver_version = wifi_get_driver_version;

    fn->wifi_set_scanning_mac_oui = wifi_set_scanning_mac_oui;
    fn->wifi_get_valid_channels = wifi_get_valid_channels;
    fn->wifi_start_logging = wifi_start_logging;
    fn->wifi_set_country_code = wifi_set_country_code;
    fn->wifi_set_log_handler = wifi_set_log_handler;
    fn->wifi_get_ring_buffers_status = wifi_get_ring_buffers_status;
    fn->wifi_get_logger_supported_feature_set
        = wifi_get_logger_supported_feature_set;
    fn->wifi_get_ring_data = wifi_get_ring_data;
    fn->wifi_configure_nd_offload = wifi_configure_nd_offload;
    fn->wifi_start_pkt_fate_monitoring = wifi_start_pkt_fate_monitoring;
    fn->wifi_get_tx_pkt_fates = wifi_get_tx_pkt_fates;
    fn->wifi_get_rx_pkt_fates = wifi_get_rx_pkt_fates;
    fn->wifi_get_packet_filter_capabilities
        = wifi_get_packet_filter_capabilities;
    fn->wifi_get_wake_reason_stats = wifi_get_wake_reason_stats;

    fn->wifi_start_sending_offloaded_packet
        = wifi_start_sending_offloaded_packet;
    fn->wifi_stop_sending_offloaded_packet = wifi_stop_sending_offloaded_packet;

    // These function will either return WIFI_ERROR_NOT_SUPPORTED or do nothing
    notSupported(fn->wifi_set_nodfs_flag);
    notSupported(fn->wifi_get_concurrency_matrix);
    notSupported(fn->wifi_start_gscan);
    notSupported(fn->wifi_stop_gscan);
    notSupported(fn->wifi_get_cached_gscan_results);
    notSupported(fn->wifi_set_bssid_hotlist);
    notSupported(fn->wifi_reset_bssid_hotlist);
    notSupported(fn->wifi_set_significant_change_handler);
    notSupported(fn->wifi_reset_significant_change_handler);
    notSupported(fn->wifi_get_gscan_capabilities);
    notSupported(fn->wifi_rtt_range_request);
    notSupported(fn->wifi_rtt_range_cancel);
    notSupported(fn->wifi_get_rtt_capabilities);
    notSupported(fn->wifi_rtt_get_responder_info);
    notSupported(fn->wifi_enable_responder);
    notSupported(fn->wifi_disable_responder);
    notSupported(fn->wifi_set_epno_list);
    notSupported(fn->wifi_reset_epno_list);
    notSupported(fn->wifi_get_firmware_memory_dump);
    notSupported(fn->wifi_reset_log_handler);
    notSupported(fn->wifi_start_rssi_monitoring);
    notSupported(fn->wifi_stop_rssi_monitoring);
    notSupported(fn->wifi_set_packet_filter);

    return WIFI_SUCCESS;
}

