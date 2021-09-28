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

#define LOG_TAG "bt_btif_config"

#include "btif_config.h"

#include <base/logging.h>
#include <ctype.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <private/android_filesystem_config.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include <btif_keystore.h>
#include "bt_types.h"
#include "btcore/include/module.h"
#include "btif_api.h"
#include "btif_common.h"
#include "btif_config_cache.h"
#include "btif_config_transcode.h"
#include "btif_util.h"
#include "common/address_obfuscator.h"
#include "common/metric_id_allocator.h"
#include "main/shim/config.h"
#include "main/shim/shim.h"
#include "osi/include/alarm.h"
#include "osi/include/allocator.h"
#include "osi/include/compat.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/properties.h"
#include "raw_address.h"

#define BT_CONFIG_SOURCE_TAG_NUM 1010001
#define TEMPORARY_SECTION_CAPACITY 10000

#define INFO_SECTION "Info"
#define FILE_TIMESTAMP "TimeCreated"
#define FILE_SOURCE "FileSource"
#define TIME_STRING_LENGTH sizeof("YYYY-MM-DD HH:MM:SS")
#define DISABLED "disabled"
static const char* TIME_STRING_FORMAT = "%Y-%m-%d %H:%M:%S";

#define BT_CONFIG_METRICS_SECTION "Metrics"
#define BT_CONFIG_METRICS_SALT_256BIT "Salt256Bit"
#define BT_CONFIG_METRICS_ID_KEY "MetricsId"

using bluetooth::bluetooth_keystore::BluetoothKeystoreInterface;
using bluetooth::common::AddressObfuscator;
using bluetooth::common::MetricIdAllocator;

// TODO(armansito): Find a better way than searching by a hardcoded path.
#if defined(OS_GENERIC)
static const char* CONFIG_FILE_PATH = "bt_config.conf";
static const char* CONFIG_BACKUP_PATH = "bt_config.bak";
static const char* CONFIG_LEGACY_FILE_PATH = "bt_config.xml";
#else   // !defined(OS_GENERIC)
static const char* CONFIG_FILE_PATH = "/data/misc/bluedroid/bt_config.conf";
static const char* CONFIG_BACKUP_PATH = "/data/misc/bluedroid/bt_config.bak";
static const char* CONFIG_LEGACY_FILE_PATH =
    "/data/misc/bluedroid/bt_config.xml";
#endif  // defined(OS_GENERIC)
static const uint64_t CONFIG_SETTLE_PERIOD_MS = 3000;

static void timer_config_save_cb(void* data);
static void btif_config_write(uint16_t event, char* p_param);
static bool is_factory_reset(void);
static void delete_config_files(void);
static std::unique_ptr<config_t> btif_config_open(const char* filename);

// Key attestation
static bool config_checksum_pass(int check_bit) {
  return ((get_niap_config_compare_result() & check_bit) == check_bit);
}
static bool btif_is_niap_mode() {
  return getuid() == AID_BLUETOOTH && is_niap_mode();
}
static bool btif_in_encrypt_key_name_list(std::string key);

static const int CONFIG_FILE_COMPARE_PASS = 1;
static const int CONFIG_BACKUP_COMPARE_PASS = 2;
static const std::string ENCRYPTED_STR = "encrypted";
static const std::string CONFIG_FILE_PREFIX = "bt_config-origin";
static const std::string CONFIG_FILE_HASH = "hash";
static const int ENCRYPT_KEY_NAME_LIST_SIZE = 7;
static const std::string encrypt_key_name_list[] = {
    "LinkKey",      "LE_KEY_PENC", "LE_KEY_PID",  "LE_KEY_LID",
    "LE_KEY_PCSRK", "LE_KEY_LENC", "LE_KEY_LCSRK"};

static enum ConfigSource {
  NOT_LOADED,
  ORIGINAL,
  BACKUP,
  LEGACY,
  NEW_FILE,
  RESET
} btif_config_source = NOT_LOADED;

