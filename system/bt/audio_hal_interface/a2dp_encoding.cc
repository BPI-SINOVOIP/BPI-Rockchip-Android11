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

#include "a2dp_encoding.h"
#include "client_interface.h"
#include "codec_status.h"

#include "btif_a2dp_source.h"
#include "btif_av.h"
#include "btif_av_co.h"
#include "btif_hf.h"
#include "osi/include/properties.h"

namespace {

using ::bluetooth::audio::AudioCapabilities;
using ::bluetooth::audio::AudioConfiguration;
using ::bluetooth::audio::BitsPerSample;
using ::bluetooth::audio::BluetoothAudioCtrlAck;
using ::bluetooth::audio::ChannelMode;
using ::bluetooth::audio::PcmParameters;
using ::bluetooth::audio::SampleRate;
using ::bluetooth::audio::SessionType;

using ::bluetooth::audio::BluetoothAudioClientInterface;
using ::bluetooth::audio::codec::A2dpAacToHalConfig;
using ::bluetooth::audio::codec::A2dpAptxToHalConfig;
using ::bluetooth::audio::codec::A2dpCodecToHalBitsPerSample;
using ::bluetooth::audio::codec::A2dpCodecToHalChannelMode;
using ::bluetooth::audio::codec::A2dpCodecToHalSampleRate;
using ::bluetooth::audio::codec::A2dpLdacToHalConfig;
using ::bluetooth::audio::codec::A2dpSbcToHalConfig;
using ::bluetooth::audio::codec::CodecConfiguration;

BluetoothAudioCtrlAck a2dp_ack_to_bt_audio_ctrl_ack(tA2DP_CTRL_ACK ack);

// Provide call-in APIs for the Bluetooth Audio HAL
class A2dpTransport : public ::bluetooth::audio::IBluetoothTransportInstance {
 public:
  A2dpTransport(SessionType sessionType)
      : IBluetoothTransportInstance(sessionType, {}),
        total_bytes_read_(0),
        data_position_({}) {
    a2dp_pending_cmd_ = A2DP_CTRL_CMD_NONE;
    remote_delay_report_ = 0;
  }

  BluetoothAudioCtrlAck StartRequest() override {
    // Check if a previous request is not finished
    if (a2dp_pending_cmd_ == A2DP_CTRL_CMD_START) {
      LOG(INFO) << __func__ << ": A2DP_CTRL_CMD_START in progress";
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_PENDING);
    } else if (a2dp_pending_cmd_ != A2DP_CTRL_CMD_NONE) {
      LOG(WARNING) << __func__ << ": busy in pending_cmd=" << a2dp_pending_cmd_;
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_FAILURE);
    }

