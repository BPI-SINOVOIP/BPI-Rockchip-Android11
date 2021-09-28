/*
 * Copyright 2020 The Android Open Source Project
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
#define LOG_TAG "bt_storage"

#include "storage/legacy.h"
#include "storage/legacy_osi_config.h"

namespace bluetooth {
namespace storage {

struct LegacyModule::impl {
  void config_read(const std::string filename, LegacyReadConfigCallback callback, os::Handler* handler) {
    std::unique_ptr<config_t> config = legacy::osi::config::config_new(filename.c_str());
    if (config && !legacy::osi::config::config_has_section(*config, "Adapter")) {
      LOG_ERROR("Config is missing adapter section");
      config = nullptr;
    }
    if (!config) {
      config = legacy::osi::config::config_new_empty();
    }
    handler->Post(common::BindOnce(std::move(callback), filename, std::move(config)));
  }

  void config_write(const std::string filename, const config_t config, LegacyWriteConfigCallback callback,
                    os::Handler* handler) {
    handler->Post(common::BindOnce(std::move(callback), filename, legacy::osi::config::config_save(config, filename)));
  }

  void checksum_read(const std::string filename, LegacyReadChecksumCallback callback, os::Handler* handler) {
    handler->Post(
        common::BindOnce(std::move(callback), filename, legacy::osi::config::checksum_read(filename.c_str())));
  }

  void checksum_write(const std::string filename, const std::string checksum, LegacyWriteChecksumCallback callback,
                      os::Handler* handler) {
    handler->Post(
        common::BindOnce(std::move(callback), filename, legacy::osi::config::checksum_save(checksum, filename)));
  }

  void Start();
  void Stop();

  impl(const LegacyModule& name_module);

 private:
  const LegacyModule& module_;
  os::Handler* handler_;
};

const ModuleFactory storage::LegacyModule::Factory = ModuleFactory([]() { return new storage::LegacyModule(); });

storage::LegacyModule::impl::impl(const storage::LegacyModule& module) : module_(module) {}

void storage::LegacyModule::impl::Start() {
  handler_ = module_.GetHandler();
}

void storage::LegacyModule::impl::Stop() {}

void storage::LegacyModule::ConfigRead(const std::string filename, LegacyReadConfigCallback callback,
                                       os::Handler* handler) {
  GetHandler()->Post(common::BindOnce(&LegacyModule::impl::config_read, common::Unretained(pimpl_.get()), filename,
                                      std::move(callback), handler));
}

void storage::LegacyModule::ConfigWrite(const std::string filename, const config_t& config,
                                        LegacyWriteConfigCallback callback, os::Handler* handler) {
  GetHandler()->Post(common::BindOnce(&LegacyModule::impl::config_write, common::Unretained(pimpl_.get()), filename,
                                      config, std::move(callback), handler));
}

void storage::LegacyModule::ChecksumRead(const std::string filename, LegacyReadChecksumCallback callback,
                                         os::Handler* handler) {
  GetHandler()->Post(common::BindOnce(&LegacyModule::impl::checksum_read, common::Unretained(pimpl_.get()), filename,
                                      std::move(callback), handler));
}

void storage::LegacyModule::ChecksumWrite(const std::string filename, const std::string checksum,
                                          LegacyWriteChecksumCallback callback, os::Handler* handler) {
  GetHandler()->Post(common::BindOnce(&LegacyModule::impl::checksum_write, common::Unretained(pimpl_.get()), filename,
                                      checksum, std::move(callback), handler));
}

/**
 * General API here
 */
storage::LegacyModule::LegacyModule() : pimpl_(std::make_unique<impl>(*this)) {}

storage::LegacyModule::~LegacyModule() {
  pimpl_.reset();
}

/**
 * Module methods here
 */
void storage::LegacyModule::ListDependencies(ModuleList* list) {}

void storage::LegacyModule::Start() {
  pimpl_->Start();
}

void storage::LegacyModule::Stop() {
  pimpl_->Stop();
}

}  // namespace storage
}  // namespace bluetooth