static char btif_config_time_created[TIME_STRING_LENGTH];

static const storage_config_t interface = {
    checksum_read,         checksum_save,      config_get_bool,
    config_get_int,        config_get_string,  config_get_uint64,
    config_has_key,        config_has_section, config_new,
    config_new_clone,      config_new_empty,   config_remove_key,
    config_remove_section, config_save,        config_set_bool,
    config_set_int,        config_set_string,  config_set_uint64,
};

static const storage_config_t* storage_config_get_interface() {
  if (bluetooth::shim::is_gd_stack_started_up()) {
    return bluetooth::shim::storage_config_get_interface();
  } else {
    return &interface;
  }
}

static BluetoothKeystoreInterface* get_bluetooth_keystore_interface() {
  return bluetooth::bluetooth_keystore::getBluetoothKeystoreInterface();
}

// TODO(zachoverflow): Move these two functions out, because they are too
// specific for this file
// {grumpy-cat/no, monty-python/you-make-me-sad}
bool btif_get_device_type(const RawAddress& bda, int* p_device_type) {
  if (p_device_type == NULL) return false;

  std::string addrstr = bda.ToString();
  const char* bd_addr_str = addrstr.c_str();

  if (!btif_config_get_int(bd_addr_str, "DevType", p_device_type)) return false;

  LOG_DEBUG(LOG_TAG, "%s: Device [%s] type %d", __func__, bd_addr_str,
            *p_device_type);
  return true;
}

bool btif_get_address_type(const RawAddress& bda, int* p_addr_type) {
  if (p_addr_type == NULL) return false;

  std::string addrstr = bda.ToString();
  const char* bd_addr_str = addrstr.c_str();

  if (!btif_config_get_int(bd_addr_str, "AddrType", p_addr_type)) return false;

  LOG_DEBUG(LOG_TAG, "%s: Device [%s] address type %d", __func__, bd_addr_str,
            *p_addr_type);
  return true;
}

/**
 * Read metrics salt from config file, if salt is invalid or does not exist,
 * generate new one and save it to config
 */
static void read_or_set_metrics_salt() {
  AddressObfuscator::Octet32 metrics_salt = {};
  size_t metrics_salt_length = metrics_salt.size();
  if (!btif_config_get_bin(BT_CONFIG_METRICS_SECTION,
                           BT_CONFIG_METRICS_SALT_256BIT, metrics_salt.data(),
                           &metrics_salt_length)) {
    LOG(WARNING) << __func__ << ": Failed to read metrics salt from config";
    // Invalidate salt
    metrics_salt.fill(0);
  }
  if (metrics_salt_length != metrics_salt.size()) {
    LOG(ERROR) << __func__ << ": Metrics salt length incorrect, "
               << metrics_salt_length << " instead of " << metrics_salt.size();
    // Invalidate salt
    metrics_salt.fill(0);
  }
  if (!AddressObfuscator::IsSaltValid(metrics_salt)) {
    LOG(INFO) << __func__ << ": Metrics salt is not invalid, creating new one";
    if (RAND_bytes(metrics_salt.data(), metrics_salt.size()) != 1) {
      LOG(FATAL) << __func__ << "Failed to generate salt for metrics";
    }
    if (!btif_config_set_bin(BT_CONFIG_METRICS_SECTION,
                             BT_CONFIG_METRICS_SALT_256BIT, metrics_salt.data(),
                             metrics_salt.size())) {
      LOG(FATAL) << __func__ << "Failed to write metrics salt to config";
    }
  }
  AddressObfuscator::GetInstance()->Initialize(metrics_salt);
}

/**
 * Initialize metric id allocator by reading metric_id from config by mac
 * address. If there is no metric id for a mac address, then allocate it a new
 * metric id.
 */
