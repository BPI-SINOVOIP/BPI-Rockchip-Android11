/*
 * Copyright 2019 The Android Open Source Project
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

#include <base/logging.h>
#include <errno.h>
#include <fcntl.h>
#include <statslog.h>
#include <stdio.h>
#include <sys/stat.h>

#include "btif_bqr.h"
#include "btif_dm.h"
#include "common/leaky_bonded_queue.h"
#include "osi/include/properties.h"
#include "stack/btm/btm_int.h"

namespace bluetooth {
namespace bqr {

using bluetooth::common::LeakyBondedQueue;
using std::chrono::system_clock;

// The instance of BQR event queue
static std::unique_ptr<LeakyBondedQueue<BqrVseSubEvt>> kpBqrEventQueue(
    new LeakyBondedQueue<BqrVseSubEvt>(kBqrEventQueueSize));

void BqrVseSubEvt::ParseBqrLinkQualityEvt(uint8_t length,
                                          uint8_t* p_param_buf) {
  if (length < kLinkQualityParamTotalLen) {
    LOG(FATAL) << __func__
               << ": Parameter total length: " << std::to_string(length)
               << " is abnormal. It shall be not shorter than: "
               << std::to_string(kLinkQualityParamTotalLen);
    return;
  }

  STREAM_TO_UINT8(bqr_link_quality_event_.quality_report_id, p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.packet_types, p_param_buf);
  STREAM_TO_UINT16(bqr_link_quality_event_.connection_handle, p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.connection_role, p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.tx_power_level, p_param_buf);
  STREAM_TO_INT8(bqr_link_quality_event_.rssi, p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.snr, p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.unused_afh_channel_count,
                  p_param_buf);
  STREAM_TO_UINT8(bqr_link_quality_event_.afh_select_unideal_channel_count,
                  p_param_buf);
  STREAM_TO_UINT16(bqr_link_quality_event_.lsto, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.connection_piconet_clock,
                   p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.retransmission_count, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.no_rx_count, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.nak_count, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.last_tx_ack_timestamp, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.flow_off_count, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.last_flow_on_timestamp, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.buffer_overflow_bytes, p_param_buf);
  STREAM_TO_UINT32(bqr_link_quality_event_.buffer_underflow_bytes, p_param_buf);

  const auto now = system_clock::to_time_t(system_clock::now());
  localtime_r(&now, &tm_timestamp_);
}

void BqrVseSubEvt::WriteLmpLlTraceLogFile(int fd, uint8_t length,
                                          uint8_t* p_param_buf) {
  const auto now = system_clock::to_time_t(system_clock::now());
  localtime_r(&now, &tm_timestamp_);

  STREAM_TO_UINT8(bqr_log_dump_event_.quality_report_id, p_param_buf);
  STREAM_TO_UINT16(bqr_log_dump_event_.connection_handle, p_param_buf);
  length -= kLogDumpParamTotalLen;
  bqr_log_dump_event_.vendor_specific_parameter = p_param_buf;

  std::stringstream ss_log;
  ss_log << "\n"
         << std::put_time(&tm_timestamp_, "%m-%d %H:%M:%S ")
         << "Handle: " << loghex(bqr_log_dump_event_.connection_handle)
         << " VSP: ";

  TEMP_FAILURE_RETRY(write(fd, ss_log.str().c_str(), ss_log.str().size()));
  TEMP_FAILURE_RETRY(
      write(fd, bqr_log_dump_event_.vendor_specific_parameter, length));
  LmpLlMessageTraceCounter++;
}

void BqrVseSubEvt::WriteBtSchedulingTraceLogFile(int fd, uint8_t length,
                                                 uint8_t* p_param_buf) {
  const auto now = system_clock::to_time_t(system_clock::now());
  localtime_r(&now, &tm_timestamp_);

  STREAM_TO_UINT8(bqr_log_dump_event_.quality_report_id, p_param_buf);
  STREAM_TO_UINT16(bqr_log_dump_event_.connection_handle, p_param_buf);
  length -= kLogDumpParamTotalLen;
  bqr_log_dump_event_.vendor_specific_parameter = p_param_buf;

  std::stringstream ss_log;
  ss_log << "\n"
         << std::put_time(&tm_timestamp_, "%m-%d %H:%M:%S ")
         << "Handle: " << loghex(bqr_log_dump_event_.connection_handle)
         << " VSP: ";

  TEMP_FAILURE_RETRY(write(fd, ss_log.str().c_str(), ss_log.str().size()));
  TEMP_FAILURE_RETRY(
      write(fd, bqr_log_dump_event_.vendor_specific_parameter, length));
  BtSchedulingTraceCounter++;
}

std::string BqrVseSubEvt::ToString() const {
  std::stringstream ss;
  ss << QualityReportIdToString(bqr_link_quality_event_.quality_report_id)
     << ", Handle: " << loghex(bqr_link_quality_event_.connection_handle)
     << ", " << PacketTypeToString(bqr_link_quality_event_.packet_types) << ", "
     << ((bqr_link_quality_event_.connection_role == 0) ? "Master" : "Slave ")
     << ", PwLv: " << loghex(bqr_link_quality_event_.tx_power_level)
     << ", RSSI: " << std::to_string(bqr_link_quality_event_.rssi)
     << ", SNR: " << std::to_string(bqr_link_quality_event_.snr)
     << ", UnusedCh: "
     << std::to_string(bqr_link_quality_event_.unused_afh_channel_count)
     << ", UnidealCh: "
     << std::to_string(bqr_link_quality_event_.afh_select_unideal_channel_count)
     << ", ReTx: "
     << std::to_string(bqr_link_quality_event_.retransmission_count)
     << ", NoRX: " << std::to_string(bqr_link_quality_event_.no_rx_count)
     << ", NAK: " << std::to_string(bqr_link_quality_event_.nak_count)
     << ", FlowOff: " << std::to_string(bqr_link_quality_event_.flow_off_count)
     << ", OverFlow: "
     << std::to_string(bqr_link_quality_event_.buffer_overflow_bytes)
     << ", UndFlow: "
     << std::to_string(bqr_link_quality_event_.buffer_underflow_bytes);
  return ss.str();
}

std::string QualityReportIdToString(uint8_t quality_report_id) {
  switch (quality_report_id) {
    case QUALITY_REPORT_ID_MONITOR_MODE:
      return "Monitoring ";
    case QUALITY_REPORT_ID_APPROACH_LSTO:
      return "Appro LSTO ";
    case QUALITY_REPORT_ID_A2DP_AUDIO_CHOPPY:
      return "A2DP Choppy";
    case QUALITY_REPORT_ID_SCO_VOICE_CHOPPY:
      return "SCO Choppy ";
    default:
      return "Invalid    ";
  }
}

std::string PacketTypeToString(uint8_t packet_type) {
  switch (packet_type) {
    case PACKET_TYPE_ID:
      return "ID";
    case PACKET_TYPE_NULL:
      return "NULL";
    case PACKET_TYPE_POLL:
      return "POLL";
    case PACKET_TYPE_FHS:
      return "FHS";
    case PACKET_TYPE_HV1:
      return "HV1";
    case PACKET_TYPE_HV2:
      return "HV2";
    case PACKET_TYPE_HV3:
      return "HV3";
    case PACKET_TYPE_DV:
      return "DV";
    case PACKET_TYPE_EV3:
      return "EV3";
    case PACKET_TYPE_EV4:
      return "EV4";
    case PACKET_TYPE_EV5:
      return "EV5";
    case PACKET_TYPE_2EV3:
      return "2EV3";
    case PACKET_TYPE_2EV5:
      return "2EV5";
    case PACKET_TYPE_3EV3:
      return "3EV3";
    case PACKET_TYPE_3EV5:
      return "3EV5";
    case PACKET_TYPE_DM1:
      return "DM1";
    case PACKET_TYPE_DH1:
      return "DH1";
    case PACKET_TYPE_DM3:
      return "DM3";
    case PACKET_TYPE_DH3:
      return "DH3";
    case PACKET_TYPE_DM5:
      return "DM5";
    case PACKET_TYPE_DH5:
      return "DH5";
    case PACKET_TYPE_AUX1:
      return "AUX1";
    case PACKET_TYPE_2DH1:
      return "2DH1";
    case PACKET_TYPE_2DH3:
      return "2DH3";
    case PACKET_TYPE_2DH5:
      return "2DH5";
    case PACKET_TYPE_3DH1:
      return "3DH1";
    case PACKET_TYPE_3DH3:
      return "3DH3";
    case PACKET_TYPE_3DH5:
      return "3DH5";
    default:
      return "UnKnown ";
  }
}

void EnableBtQualityReport(bool is_enable) {
  LOG(INFO) << __func__ << ": is_enable: " << logbool(is_enable);

  char bqr_prop_evtmask[PROPERTY_VALUE_MAX] = {0};
  char bqr_prop_interval_ms[PROPERTY_VALUE_MAX] = {0};
  osi_property_get(kpPropertyEventMask, bqr_prop_evtmask, "");
  osi_property_get(kpPropertyMinReportIntervalMs, bqr_prop_interval_ms, "");

  if (strlen(bqr_prop_evtmask) == 0 || strlen(bqr_prop_interval_ms) == 0) {
    LOG(WARNING) << __func__ << ": Bluetooth Quality Report is disabled."
                 << " bqr_prop_evtmask: " << bqr_prop_evtmask
                 << ", bqr_prop_interval_ms: " << bqr_prop_interval_ms;
    return;
  }

  BqrConfiguration bqr_config = {};

  if (is_enable) {
    bqr_config.report_action = REPORT_ACTION_ADD;
    bqr_config.quality_event_mask =
        static_cast<uint32_t>(atoi(bqr_prop_evtmask));
    bqr_config.minimum_report_interval_ms =
        static_cast<uint16_t>(atoi(bqr_prop_interval_ms));
  } else {
    bqr_config.report_action = REPORT_ACTION_CLEAR;
    bqr_config.quality_event_mask = kQualityEventMaskAllOff;
    bqr_config.minimum_report_interval_ms = kMinReportIntervalNoLimit;
  }

  LOG(INFO) << __func__
            << ": Event Mask: " << loghex(bqr_config.quality_event_mask)
            << ", Interval: " << bqr_config.minimum_report_interval_ms;
  ConfigureBqr(bqr_config);
}

void ConfigureBqr(const BqrConfiguration& bqr_config) {
  if (bqr_config.report_action > REPORT_ACTION_CLEAR ||
      bqr_config.quality_event_mask > kQualityEventMaskAll ||
      bqr_config.minimum_report_interval_ms > kMinReportIntervalMaxMs) {
    LOG(FATAL) << __func__ << ": Invalid Parameter"
               << ", Action: " << bqr_config.report_action
               << ", Mask: " << loghex(bqr_config.quality_event_mask)
               << ", Interval: " << bqr_config.minimum_report_interval_ms;
    return;
  }

  LOG(INFO) << __func__ << ": Action: " << bqr_config.report_action
            << ", Mask: " << loghex(bqr_config.quality_event_mask)
            << ", Interval: " << bqr_config.minimum_report_interval_ms;

  uint8_t param[sizeof(BqrConfiguration)];
  uint8_t* p_param = param;
  UINT8_TO_STREAM(p_param, bqr_config.report_action);
  UINT32_TO_STREAM(p_param, bqr_config.quality_event_mask);
  UINT16_TO_STREAM(p_param, bqr_config.minimum_report_interval_ms);

  BTM_VendorSpecificCommand(HCI_CONTROLLER_BQR, p_param - param, param,
                            BqrVscCompleteCallback);
}

void BqrVscCompleteCallback(tBTM_VSC_CMPL* p_vsc_cmpl_params) {
  if (p_vsc_cmpl_params->param_len < 1) {
    LOG(ERROR) << __func__ << ": The length of returned parameters is less than 1";
    return;
  }

  uint8_t* p_event_param_buf = p_vsc_cmpl_params->p_param_buf;
  uint8_t status = 0xff;
  // [Return Parameter]         | [Size]   | [Purpose]
  // Status                     | 1 octet  | Command complete status
  // Current_Quality_Event_Mask | 4 octets | Indicates current bit mask setting
  STREAM_TO_UINT8(status, p_event_param_buf);
  if (status != HCI_SUCCESS) {
    LOG(ERROR) << __func__ << ": Fail to configure BQR. status: " << loghex(status);
    return;
  }

  if (p_vsc_cmpl_params->param_len != 5) {
    LOG(FATAL) << __func__
               << ": The length of returned parameters is not equal to 5: "
               << std::to_string(p_vsc_cmpl_params->param_len);
    return;
  }

  uint32_t current_quality_event_mask = kQualityEventMaskAllOff;
  STREAM_TO_UINT32(current_quality_event_mask, p_event_param_buf);

  LOG(INFO) << __func__
            << ", current event mask: " << loghex(current_quality_event_mask);
  ConfigureBqrCmpl(current_quality_event_mask);
}

void ConfigureBqrCmpl(uint32_t current_evt_mask) {
  LOG(INFO) << __func__ << ": current_evt_mask: " << loghex(current_evt_mask);
  // (Un)Register for VSE of Bluetooth Quality Report sub event
  tBTM_STATUS btm_status = BTM_BT_Quality_Report_VSE_Register(
      current_evt_mask > kQualityEventMaskAllOff, CategorizeBqrEvent);

  if (btm_status != BTM_SUCCESS) {
    LOG(ERROR) << __func__ << ": Fail to (un)register VSE of BQR sub event."
               << " status: " << btm_status;
    return;
  }

  if (LmpLlMessageTraceLogFd != INVALID_FD &&
      (current_evt_mask & kQualityEventMaskLmpMessageTrace) == 0) {
    LOG(INFO) << __func__ << ": Closing LMP/LL log file.";
    close(LmpLlMessageTraceLogFd);
    LmpLlMessageTraceLogFd = INVALID_FD;
  }
  if (BtSchedulingTraceLogFd != INVALID_FD &&
      (current_evt_mask & kQualityEventMaskBtSchedulingTrace) == 0) {
    LOG(INFO) << __func__ << ": Closing Scheduling log file.";
    close(BtSchedulingTraceLogFd);
    BtSchedulingTraceLogFd = INVALID_FD;
  }
}

void CategorizeBqrEvent(uint8_t length, uint8_t* p_bqr_event) {
  if (length == 0) {
    LOG(WARNING) << __func__ << ": Lengths of all of the parameters are zero.";
    return;
  }

  uint8_t quality_report_id = p_bqr_event[0];
  switch (quality_report_id) {
    case QUALITY_REPORT_ID_MONITOR_MODE:
    case QUALITY_REPORT_ID_APPROACH_LSTO:
    case QUALITY_REPORT_ID_A2DP_AUDIO_CHOPPY:
    case QUALITY_REPORT_ID_SCO_VOICE_CHOPPY:
      if (length < kLinkQualityParamTotalLen) {
        LOG(FATAL) << __func__
                   << ": Parameter total length: " << std::to_string(length)
                   << " is abnormal. It shall be not shorter than: "
                   << std::to_string(kLinkQualityParamTotalLen);
        return;
      }

      AddLinkQualityEventToQueue(length, p_bqr_event);
      break;

    // The Root Inflammation and Log Dump related event should be handled and
    // intercepted already.
    case QUALITY_REPORT_ID_ROOT_INFLAMMATION:
    case QUALITY_REPORT_ID_LMP_LL_MESSAGE_TRACE:
    case QUALITY_REPORT_ID_BT_SCHEDULING_TRACE:
    case QUALITY_REPORT_ID_CONTROLLER_DBG_INFO:
      LOG(WARNING) << __func__
                   << ": Unexpected ID: " << loghex(quality_report_id);
      break;

    default:
      LOG(WARNING) << __func__ << ": Unknown ID: " << loghex(quality_report_id);
      break;
  }
}

void AddLinkQualityEventToQueue(uint8_t length, uint8_t* p_link_quality_event) {
  std::unique_ptr<BqrVseSubEvt> p_bqr_event = std::make_unique<BqrVseSubEvt>();
  p_bqr_event->ParseBqrLinkQualityEvt(length, p_link_quality_event);

  LOG(WARNING) << *p_bqr_event;
  int ret = android::util::stats_write(
      android::util::BLUETOOTH_QUALITY_REPORT_REPORTED,
      p_bqr_event->bqr_link_quality_event_.quality_report_id,
      p_bqr_event->bqr_link_quality_event_.packet_types,
      p_bqr_event->bqr_link_quality_event_.connection_handle,
      p_bqr_event->bqr_link_quality_event_.connection_role,
      p_bqr_event->bqr_link_quality_event_.tx_power_level,
      p_bqr_event->bqr_link_quality_event_.rssi,
      p_bqr_event->bqr_link_quality_event_.snr,
      p_bqr_event->bqr_link_quality_event_.unused_afh_channel_count,
      p_bqr_event->bqr_link_quality_event_.afh_select_unideal_channel_count,
      p_bqr_event->bqr_link_quality_event_.lsto,
      p_bqr_event->bqr_link_quality_event_.connection_piconet_clock,
      p_bqr_event->bqr_link_quality_event_.retransmission_count,
      p_bqr_event->bqr_link_quality_event_.no_rx_count,
      p_bqr_event->bqr_link_quality_event_.nak_count,
      p_bqr_event->bqr_link_quality_event_.last_tx_ack_timestamp,
      p_bqr_event->bqr_link_quality_event_.flow_off_count,
      p_bqr_event->bqr_link_quality_event_.last_flow_on_timestamp,
      p_bqr_event->bqr_link_quality_event_.buffer_overflow_bytes,
      p_bqr_event->bqr_link_quality_event_.buffer_underflow_bytes);
  if (ret < 0) {
    LOG(WARNING) << __func__ << ": failed to log BQR event to statsd, error "
                 << ret;
  }
  kpBqrEventQueue->Enqueue(p_bqr_event.release());
}

void DumpLmpLlMessage(uint8_t length, uint8_t* p_lmp_ll_message_event) {
  std::unique_ptr<BqrVseSubEvt> p_bqr_event = std::make_unique<BqrVseSubEvt>();

  if (LmpLlMessageTraceLogFd == INVALID_FD ||
      LmpLlMessageTraceCounter >= kLogDumpEventPerFile) {
    LmpLlMessageTraceLogFd = OpenLmpLlTraceLogFile();
  }
  if (LmpLlMessageTraceLogFd != INVALID_FD) {
    p_bqr_event->WriteLmpLlTraceLogFile(LmpLlMessageTraceLogFd, length,
                                        p_lmp_ll_message_event);
  }
}

int OpenLmpLlTraceLogFile() {
  if (rename(kpLmpLlMessageTraceLogPath, kpLmpLlMessageTraceLastLogPath) != 0 &&
      errno != ENOENT) {
    LOG(ERROR) << __func__ << ": Unable to rename '"
               << kpLmpLlMessageTraceLogPath << "' to '"
               << kpLmpLlMessageTraceLastLogPath << "' : " << strerror(errno);
  }

  mode_t prevmask = umask(0);
  int logfile_fd =
      open(kpLmpLlMessageTraceLogPath, O_WRONLY | O_CREAT | O_TRUNC,
           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  umask(prevmask);
  if (logfile_fd == INVALID_FD) {
    LOG(ERROR) << __func__ << ": Unable to open '" << kpLmpLlMessageTraceLogPath
               << "' : " << strerror(errno);
  } else {
    LmpLlMessageTraceCounter = 0;
  }
  return logfile_fd;
}

void DumpBtScheduling(uint8_t length, uint8_t* p_bt_scheduling_event) {
  std::unique_ptr<BqrVseSubEvt> p_bqr_event = std::make_unique<BqrVseSubEvt>();

  if (BtSchedulingTraceLogFd == INVALID_FD ||
      BtSchedulingTraceCounter == kLogDumpEventPerFile) {
    BtSchedulingTraceLogFd = OpenBtSchedulingTraceLogFile();
  }
  if (BtSchedulingTraceLogFd != INVALID_FD) {
    p_bqr_event->WriteBtSchedulingTraceLogFile(BtSchedulingTraceLogFd, length,
                                               p_bt_scheduling_event);
  }
}

int OpenBtSchedulingTraceLogFile() {
  if (rename(kpBtSchedulingTraceLogPath, kpBtSchedulingTraceLastLogPath) != 0 &&
      errno != ENOENT) {
    LOG(ERROR) << __func__ << ": Unable to rename '"
               << kpBtSchedulingTraceLogPath << "' to '"
               << kpBtSchedulingTraceLastLogPath << "' : " << strerror(errno);
  }

  mode_t prevmask = umask(0);
  int logfile_fd =
      open(kpBtSchedulingTraceLogPath, O_WRONLY | O_CREAT | O_TRUNC,
           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  umask(prevmask);
  if (logfile_fd == INVALID_FD) {
    LOG(ERROR) << __func__ << ": Unable to open '" << kpBtSchedulingTraceLogPath
               << "' : " << strerror(errno);
  } else {
    BtSchedulingTraceCounter = 0;
  }
  return logfile_fd;
}

void DebugDump(int fd) {
  dprintf(fd, "\nBT Quality Report Events: \n");

  if (kpBqrEventQueue->Empty()) {
    dprintf(fd, "Event queue is empty.\n");
    return;
  }

  while (!kpBqrEventQueue->Empty()) {
    std::unique_ptr<BqrVseSubEvt> p_event(kpBqrEventQueue->Dequeue());

    bool warning = (p_event->bqr_link_quality_event_.rssi < kCriWarnRssi ||
                    p_event->bqr_link_quality_event_.unused_afh_channel_count >
                        kCriWarnUnusedCh);

    std::stringstream ss_timestamp;
    ss_timestamp << std::put_time(&p_event->tm_timestamp_, "%m-%d %H:%M:%S");

    dprintf(fd, "%c  %s %s\n", warning ? '*' : ' ', ss_timestamp.str().c_str(),
            p_event->ToString().c_str());
  }

  dprintf(fd, "\n");
}

}  // namespace bqr
}  // namespace bluetooth