    // Don't send START request to stack while we are in a call
    if (!bluetooth::headset::IsCallIdle()) {
      LOG(ERROR) << __func__ << ": call state is busy";
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_INCALL_FAILURE);
    }

    if (btif_av_stream_started_ready()) {
      // Already started, ACK back immediately.
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_SUCCESS);
    }
    if (btif_av_stream_ready()) {
      /*
       * Post start event and wait for audio path to open.
       * If we are the source, the ACK will be sent after the start
       * procedure is completed, othewise send it now.
       */
      a2dp_pending_cmd_ = A2DP_CTRL_CMD_START;
      btif_av_stream_start();
      if (btif_av_get_peer_sep() != AVDT_TSEP_SRC) {
        LOG(INFO) << __func__ << ": accepted";
        return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_PENDING);
      }
      a2dp_pending_cmd_ = A2DP_CTRL_CMD_NONE;
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_SUCCESS);
    }
    LOG(ERROR) << __func__ << ": AV stream is not ready to start";
    return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_FAILURE);
  }

  BluetoothAudioCtrlAck SuspendRequest() override {
    // Previous request is not finished
    if (a2dp_pending_cmd_ == A2DP_CTRL_CMD_SUSPEND) {
      LOG(INFO) << __func__ << ": A2DP_CTRL_CMD_SUSPEND in progress";
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_PENDING);
    } else if (a2dp_pending_cmd_ != A2DP_CTRL_CMD_NONE) {
      LOG(WARNING) << __func__ << ": busy in pending_cmd=" << a2dp_pending_cmd_;
      return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_FAILURE);
    }
    // Local suspend
    if (btif_av_stream_started_ready()) {
      LOG(INFO) << __func__ << ": accepted";
      a2dp_pending_cmd_ = A2DP_CTRL_CMD_SUSPEND;
      btif_av_stream_suspend();
      return BluetoothAudioCtrlAck::PENDING;
    }
    /* If we are not in started state, just ack back ok and let
     * audioflinger close the channel. This can happen if we are
     * remotely suspended, clear REMOTE SUSPEND flag.
     */
    btif_av_clear_remote_suspend_flag();
    return a2dp_ack_to_bt_audio_ctrl_ack(A2DP_CTRL_ACK_SUCCESS);
  }

  void StopRequest() override {
    if (btif_av_get_peer_sep() == AVDT_TSEP_SNK &&
        !btif_av_stream_started_ready()) {
      return;
    }
    LOG(INFO) << __func__ << ": handling";
    a2dp_pending_cmd_ = A2DP_CTRL_CMD_STOP;
    btif_av_stream_stop(RawAddress::kEmpty);
  }

  bool GetPresentationPosition(uint64_t* remote_delay_report_ns,
                               uint64_t* total_bytes_read,
                               timespec* data_position) override {
    *remote_delay_report_ns = remote_delay_report_ * 100000u;
    *total_bytes_read = total_bytes_read_;
    *data_position = data_position_;
    VLOG(2) << __func__ << ": delay=" << remote_delay_report_
            << "/10ms, data=" << total_bytes_read_
            << " byte(s), timestamp=" << data_position_.tv_sec << "."
            << data_position_.tv_nsec << "s";
    return true;
  }

  void MetadataChanged(const source_metadata_t& source_metadata) override {
    auto track_count = source_metadata.track_count;
    auto tracks = source_metadata.tracks;
    VLOG(1) << __func__ << ": " << track_count << " track(s) received";
    while (track_count) {
      VLOG(2) << __func__ << ": usage=" << tracks->usage
              << ", content_type=" << tracks->content_type
              << ", gain=" << tracks->gain;
      --track_count;
      ++tracks;
    }
  }

  tA2DP_CTRL_CMD GetPendingCmd() const { return a2dp_pending_cmd_; }

  void ResetPendingCmd() { a2dp_pending_cmd_ = A2DP_CTRL_CMD_NONE; }

  void ResetPresentationPosition() override {
    remote_delay_report_ = 0;
    total_bytes_read_ = 0;
    data_position_ = {};
  }

  void LogBytesRead(size_t bytes_read) override {
    if (bytes_read != 0) {
      total_bytes_read_ += bytes_read;
      clock_gettime(CLOCK_MONOTONIC, &data_position_);
    }
  }

  // delay reports from AVDTP is based on 1/10 ms (100us)
  void SetRemoteDelay(uint16_t delay_report) {
    remote_delay_report_ = delay_report;
  }

 private:
  static tA2DP_CTRL_CMD a2dp_pending_cmd_;
  static uint16_t remote_delay_report_;
  uint64_t total_bytes_read_;
  timespec data_position_;
};

tA2DP_CTRL_CMD A2dpTransport::a2dp_pending_cmd_ = A2DP_CTRL_CMD_NONE;
uint16_t A2dpTransport::remote_delay_report_ = 0;

// Common interface to call-out into Bluetooth Audio HAL
BluetoothAudioClientInterface* software_hal_interface = nullptr;
BluetoothAudioClientInterface* offloading_hal_interface = nullptr;
BluetoothAudioClientInterface* active_hal_interface = nullptr;

// Save the value if the remote reports its delay before this interface is
// initialized
uint16_t remote_delay = 0;

bool btaudio_a2dp_disabled = false;
bool is_configured = false;

BluetoothAudioCtrlAck a2dp_ack_to_bt_audio_ctrl_ack(tA2DP_CTRL_ACK ack) {
  switch (ack) {
    case A2DP_CTRL_ACK_SUCCESS:
      return BluetoothAudioCtrlAck::SUCCESS_FINISHED;
    case A2DP_CTRL_ACK_PENDING:
      return BluetoothAudioCtrlAck::PENDING;
    case A2DP_CTRL_ACK_INCALL_FAILURE:
      return BluetoothAudioCtrlAck::FAILURE_BUSY;
    case A2DP_CTRL_ACK_DISCONNECT_IN_PROGRESS:
      return BluetoothAudioCtrlAck::FAILURE_DISCONNECTING;
    case A2DP_CTRL_ACK_UNSUPPORTED: /* Offloading but resource failure */
      return BluetoothAudioCtrlAck::FAILURE_UNSUPPORTED;
    case A2DP_CTRL_ACK_FAILURE:
      return BluetoothAudioCtrlAck::FAILURE;
    default:
      return BluetoothAudioCtrlAck::FAILURE;
  }
}