static void init_metric_id_allocator() {
  std::unordered_map<RawAddress, int> paired_device_map;

  // When user update the system, there will be devices paired with older
  // version of android without a metric id.
  std::vector<RawAddress> addresses_without_id;

  for (auto& section : btif_config_sections()) {
    auto& section_name = section.name;
    RawAddress mac_address;
    if (!RawAddress::FromString(section_name, mac_address)) {
      continue;
    }
    // if the section name is a mac address
    bool is_valid_id_found = false;
    if (btif_config_exist(section_name, BT_CONFIG_METRICS_ID_KEY)) {
      // there is one metric id under this mac_address
      int id = 0;
      btif_config_get_int(section_name, BT_CONFIG_METRICS_ID_KEY, &id);
      if (MetricIdAllocator::IsValidId(id)) {
        paired_device_map[mac_address] = id;
        is_valid_id_found = true;
      }
    }
    if (!is_valid_id_found) {
      addresses_without_id.push_back(mac_address);
    }
  }

  // Initialize MetricIdAllocator
  MetricIdAllocator::Callback save_device_callback =
      [](const RawAddress& address, const int id) {
        return btif_config_set_int(address.ToString(), BT_CONFIG_METRICS_ID_KEY,
                                   id);
      };
  MetricIdAllocator::Callback forget_device_callback =
      [](const RawAddress& address, const int id) {
        return btif_config_remove(address.ToString(), BT_CONFIG_METRICS_ID_KEY);
      };
  if (!MetricIdAllocator::GetInstance().Init(
          paired_device_map, std::move(save_device_callback),
          std::move(forget_device_callback))) {
    LOG(FATAL) << __func__ << "Failed to initialize MetricIdAllocator";
  }

  // Add device_without_id
  for (auto& address : addresses_without_id) {
    MetricIdAllocator::GetInstance().AllocateId(address);
    MetricIdAllocator::GetInstance().SaveDevice(address);
  }
}

static std::recursive_mutex config_lock;  // protects operations on |config|.
static alarm_t* config_timer;

// limited btif config cache capacity
static BtifConfigCache btif_config_cache(TEMPORARY_SECTION_CAPACITY);

// Module lifecycle functions

static future_t* init(void) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  std::unique_ptr<config_t> config;

  if (is_factory_reset()) delete_config_files();

  std::string file_source;

  if (config_checksum_pass(CONFIG_FILE_COMPARE_PASS)) {
    config = btif_config_open(CONFIG_FILE_PATH);
    btif_config_source = ORIGINAL;
  }
  if (!config) {
    LOG_WARN(LOG_TAG, "%s unable to load config file: %s; using backup.",
             __func__, CONFIG_FILE_PATH);
    if (config_checksum_pass(CONFIG_BACKUP_COMPARE_PASS)) {
      config = btif_config_open(CONFIG_BACKUP_PATH);
      btif_config_source = BACKUP;
      file_source = "Backup";
    }
  }
  if (!config) {
    LOG_WARN(LOG_TAG,
             "%s unable to load backup; attempting to transcode legacy file.",
             __func__);
    config = btif_config_transcode(CONFIG_LEGACY_FILE_PATH);
    btif_config_source = LEGACY;
    file_source = "Legacy";
  }
  if (!config) {
    LOG_ERROR(LOG_TAG,
              "%s unable to transcode legacy file; creating empty config.",
              __func__);
    config = storage_config_get_interface()->config_new_empty();
    btif_config_source = NEW_FILE;
    file_source = "Empty";
  }

  // move persistent config data from btif_config file to btif config cache
  btif_config_cache.Init(std::move(config));

  if (!file_source.empty()) {
    btif_config_cache.SetString(INFO_SECTION, FILE_SOURCE, file_source);
  }

  // Cleanup temporary pairings if we have left guest mode
  if (!is_restricted_mode()) {
    btif_config_cache.RemovePersistentSectionsWithKey("Restricted");
  }

  // Read or set config file creation timestamp
  auto time_str = btif_config_cache.GetString(INFO_SECTION, FILE_TIMESTAMP);
  if (!time_str) {
    time_t current_time = time(NULL);
    struct tm* time_created = localtime(&current_time);
    strftime(btif_config_time_created, TIME_STRING_LENGTH, TIME_STRING_FORMAT,
             time_created);
    btif_config_cache.SetString(INFO_SECTION, FILE_TIMESTAMP,
                                btif_config_time_created);
  } else {
    strlcpy(btif_config_time_created, time_str->c_str(), TIME_STRING_LENGTH);
  }

  // Read or set metrics 256 bit hashing salt
  read_or_set_metrics_salt();

  // Initialize MetricIdAllocator
  init_metric_id_allocator();

  // TODO(sharvil): use a non-wake alarm for this once we have
  // API support for it. There's no need to wake the system to
  // write back to disk.
  config_timer = alarm_new("btif.config");
  if (!config_timer) {
    LOG_ERROR(LOG_TAG, "%s unable to create alarm.", __func__);
    goto error;
  }

  LOG_EVENT_INT(BT_CONFIG_SOURCE_TAG_NUM, btif_config_source);

  return future_new_immediate(FUTURE_SUCCESS);

