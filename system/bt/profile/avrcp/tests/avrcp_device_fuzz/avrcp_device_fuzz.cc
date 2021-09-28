#include <cstddef>
#include <cstdint>

#include "avrcp_packet.h"
#include "device.h"
#include "packet_test_helper.h"
#include "stack_config.h"

namespace bluetooth {
namespace avrcp {
class FakeMediaInterface : public MediaInterface {
 public:
  virtual void SendKeyEvent(uint8_t key, KeyState state) {}
  using SongInfoCallback = base::Callback<void(SongInfo)>;
  virtual void GetSongInfo(SongInfoCallback info_cb) {}
  using PlayStatusCallback = base::Callback<void(PlayStatus)>;
  virtual void GetPlayStatus(PlayStatusCallback status_cb) {}
  using NowPlayingCallback =
      base::Callback<void(std::string, std::vector<SongInfo>)>;
  virtual void GetNowPlayingList(NowPlayingCallback now_playing_cb) {}
  using MediaListCallback =
      base::Callback<void(uint16_t curr_player, std::vector<MediaPlayerInfo>)>;
  virtual void GetMediaPlayerList(MediaListCallback list_cb) {}
  using FolderItemsCallback = base::Callback<void(std::vector<ListItem>)>;
  virtual void GetFolderItems(uint16_t player_id, std::string media_id,
                              FolderItemsCallback folder_cb) {}
  using SetBrowsedPlayerCallback = base::Callback<void(
      bool success, std::string root_id, uint32_t num_items)>;
  virtual void SetBrowsedPlayer(uint16_t player_id,
                                SetBrowsedPlayerCallback browse_cb) {}
  virtual void PlayItem(uint16_t player_id, bool now_playing,
                        std::string media_id) {}
  virtual void SetActiveDevice(const RawAddress& address) {}
  virtual void RegisterUpdateCallback(MediaCallbacks* callback) {}
  virtual void UnregisterUpdateCallback(MediaCallbacks* callback) {}
};

class FakeVolumeInterface : public VolumeInterface {
 public:
  virtual void DeviceConnected(const RawAddress& bdaddr) {}
  virtual void DeviceConnected(const RawAddress& bdaddr, VolumeChangedCb cb) {}
  virtual void DeviceDisconnected(const RawAddress& bdaddr) {}
  virtual void SetVolume(int8_t volume) {}
};

class FakeA2dpInterface : public A2dpInterface {
 public:
  virtual RawAddress active_peer() { return RawAddress(); }
  virtual bool is_peer_in_silence_mode(const RawAddress& peer_address) {
    return false;
  }
};

bool get_pts_avrcp_test(void) { return false; }

const stack_config_t interface = {
    nullptr, get_pts_avrcp_test, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr};

void Callback(uint8_t, bool, std::unique_ptr<::bluetooth::PacketBuilder>) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  FakeMediaInterface fmi;
  FakeVolumeInterface fvi;
  FakeA2dpInterface fai;

  std::vector<uint8_t> Packet(Data, Data + Size);
  Device device(RawAddress::kAny, true,
                base::Bind([](uint8_t, bool,
                              std::unique_ptr<::bluetooth::PacketBuilder>) {}),
                0xFFFF, 0xFFFF);
  device.RegisterInterfaces(&fmi, &fai, &fvi);

  auto browse_request = TestPacketType<BrowsePacket>::Make(Packet);
  device.BrowseMessageReceived(1, browse_request);

  auto avrcp_request = TestPacketType<avrcp::Packet>::Make(Packet);
  device.MessageReceived(1, avrcp_request);
  return 0;
}
}  // namespace avrcp
}  // namespace bluetooth

const stack_config_t* stack_config_get_interface(void) {
  return &bluetooth::avrcp::interface;
}