bool a2dp_get_selected_hal_codec_config(CodecConfiguration* codec_config) {
  A2dpCodecConfig* a2dp_config = bta_av_get_a2dp_current_codec();
  if (a2dp_config == nullptr) {
    LOG(WARNING) << __func__ << ": failure to get A2DP codec config";
    *codec_config = ::bluetooth::audio::codec::kInvalidCodecConfiguration;
    return false;
  }
  btav_a2dp_codec_config_t current_codec = a2dp_config->getCodecConfig();
  switch (current_codec.codec_type) {
    case BTAV_A2DP_CODEC_INDEX_SOURCE_SBC:
      [[fallthrough]];
    case BTAV_A2DP_CODEC_INDEX_SINK_SBC: {
      if (!A2dpSbcToHalConfig(codec_config, a2dp_config)) {
        return false;
      }
      break;
    }
    case BTAV_A2DP_CODEC_INDEX_SOURCE_AAC:
      [[fallthrough]];
    case BTAV_A2DP_CODEC_INDEX_SINK_AAC: {
      if (!A2dpAacToHalConfig(codec_config, a2dp_config)) {
        return false;
      }
      break;
    }
    case BTAV_A2DP_CODEC_INDEX_SOURCE_APTX:
      [[fallthrough]];
    case BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_HD: {
      if (!A2dpAptxToHalConfig(codec_config, a2dp_config)) {
        return false;
      }
      break;
    }
    case BTAV_A2DP_CODEC_INDEX_SOURCE_LDAC: {
      if (!A2dpLdacToHalConfig(codec_config, a2dp_config)) {
        return false;
      }
      break;
    }
    case BTAV_A2DP_CODEC_INDEX_MAX:
      [[fallthrough]];
    default:
      LOG(ERROR) << __func__
                 << ": Unknown codec_type=" << current_codec.codec_type;
      *codec_config = ::bluetooth::audio::codec::kInvalidCodecConfiguration;
      return false;
  }
  codec_config->encodedAudioBitrate = a2dp_config->getTrackBitRate();
  // Obtain the MTU
  RawAddress peer_addr = btif_av_source_active_peer();
  tA2DP_ENCODER_INIT_PEER_PARAMS peer_param;
  bta_av_co_get_peer_params(peer_addr, &peer_param);
  int effectiveMtu = a2dp_config->getEffectiveMtu();
  if (effectiveMtu > 0 && effectiveMtu < peer_param.peer_mtu) {
    codec_config->peerMtu = effectiveMtu;
  } else {
    codec_config->peerMtu = peer_param.peer_mtu;
  }
  LOG(INFO) << __func__ << ": CodecConfiguration=" << toString(*codec_config);
  return true;
}

bool a2dp_get_selected_hal_pcm_config(PcmParameters* pcm_config) {
  if (pcm_config == nullptr) return false;
  A2dpCodecConfig* a2dp_codec_configs = bta_av_get_a2dp_current_codec();
  if (a2dp_codec_configs == nullptr) {
    LOG(WARNING) << __func__ << ": failure to get A2DP codec config";
    *pcm_config = ::bluetooth::audio::BluetoothAudioClientInterface::
        kInvalidPcmConfiguration;
    return false;
  }

  btav_a2dp_codec_config_t current_codec = a2dp_codec_configs->getCodecConfig();
  pcm_config->sampleRate = A2dpCodecToHalSampleRate(current_codec);
  pcm_config->bitsPerSample = A2dpCodecToHalBitsPerSample(current_codec);
  pcm_config->channelMode = A2dpCodecToHalChannelMode(current_codec);
  return (pcm_config->sampleRate != SampleRate::RATE_UNKNOWN &&
          pcm_config->bitsPerSample != BitsPerSample::BITS_UNKNOWN &&
          pcm_config->channelMode != ChannelMode::UNKNOWN);
}

// Checking if new bluetooth_audio is supported
bool is_hal_2_0_force_disabled() {
  if (!is_configured) {
    btaudio_a2dp_disabled = osi_property_get_bool(BLUETOOTH_AUDIO_HAL_PROP_DISABLED, false);
    is_configured = true;
  }
  return btaudio_a2dp_disabled;
}
}  // namespace