error:
  alarm_free(config_timer);
  config.reset();
  btif_config_cache.Clear();
  config_timer = NULL;
  btif_config_source = NOT_LOADED;
  return future_new_immediate(FUTURE_FAIL);
}

static std::unique_ptr<config_t> btif_config_open(const char* filename) {
  std::unique_ptr<config_t> config =
      storage_config_get_interface()->config_new(filename);
  if (!config) return nullptr;

  if (!config_has_section(*config, "Adapter")) {
    LOG_ERROR(LOG_TAG, "Config is missing adapter section");
    return nullptr;
  }

  return config;
}

static future_t* shut_down(void) {
  btif_config_flush();
  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t* clean_up(void) {
  btif_config_flush();

  alarm_free(config_timer);
  config_timer = NULL;

  std::unique_lock<std::recursive_mutex> lock(config_lock);
  get_bluetooth_keystore_interface()->clear_map();
  MetricIdAllocator::GetInstance().Close();
  btif_config_cache.Clear();
  return future_new_immediate(FUTURE_SUCCESS);
}

EXPORT_SYMBOL module_t btif_config_module = {.name = BTIF_CONFIG_MODULE,
                                             .init = init,
                                             .start_up = NULL,
                                             .shut_down = shut_down,
                                             .clean_up = clean_up};

bool btif_config_has_section(const char* section) {
  CHECK(section != NULL);

  std::unique_lock<std::recursive_mutex> lock(config_lock);
  return btif_config_cache.HasSection(section);
}

bool btif_config_exist(const std::string& section, const std::string& key) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  return btif_config_cache.HasKey(section, key);
}

bool btif_config_get_int(const std::string& section, const std::string& key,
                         int* value) {
  CHECK(value != NULL);
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  auto ret = btif_config_cache.GetInt(section, key);
  if (!ret) {
    return false;
  }
  *value = *ret;
  return true;
}

bool btif_config_set_int(const std::string& section, const std::string& key,
                         int value) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  btif_config_cache.SetInt(section, key, value);
  return true;
}

bool btif_config_get_uint64(const std::string& section, const std::string& key,
                            uint64_t* value) {
  CHECK(value != NULL);
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  auto ret = btif_config_cache.GetUint64(section, key);
  if (!ret) {
    return false;
  }
  *value = *ret;
  return true;
}

bool btif_config_set_uint64(const std::string& section, const std::string& key,
                            uint64_t value) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  btif_config_cache.SetUint64(section, key, value);
  return true;
}

bool btif_config_get_str(const std::string& section, const std::string& key,
                         char* value, int* size_bytes) {
  CHECK(value != NULL);
  CHECK(size_bytes != NULL);

  {
    std::unique_lock<std::recursive_mutex> lock(config_lock);
    auto stored_value = btif_config_cache.GetString(section, key);
    if (!stored_value) return false;
    strlcpy(value, stored_value->c_str(), *size_bytes);
  }
  *size_bytes = strlen(value) + 1;
  return true;
}

