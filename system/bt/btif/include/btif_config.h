/******************************************************************************
 *
 *  Copyright 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "bt_types.h"

#include <list>
#include <string>
#include "osi/include/config.h"

static const char BTIF_CONFIG_MODULE[] = "btif_config_module";

static const std::string BT_CONFIG_KEY_SDP_DI_MANUFACTURER =
    "SdpDiManufacturer";
static const std::string BT_CONFIG_KEY_SDP_DI_MODEL = "SdpDiModel";
static const std::string BT_CONFIG_KEY_SDP_DI_HW_VERSION =
    "SdpDiHardwareVersion";
static const std::string BT_CONFIG_KEY_SDP_DI_VENDOR_ID_SRC =
    "SdpDiVendorIdSource";

static const std::string BT_CONFIG_KEY_REMOTE_VER_MFCT = "Manufacturer";
static const std::string BT_CONFIG_KEY_REMOTE_VER_VER = "LmpVer";
static const std::string BT_CONFIG_KEY_REMOTE_VER_SUBVER = "LmpSubVer";

bool btif_config_has_section(const char* section);
bool btif_config_exist(const std::string& section, const std::string& key);
bool btif_config_get_int(const std::string& section, const std::string& key,
                         int* value);
bool btif_config_set_int(const std::string& section, const std::string& key,
                         int value);
bool btif_config_get_uint64(const std::string& section, const std::string& key,
                            uint64_t* value);
bool btif_config_set_uint64(const std::string& section, const std::string& key,
                            uint64_t value);
bool btif_config_get_str(const std::string& section, const std::string& key,
                         char* value, int* size_bytes);
bool btif_config_set_str(const std::string& section, const std::string& key,
                         const std::string& value);
bool btif_config_get_bin(const std::string& section, const std::string& key,
                         uint8_t* value, size_t* length);
bool btif_config_set_bin(const std::string& section, const std::string& key,
                         const uint8_t* value, size_t length);
bool btif_config_remove(const std::string& section, const std::string& key);

size_t btif_config_get_bin_length(const std::string& section,
                                  const std::string& key);

const std::list<section_t>& btif_config_sections();

void btif_config_save(void);
void btif_config_flush(void);
bool btif_config_clear(void);

// TODO(zachoverflow): Eww...we need to move these out. These are peer specific,
// not config general.
bool btif_get_address_type(const RawAddress& bd_addr, int* p_addr_type);
bool btif_get_device_type(const RawAddress& bd_addr, int* p_device_type);

void btif_debug_config_dump(int fd);

typedef struct {
  std::string (*checksum_read)(const char* filename);
  bool (*checksum_save)(const std::string& checksum,
                        const std::string& filename);
  bool (*config_get_bool)(const config_t& config, const std::string& section,
                          const std::string& key, bool def_value);
  int (*config_get_int)(const config_t& config, const std::string& section,
                        const std::string& key, int def_value);
  const std::string* (*config_get_string)(const config_t& config,
                                          const std::string& section,
                                          const std::string& key,
                                          const std::string* def_value);
  uint64_t (*config_get_uint64)(const config_t& config,
                                const std::string& section,
                                const std::string& key, uint64_t def_value);
  bool (*config_has_key)(const config_t& config, const std::string& section,
                         const std::string& key);
  bool (*config_has_section)(const config_t& config,
                             const std::string& section);
  std::unique_ptr<config_t> (*config_new)(const char* filename);
  std::unique_ptr<config_t> (*config_new_clone)(const config_t& src);
  std::unique_ptr<config_t> (*config_new_empty)(void);
  bool (*config_remove_key)(config_t* config, const std::string& section,
                            const std::string& key);
  bool (*config_remove_section)(config_t* config, const std::string& section);
  bool (*config_save)(const config_t& config, const std::string& filename);
  void (*config_set_bool)(config_t* config, const std::string& section,
                          const std::string& key, bool value);
  void (*config_set_int)(config_t* config, const std::string& section,
                         const std::string& key, int value);
  void (*config_set_string)(config_t* config, const std::string& section,
                            const std::string& key, const std::string& value);
  void (*config_set_uint64)(config_t* config, const std::string& section,
                            const std::string& key, uint64_t value);
} storage_config_t;