namespace bluetooth {
namespace audio {
namespace a2dp {

bool update_codec_offloading_capabilities(
    const std::vector<btav_a2dp_codec_config_t>& framework_preference) {
  return ::bluetooth::audio::codec::UpdateOffloadingCapabilities(
      framework_preference);
}

// Checking if new bluetooth_audio is enabled
bool is_hal_2_0_enabled() { return active_hal_interface != nullptr; }

// Check if new bluetooth_audio is running with offloading encoders
bool is_hal_2_0_offloading() {
  if (!is_hal_2_0_enabled()) {
    return false;
  }
  return active_hal_interface->GetTransportInstance()->GetSessionType() ==
         SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH;
}

// Initialize BluetoothAudio HAL: openProvider
bool init(bluetooth::common::MessageLoopThread* message_loop) {
  LOG(INFO) << __func__;

  if (is_hal_2_0_force_disabled()) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is disabled";
    return false;
  }

  auto a2dp_sink =
      new A2dpTransport(SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH);
  software_hal_interface = new bluetooth::audio::BluetoothAudioClientInterface(
      a2dp_sink, message_loop);
  if (!software_hal_interface->IsValid()) {
    LOG(WARNING) << __func__ << ": BluetoothAudio HAL for A2DP is invalid?!";
    delete software_hal_interface;
    software_hal_interface = nullptr;
    delete a2dp_sink;
    return false;
  }

  if (btif_av_is_a2dp_offload_enabled()) {
    a2dp_sink = new A2dpTransport(SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH);
    offloading_hal_interface =
        new bluetooth::audio::BluetoothAudioClientInterface(a2dp_sink,
                                                            message_loop);
    if (!offloading_hal_interface->IsValid()) {
      LOG(FATAL) << __func__
                 << ": BluetoothAudio HAL for A2DP offloading is invalid?!";
      delete offloading_hal_interface;
      offloading_hal_interface = nullptr;
      delete a2dp_sink;
      a2dp_sink = static_cast<A2dpTransport*>(
          software_hal_interface->GetTransportInstance());
      delete software_hal_interface;
      software_hal_interface = nullptr;
      delete a2dp_sink;
      return false;
    }
  }

  active_hal_interface =
      (offloading_hal_interface != nullptr ? offloading_hal_interface
                                           : software_hal_interface);

  if (remote_delay != 0) {
    LOG(INFO) << __func__ << ": restore DELAY "
              << static_cast<float>(remote_delay / 10.0) << " ms";
    static_cast<A2dpTransport*>(active_hal_interface->GetTransportInstance())
        ->SetRemoteDelay(remote_delay);
    remote_delay = 0;
  }
  return true;
}

// Clean up BluetoothAudio HAL
void cleanup() {
  if (!is_hal_2_0_enabled()) return;
  end_session();

  auto a2dp_sink = active_hal_interface->GetTransportInstance();
  static_cast<A2dpTransport*>(a2dp_sink)->ResetPendingCmd();
  static_cast<A2dpTransport*>(a2dp_sink)->ResetPresentationPosition();
  active_hal_interface = nullptr;

  a2dp_sink = software_hal_interface->GetTransportInstance();
  delete software_hal_interface;
  software_hal_interface = nullptr;
  delete a2dp_sink;
  if (offloading_hal_interface != nullptr) {
    a2dp_sink = offloading_hal_interface->GetTransportInstance();
    delete offloading_hal_interface;
    offloading_hal_interface = nullptr;
    delete a2dp_sink;
  }

  remote_delay = 0;
}

// Set up the codec into BluetoothAudio HAL
bool setup_codec() {
  if (!is_hal_2_0_enabled()) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is not enabled";
    return false;
  }
  CodecConfiguration codec_config{};
  if (!a2dp_get_selected_hal_codec_config(&codec_config)) {
    LOG(ERROR) << __func__ << ": Failed to get CodecConfiguration";
    return false;
  }
  bool should_codec_offloading =
      bluetooth::audio::codec::IsCodecOffloadingEnabled(codec_config);
  if (should_codec_offloading && !is_hal_2_0_offloading()) {
    LOG(WARNING) << __func__ << ": Switching BluetoothAudio HAL to Hardware";
    end_session();
    active_hal_interface = offloading_hal_interface;
  } else if (!should_codec_offloading && is_hal_2_0_offloading()) {
    LOG(WARNING) << __func__ << ": Switching BluetoothAudio HAL to Software";
    end_session();
    active_hal_interface = software_hal_interface;
  }

