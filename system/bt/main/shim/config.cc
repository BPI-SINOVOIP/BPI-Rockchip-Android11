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

#define LOG_TAG "bt_shim_storage"

#include <cstdint>
#include <cstring>
#include <future>
#include <memory>

#include "btif/include/btif_config.h"
#include "main/shim/config.h"
#include "main/shim/entry.h"

#include "storage/legacy.h"

using ::bluetooth::shim::GetStorage;

namespace bluetooth {
namespace shim {

std::string checksum_read(const char* filename) {
  CHECK(filename != nullptr);

  std::promise<std::string> promise;
  auto future = promise.get_future();
  GetStorage()->ChecksumRead(
      std::string(filename),
      common::BindOnce(
          [](std::promise<std::string>* promise, std::string,
             std::string hash_value) { promise->set_value(hash_value); },
          &promise),
      bluetooth::shim::GetGdShimHandler());
  return future.get();
}

bool checksum_save(const std::string& checksum, const std::string& filename) {
  std::promise<bool> promise;
  auto future = promise.get_future();
  GetStorage()->ChecksumWrite(
      filename, checksum,
      common::BindOnce([](std::promise<bool>* promise, std::string,
                          bool success) { promise->set_value(success); },
                       &promise),
      bluetooth::shim::GetGdShimHandler());
  return future.get();
}

std::unique_ptr<config_t> config_new(const char* filename) {
  CHECK(filename != nullptr);

  std::promise<std::unique_ptr<config_t>> promise;
  auto future = promise.get_future();
  GetStorage()->ConfigRead(
      std::string(filename),
      common::BindOnce(
          [](std::promise<std::unique_ptr<config_t>>* promise, std::string,
             std::unique_ptr<config_t> config) {
            promise->set_value(std::move(config));
          },
          &promise),
      bluetooth::shim::GetGdShimHandler());
  return future.get();
}

bool config_save(const config_t& config, const std::string& filename) {
  std::promise<bool> promise;
  auto future = promise.get_future();
  GetStorage()->ConfigWrite(
      filename, config,
      common::BindOnce([](std::promise<bool>* promise, std::string,
                          bool success) { promise->set_value(success); },
                       &promise),
      bluetooth::shim::GetGdShimHandler());
  return future.get();
}

}  // namespace shim
}  // namespace bluetooth

namespace {
const storage_config_t interface = {
    bluetooth::shim::checksum_read,
    bluetooth::shim::checksum_save,
    config_get_bool,
    config_get_int,
    config_get_string,
    config_get_uint64,
    config_has_key,
    config_has_section,
    bluetooth::shim::config_new,
    config_new_clone,
    config_new_empty,
    config_remove_key,
    config_remove_section,
    bluetooth::shim::config_save,
    config_set_bool,
    config_set_int,
    config_set_string,
    config_set_uint64,
};
}

const storage_config_t* bluetooth::shim::storage_config_get_interface() {
  return &interface;
}