bool btif_config_set_str(const std::string& section, const std::string& key,
                         const std::string& value) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  btif_config_cache.SetString(section, key, value);
  return true;
}

static bool btif_in_encrypt_key_name_list(std::string key) {
  return std::find(encrypt_key_name_list,
                   encrypt_key_name_list + ENCRYPT_KEY_NAME_LIST_SIZE,
                   key) != (encrypt_key_name_list + ENCRYPT_KEY_NAME_LIST_SIZE);
}

bool btif_config_get_bin(const std::string& section, const std::string& key,
                         uint8_t* value, size_t* length) {
  CHECK(value != NULL);
  CHECK(length != NULL);

  std::unique_lock<std::recursive_mutex> lock(config_lock);
  const std::string* value_str;
  auto value_str_from_config = btif_config_cache.GetString(section, key);

  if (!value_str_from_config) {
    VLOG(1) << __func__ << ": cannot find string for section " << section
            << ", key " << key;
    return false;
  }

  bool in_encrypt_key_name_list = btif_in_encrypt_key_name_list(key);
  bool is_key_encrypted = *value_str_from_config == ENCRYPTED_STR;
  std::string string;

  if (!value_str_from_config->empty() && in_encrypt_key_name_list &&
      is_key_encrypted) {
    string = get_bluetooth_keystore_interface()->get_key(section + "-" + key);
    value_str = &string;
  } else {
    value_str = &value_str_from_config.value();
  }

  size_t value_len = value_str->length();
  if ((value_len % 2) != 0 || *length < (value_len / 2)) {
    LOG(WARNING) << ": value size not divisible by 2, size is " << value_len;
    return false;
  }

  for (size_t i = 0; i < value_len; ++i)
    if (!isxdigit(value_str->c_str()[i])) {
      LOG(WARNING) << ": value is not hex digit";
      return false;
    }

  const char* ptr = value_str->c_str();
  for (*length = 0; *ptr; ptr += 2, *length += 1) {
    sscanf(ptr, "%02hhx", &value[*length]);
  }

  if (btif_is_niap_mode()) {
    if (!value_str_from_config->empty() && in_encrypt_key_name_list &&
        !is_key_encrypted) {
      get_bluetooth_keystore_interface()->set_encrypt_key_or_remove_key(
          section + "-" + key, *value_str_from_config);
      btif_config_cache.SetString(section, key, ENCRYPTED_STR);
    }
  } else {
    if (in_encrypt_key_name_list && is_key_encrypted) {
      btif_config_cache.SetString(section, key, *value_str);
    }
  }

  return true;
}

size_t btif_config_get_bin_length(const std::string& section,
                                  const std::string& key) {
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  auto value_str = btif_config_cache.GetString(section, key);
  if (!value_str) return 0;
  size_t value_len = value_str->length();
  return ((value_len % 2) != 0) ? 0 : (value_len / 2);
}

bool btif_config_set_bin(const std::string& section, const std::string& key,
                         const uint8_t* value, size_t length) {
  const char* lookup = "0123456789abcdef";
  if (length > 0) CHECK(value != NULL);

  size_t max_value = ((size_t)-1);
  if (((max_value - 1) / 2) < length) {
    LOG(ERROR) << __func__ << ": length too long";
    return false;
  }

  char* str = (char*)osi_calloc(length * 2 + 1);

  for (size_t i = 0; i < length; ++i) {
    str[(i * 2) + 0] = lookup[(value[i] >> 4) & 0x0F];
    str[(i * 2) + 1] = lookup[value[i] & 0x0F];
  }

  std::string value_str;
  if ((length > 0) && btif_is_niap_mode() &&
      btif_in_encrypt_key_name_list(key)) {
    get_bluetooth_keystore_interface()->set_encrypt_key_or_remove_key(
        section + "-" + key, str);
    value_str = ENCRYPTED_STR;
  } else {
    value_str = str;
  }

  {
    std::unique_lock<std::recursive_mutex> lock(config_lock);
    btif_config_cache.SetString(section, key, value_str);
  }

  osi_free(str);
  return true;
}

