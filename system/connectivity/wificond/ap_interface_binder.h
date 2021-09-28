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

#ifndef WIFICOND_AP_INTERFACE_BINDER_H_
#define WIFICOND_AP_INTERFACE_BINDER_H_

#include <android-base/macros.h>

#include "wificond/net/netlink_manager.h"

#include "android/net/wifi/nl80211/BnApInterface.h"
#include "android/net/wifi/nl80211/IApInterfaceEventCallback.h"

using android::net::wifi::nl80211::IApInterfaceEventCallback;
using android::net::wifi::nl80211::NativeWifiClient;

namespace android {
namespace wificond {

class ApInterfaceImpl;

class ApInterfaceBinder : public android::net::wifi::nl80211::BnApInterface {
 public:
  explicit ApInterfaceBinder(ApInterfaceImpl* impl);
  ~ApInterfaceBinder() override;

  // Called by |impl_| on its destruction.
  // This informs the binder proxy that no future manipulations of |impl_|
  // by remote processes are possible.
  void NotifyImplDead() { impl_ = nullptr; }

  // Called by |impl_| every time the access point's connected clients change.
  void NotifyConnectedClientsChanged(const NativeWifiClient client, bool isConnected);

  // Called by |impl_| on every channel switch event.
  void NotifySoftApChannelSwitched(int frequency,
                                   ChannelBandwidth channel_bandwidth);

  binder::Status registerCallback(
      const sp<IApInterfaceEventCallback>& callback,
      bool* out_success) override;
  binder::Status getInterfaceName(std::string* out_name) override;

 private:
  ApInterfaceImpl* impl_;

  android::sp<IApInterfaceEventCallback>
      ap_interface_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(ApInterfaceBinder);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_AP_INTERFACE_BINDER_H_
