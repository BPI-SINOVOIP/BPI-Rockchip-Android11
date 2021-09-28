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
#pragma once

#include <memory>

#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/fixed_channel_manager.h"
#include "module.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

class L2capClassicModule : public bluetooth::Module {
 public:
  L2capClassicModule();
  virtual ~L2capClassicModule();

  /**
   * Get the api to the classic fixed channel l2cap module
   */
  virtual std::unique_ptr<FixedChannelManager> GetFixedChannelManager();

  /**
   * Get the api to the classic dynamic channel l2cap module
   */
  virtual std::unique_ptr<DynamicChannelManager> GetDynamicChannelManager();

  static const ModuleFactory Factory;

 protected:
  void ListDependencies(ModuleList* list) override;

  void Start() override;

  void Stop() override;

  std::string ToString() const override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;
  DISALLOW_COPY_AND_ASSIGN(L2capClassicModule);
};

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