  AudioConfiguration audio_config{};
  if (active_hal_interface->GetTransportInstance()->GetSessionType() ==
      SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH) {
    audio_config.codecConfig(codec_config);
  } else {
    PcmParameters pcm_config{};
    if (!a2dp_get_selected_hal_pcm_config(&pcm_config)) {
      LOG(ERROR) << __func__ << ": Failed to get PcmConfiguration";
      return false;
    }
    audio_config.pcmConfig(pcm_config);
  }
  return active_hal_interface->UpdateAudioConfig(audio_config);
}

void start_session() {
  if (!is_hal_2_0_enabled()) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is not enabled";
    return;
  }
  active_hal_interface->StartSession();
}

void end_session() {
  if (!is_hal_2_0_enabled()) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is not enabled";
    return;
  }
  active_hal_interface->EndSession();
  static_cast<A2dpTransport*>(active_hal_interface->GetTransportInstance())
      ->ResetPresentationPosition();
}

void ack_stream_started(const tA2DP_CTRL_ACK& ack) {
  auto ctrl_ack = a2dp_ack_to_bt_audio_ctrl_ack(ack);
  LOG(INFO) << __func__ << ": result=" << ctrl_ack;
  auto a2dp_sink =
      static_cast<A2dpTransport*>(active_hal_interface->GetTransportInstance());
  auto pending_cmd = a2dp_sink->GetPendingCmd();
  if (pending_cmd == A2DP_CTRL_CMD_START) {
    active_hal_interface->StreamStarted(ctrl_ack);
  } else {
    LOG(WARNING) << __func__ << ": pending=" << pending_cmd
                 << " ignore result=" << ctrl_ack;
    return;
  }
  if (ctrl_ack != bluetooth::audio::BluetoothAudioCtrlAck::PENDING) {
    a2dp_sink->ResetPendingCmd();
  }
}

void ack_stream_suspended(const tA2DP_CTRL_ACK& ack) {
  auto ctrl_ack = a2dp_ack_to_bt_audio_ctrl_ack(ack);
  LOG(INFO) << __func__ << ": result=" << ctrl_ack;
  auto a2dp_sink =
      static_cast<A2dpTransport*>(active_hal_interface->GetTransportInstance());
  auto pending_cmd = a2dp_sink->GetPendingCmd();
  if (pending_cmd == A2DP_CTRL_CMD_SUSPEND) {
    active_hal_interface->StreamSuspended(ctrl_ack);
  } else if (pending_cmd == A2DP_CTRL_CMD_STOP) {
    LOG(INFO) << __func__ << ": A2DP_CTRL_CMD_STOP result=" << ctrl_ack;
  } else {
    LOG(WARNING) << __func__ << ": pending=" << pending_cmd
                 << " ignore result=" << ctrl_ack;
    return;
  }
  if (ctrl_ack != bluetooth::audio::BluetoothAudioCtrlAck::PENDING) {
    a2dp_sink->ResetPendingCmd();
  }
}

// Read from the FMQ of BluetoothAudio HAL
size_t read(uint8_t* p_buf, uint32_t len) {
  if (!is_hal_2_0_enabled()) {
    LOG(ERROR) << __func__ << ": BluetoothAudio HAL is not enabled";
    return 0;
  } else if (is_hal_2_0_offloading()) {
    LOG(ERROR) << __func__ << ": session_type="
               << toString(active_hal_interface->GetTransportInstance()
                               ->GetSessionType())
               << " is not A2DP_SOFTWARE_ENCODING_DATAPATH";
    return 0;
  }
  return active_hal_interface->ReadAudioData(p_buf, len);
}

// Update A2DP delay report to BluetoothAudio HAL
void set_remote_delay(uint16_t delay_report) {
  if (!is_hal_2_0_enabled()) {
    LOG(INFO) << __func__ << ":  not ready for DelayReport "
              << static_cast<float>(delay_report / 10.0) << " ms";
    remote_delay = delay_report;
    return;
  }
  VLOG(1) << __func__ << ": DELAY " << static_cast<float>(delay_report / 10.0)
          << " ms";
  static_cast<A2dpTransport*>(active_hal_interface->GetTransportInstance())
      ->SetRemoteDelay(delay_report);
}

}  // namespace a2dp
}  // namespace audio
}  // namespace bluetooth
