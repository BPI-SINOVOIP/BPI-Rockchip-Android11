/*
 * hidl interface for wpa_supplicant daemon
 * Copyright (c) 2004-2016, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004-2016, Roshan Pius <rpius@google.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef MISC_UTILS_H_
#define MISC_UTILS_H_

#include <iostream>

extern "C"
{
#include "wpabuf.h"
}

namespace {
constexpr size_t kWpsPinNumDigits = 8;
// Custom deleter for wpabuf.
void freeWpaBuf(wpabuf *ptr) { wpabuf_free(ptr); }
}  // namespace

namespace android {
namespace hardware {
namespace wifi {
namespace supplicant {
namespace V1_3 {
namespace implementation {
namespace misc_utils {
using wpabuf_unique_ptr = std::unique_ptr<wpabuf, void (*)(wpabuf *)>;

// Creates a unique_ptr for wpabuf ptr with a custom deleter.
inline wpabuf_unique_ptr createWpaBufUniquePtr(struct wpabuf *raw_ptr)
{
	return {raw_ptr, freeWpaBuf};
}

// Creates a wpabuf ptr with a custom deleter copying the data from the provided
// vector.
inline wpabuf_unique_ptr convertVectorToWpaBuf(const std::vector<uint8_t> &data)
{
	return createWpaBufUniquePtr(
	    wpabuf_alloc_copy(data.data(), data.size()));
}

// Copies the provided wpabuf contents to a std::vector.
inline std::vector<uint8_t> convertWpaBufToVector(const struct wpabuf *buf)
{
	if (buf) {
		return std::vector<uint8_t>(
		    wpabuf_head_u8(buf), wpabuf_head_u8(buf) + wpabuf_len(buf));
	} else {
		return std::vector<uint8_t>();
	}
}

// Returns a string holding the wps pin.
inline std::string convertWpsPinToString(int pin)
{
	char pin_str[kWpsPinNumDigits + 1];
	snprintf(pin_str, sizeof(pin_str), "%08d", pin);
	return pin_str;
}

inline std::stringstream& serializePmkCacheEntry(
    std::stringstream &ss, struct rsn_pmksa_cache_entry *pmksa_entry) {
	ss.write((char *) &pmksa_entry->pmk_len, sizeof(pmksa_entry->pmk_len));
	ss.write((char *) pmksa_entry->pmk, pmksa_entry->pmk_len);
	ss.write((char *) pmksa_entry->pmkid, PMKID_LEN);
	ss.write((char *) pmksa_entry->aa, ETH_ALEN);
	// Omit wpa_ssid field because the network is created on connecting to a access point.
	ss.write((char *) &pmksa_entry->akmp, sizeof(pmksa_entry->akmp));
	ss.write((char *) &pmksa_entry->reauth_time, sizeof(pmksa_entry->reauth_time));
	ss.write((char *) &pmksa_entry->expiration, sizeof(pmksa_entry->expiration));
	ss.write((char *) &pmksa_entry->opportunistic, sizeof(pmksa_entry->opportunistic));
	char byte = (pmksa_entry->fils_cache_id_set) ? 1 : 0;
	ss.write((char *) &byte, sizeof(byte));
	ss.write((char *) pmksa_entry->fils_cache_id, FILS_CACHE_ID_LEN);
	ss << std::flush;
	return ss;
}

inline std::stringstream& deserializePmkCacheEntry(
    std::stringstream &ss, struct rsn_pmksa_cache_entry *pmksa_entry) {
	ss.seekg(0);
	ss.read((char *) &pmksa_entry->pmk_len, sizeof(pmksa_entry->pmk_len));
	ss.read((char *) pmksa_entry->pmk, pmksa_entry->pmk_len);
	ss.read((char *) pmksa_entry->pmkid, PMKID_LEN);
	ss.read((char *) pmksa_entry->aa, ETH_ALEN);
	// Omit wpa_ssid field because the network is created on connecting to a access point.
	ss.read((char *) &pmksa_entry->akmp, sizeof(pmksa_entry->akmp));
	ss.read((char *) &pmksa_entry->reauth_time, sizeof(pmksa_entry->reauth_time));
	ss.read((char *) &pmksa_entry->expiration, sizeof(pmksa_entry->expiration));
	ss.read((char *) &pmksa_entry->opportunistic, sizeof(pmksa_entry->opportunistic));
	char byte = 0;
	ss.read((char *) &byte, sizeof(byte));
	pmksa_entry->fils_cache_id_set = (byte) ? 1 : 0;
	ss.read((char *) pmksa_entry->fils_cache_id, FILS_CACHE_ID_LEN);
	return ss;
}
}  // namespace misc_utils
}  // namespace implementation
}  // namespace V1_3
}  // namespace supplicant
}  // namespace wifi
}  // namespace hardware
}  // namespace android
#endif  // MISC_UTILS_H_