const std::list<section_t>& btif_config_sections() {
  return btif_config_cache.GetPersistentSections();
}

bool btif_config_remove(const std::string& section, const std::string& key) {
  if (is_niap_mode() && btif_in_encrypt_key_name_list(key)) {
    get_bluetooth_keystore_interface()->set_encrypt_key_or_remove_key(
        section + "-" + key, "");
  }
  std::unique_lock<std::recursive_mutex> lock(config_lock);
  return btif_config_cache.RemoveKey(section, key);
}

void btif_config_save(void) {
  CHECK(config_timer != NULL);

  alarm_set(config_timer, CONFIG_SETTLE_PERIOD_MS, timer_config_save_cb, NULL);
}

void btif_config_flush(void) {
  CHECK(config_timer != NULL);

  alarm_cancel(config_timer);
  btif_config_write(0, NULL);
}

bool btif_config_clear(void) {
  CHECK(config_timer != NULL);

  alarm_cancel(config_timer);

  std::unique_lock<std::recursive_mutex> lock(config_lock);

  btif_config_cache.Clear();
  bool ret = storage_config_get_interface()->config_save(
      btif_config_cache.PersistentSectionCopy(), CONFIG_FILE_PATH);
  btif_config_source = RESET;

  return ret;
}

static void timer_config_save_cb(UNUSED_ATTR void* data) {
  // Moving file I/O to btif context instead of timer callback because
  // it usually takes a lot of time to be completed, introducing
  // delays during A2DP playback causing blips or choppiness.
  btif_transfer_context(btif_config_write, 0, NULL, 0, NULL);
}

static void btif_config_write(UNUSED_ATTR uint16_t event,
                              UNUSED_ATTR char* p_param) {
  CHECK(config_timer != NULL);

  std::unique_lock<std::recursive_mutex> lock(config_lock);
  rename(CONFIG_FILE_PATH, CONFIG_BACKUP_PATH);
  storage_config_get_interface()->config_save(
      btif_config_cache.PersistentSectionCopy(), CONFIG_FILE_PATH);
  if (btif_is_niap_mode()) {
    get_bluetooth_keystore_interface()->set_encrypt_key_or_remove_key(
        CONFIG_FILE_PREFIX, CONFIG_FILE_HASH);
  }
}

void btif_debug_config_dump(int fd) {
  dprintf(fd, "\nBluetooth Config:\n");

  dprintf(fd, "  Config Source: ");
  switch (btif_config_source) {
    case NOT_LOADED:
      dprintf(fd, "Not loaded\n");
      break;
    case ORIGINAL:
      dprintf(fd, "Original file\n");
      break;
    case BACKUP:
      dprintf(fd, "Backup file\n");
      break;
    case LEGACY:
      dprintf(fd, "Legacy file\n");
      break;
    case NEW_FILE:
      dprintf(fd, "New file\n");
      break;
    case RESET:
      dprintf(fd, "Reset file\n");
      break;
  }

  auto file_source = btif_config_cache.GetString(INFO_SECTION, FILE_SOURCE);
  if (!file_source) {
    file_source.emplace("Original");
  }

  dprintf(fd, "  Devices loaded: %zu\n",
          btif_config_cache.GetPersistentSections().size());
  dprintf(fd, "  File created/tagged: %s\n", btif_config_time_created);
  dprintf(fd, "  File source: %s\n", file_source->c_str());
}

static bool is_factory_reset(void) {
  char factory_reset[PROPERTY_VALUE_MAX] = {0};
  osi_property_get("persist.bluetooth.factoryreset", factory_reset, "false");
  return strncmp(factory_reset, "true", 4) == 0;
}

static void delete_config_files(void) {
  remove(CONFIG_FILE_PATH);
  remove(CONFIG_BACKUP_PATH);
  osi_property_set("persist.bluetooth.factoryreset", "false");
}
