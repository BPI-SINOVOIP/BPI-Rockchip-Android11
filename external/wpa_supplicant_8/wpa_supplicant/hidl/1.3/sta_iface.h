/*
 * hidl interface for wpa_supplicant daemon
 * Copyright (c) 2004-2016, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004-2016, Roshan Pius <rpius@google.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_SUPPLICANT_HIDL_STA_IFACE_H
#define WPA_SUPPLICANT_HIDL_STA_IFACE_H

#include <array>
#include <vector>

#include <android-base/macros.h>

#include <android/hardware/wifi/supplicant/1.3/ISupplicantStaIface.h>
#include <android/hardware/wifi/supplicant/1.2/ISupplicantStaIfaceCallback.h>
#include <android/hardware/wifi/supplicant/1.3/ISupplicantStaNetwork.h>

extern "C"
{
#include "utils/common.h"
#include "utils/includes.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#include "driver_i.h"
#include "wpa.h"
}

namespace android {
namespace hardware {
namespace wifi {
namespace supplicant {
namespace V1_3 {
namespace implementation {
using V1_0::ISupplicantNetwork;
using android::hardware::wifi::supplicant::V1_2::DppAkm;
using android::hardware::wifi::supplicant::V1_2::DppNetRole;

/**
 * Implementation of StaIface hidl object. Each unique hidl
 * object is used for control operations on a specific interface
 * controlled by wpa_supplicant.
 */
class StaIface : public V1_3::ISupplicantStaIface
{
public:
	StaIface(struct wpa_global* wpa_global, const char ifname[]);
	~StaIface() override = default;
	// HIDL does not provide a built-in mechanism to let the server
	// invalidate a HIDL interface object after creation. If any client
	// process holds onto a reference to the object in their context,
	// any method calls on that reference will continue to be directed to
	// the server.
	// However Supplicant HAL needs to control the lifetime of these
	// objects. So, add a public |invalidate| method to all |Iface| and
	// |Network| objects.
	// This will be used to mark an object invalid when the corresponding
	// iface or network is removed.
	// All HIDL method implementations should check if the object is still
	// marked valid before processing them.
	void invalidate();
	bool isValid();

	// Hidl methods exposed.
	Return<void> getName(getName_cb _hidl_cb) override;
	Return<void> getType(getType_cb _hidl_cb) override;
	Return<void> addNetwork(addNetwork_cb _hidl_cb) override;
	Return<void> removeNetwork(
	    SupplicantNetworkId id, removeNetwork_cb _hidl_cb) override;
	Return<void> filsHlpFlushRequest(
	    filsHlpFlushRequest_cb _hidl_cb) override;
	Return<void> filsHlpAddRequest(
	    const hidl_array<uint8_t, 6>& dst_mac, const hidl_vec<uint8_t>& pkt,
	    filsHlpAddRequest_cb _hidl_cb) override;
	Return<void> getNetwork(
	    SupplicantNetworkId id, getNetwork_cb _hidl_cb) override;
	Return<void> listNetworks(listNetworks_cb _hidl_cb) override;
	Return<void> registerCallback(
	    const sp<V1_0::ISupplicantStaIfaceCallback>& callback,
	    registerCallback_cb _hidl_cb) override;
	Return<void> registerCallback_1_1(
	    const sp<V1_1::ISupplicantStaIfaceCallback>& callback,
	    registerCallback_cb _hidl_cb) override;
	Return<void> registerCallback_1_2(
	    const sp<V1_2::ISupplicantStaIfaceCallback>& callback,
	    registerCallback_cb _hidl_cb) override;
	Return<void> registerCallback_1_3(
	    const sp<V1_3::ISupplicantStaIfaceCallback>& callback,
	    registerCallback_cb _hidl_cb) override;
	Return<void> reassociate(reassociate_cb _hidl_cb) override;
	Return<void> reconnect(reconnect_cb _hidl_cb) override;
	Return<void> disconnect(disconnect_cb _hidl_cb) override;
	Return<void> setPowerSave(
	    bool enable, setPowerSave_cb _hidl_cb) override;
	Return<void> initiateTdlsDiscover(
	    const hidl_array<uint8_t, 6>& mac_address,
	    initiateTdlsDiscover_cb _hidl_cb) override;
	Return<void> initiateTdlsSetup(
	    const hidl_array<uint8_t, 6>& mac_address,
	    initiateTdlsSetup_cb _hidl_cb) override;
	Return<void> initiateTdlsTeardown(
	    const hidl_array<uint8_t, 6>& mac_address,
	    initiateTdlsTeardown_cb _hidl_cb) override;
	Return<void> initiateAnqpQuery(
	    const hidl_array<uint8_t, 6>& mac_address,
	    const hidl_vec<ISupplicantStaIface::AnqpInfoId>& info_elements,
	    const hidl_vec<ISupplicantStaIface::Hs20AnqpSubtypes>& sub_types,
	    initiateAnqpQuery_cb _hidl_cb) override;
	Return<void> initiateHs20IconQuery(
	    const hidl_array<uint8_t, 6>& mac_address,
	    const hidl_string& file_name,
	    initiateHs20IconQuery_cb _hidl_cb) override;
	Return<void> getMacAddress(getMacAddress_cb _hidl_cb) override;
	Return<void> startRxFilter(startRxFilter_cb _hidl_cb) override;
	Return<void> stopRxFilter(stopRxFilter_cb _hidl_cb) override;
	Return<void> addRxFilter(
	    ISupplicantStaIface::RxFilterType type,
	    addRxFilter_cb _hidl_cb) override;
	Return<void> removeRxFilter(
	    ISupplicantStaIface::RxFilterType type,
	    removeRxFilter_cb _hidl_cb) override;
	Return<void> setBtCoexistenceMode(
	    ISupplicantStaIface::BtCoexistenceMode mode,
	    setBtCoexistenceMode_cb _hidl_cb) override;
	Return<void> setBtCoexistenceScanModeEnabled(
	    bool enable, setBtCoexistenceScanModeEnabled_cb _hidl_cb) override;
	Return<void> setSuspendModeEnabled(
	    bool enable, setSuspendModeEnabled_cb _hidl_cb) override;
	Return<void> setCountryCode(
	    const hidl_array<int8_t, 2>& code,
	    setCountryCode_cb _hidl_cb) override;
	Return<void> startWpsRegistrar(
	    const hidl_array<uint8_t, 6>& bssid, const hidl_string& pin,
	    startWpsRegistrar_cb _hidl_cb) override;
	Return<void> startWpsPbc(
	    const hidl_array<uint8_t, 6>& bssid,
	    startWpsPbc_cb _hidl_cb) override;
	Return<void> startWpsPinKeypad(
	    const hidl_string& pin, startWpsPinKeypad_cb _hidl_cb) override;
	Return<void> startWpsPinDisplay(
	    const hidl_array<uint8_t, 6>& bssid,
	    startWpsPinDisplay_cb _hidl_cb) override;
	Return<void> cancelWps(cancelWps_cb _hidl_cb) override;
	Return<void> setWpsDeviceName(
	    const hidl_string& name, setWpsDeviceName_cb _hidl_cb) override;
	Return<void> setWpsDeviceType(
	    const hidl_array<uint8_t, 8>& type,
	    setWpsDeviceType_cb _hidl_cb) override;
	Return<void> setWpsManufacturer(
	    const hidl_string& manufacturer,
	    setWpsManufacturer_cb _hidl_cb) override;
	Return<void> setWpsModelName(
	    const hidl_string& model_name,
	    setWpsModelName_cb _hidl_cb) override;
	Return<void> setWpsModelNumber(
	    const hidl_string& model_number,
	    setWpsModelNumber_cb _hidl_cb) override;
	Return<void> setWpsSerialNumber(
	    const hidl_string& serial_number,
	    setWpsSerialNumber_cb _hidl_cb) override;
	Return<void> setWpsConfigMethods(
	    uint16_t config_methods, setWpsConfigMethods_cb _hidl_cb) override;
	Return<void> setExternalSim(
	    bool useExternalSim, setExternalSim_cb _hidl_cb) override;
	Return<void> addExtRadioWork(
	    const hidl_string& name, uint32_t freq_in_mhz,
	    uint32_t timeout_in_sec, addExtRadioWork_cb _hidl_cb) override;
	Return<void> removeExtRadioWork(
	    uint32_t id, removeExtRadioWork_cb _hidl_cb) override;
	Return<void> enableAutoReconnect(
	    bool enable, enableAutoReconnect_cb _hidl_cb) override;
	Return<void> getKeyMgmtCapabilities(
	    getKeyMgmtCapabilities_cb _hidl_cb) override;
	Return<void> addDppPeerUri(const hidl_string& uri,
			addDppPeerUri_cb _hidl_cb) override;
	Return<void> removeDppUri(uint32_t bootstrap_id,
			removeDppUri_cb _hidl_cb) override;
	Return<void> startDppConfiguratorInitiator(uint32_t peer_bootstrap_id,
			uint32_t own_bootstrap_id, const hidl_string& ssid,
			const hidl_string& password, const hidl_string& psk,
			DppNetRole net_role, DppAkm security_akm,
			startDppConfiguratorInitiator_cb _hidl_cb) override;
	Return<void> startDppEnrolleeInitiator(uint32_t peer_bootstrap_id,
			uint32_t own_bootstrap_id,
			startDppConfiguratorInitiator_cb _hidl_cb) override;
	Return<void> stopDppInitiator(stopDppInitiator_cb _hidl_cb) override;
	Return<void> getConnectionCapabilities(
	    getConnectionCapabilities_cb _hidl_cb) override;
	Return<void> getWpaDriverCapabilities(
	    getWpaDriverCapabilities_cb _hidl_cb) override;
	Return<void> setMboCellularDataStatus(bool available,
	    setMboCellularDataStatus_cb _hidl_cb) override;
	Return<void> getKeyMgmtCapabilities_1_3(
	    getKeyMgmtCapabilities_1_3_cb _hidl_cb) override;

private:
	// Corresponding worker functions for the HIDL methods.
	std::pair<SupplicantStatus, std::string> getNameInternal();
	std::pair<SupplicantStatus, IfaceType> getTypeInternal();
	std::pair<SupplicantStatus, sp<ISupplicantNetwork>>
	addNetworkInternal();
	SupplicantStatus filsHlpFlushRequestInternal();
	SupplicantStatus filsHlpAddRequestInternal(
	    const std::array<uint8_t, 6>& dst_mac,
	    const std::vector<uint8_t>& pkt);
	SupplicantStatus removeNetworkInternal(SupplicantNetworkId id);
	std::pair<SupplicantStatus, sp<ISupplicantNetwork>> getNetworkInternal(
	    SupplicantNetworkId id);
	std::pair<SupplicantStatus, std::vector<SupplicantNetworkId>>
	listNetworksInternal();
	SupplicantStatus registerCallbackInternal(
	    const sp<V1_0::ISupplicantStaIfaceCallback>& callback);
	SupplicantStatus registerCallbackInternal_1_1(
	    const sp<V1_1::ISupplicantStaIfaceCallback>& callback);
	SupplicantStatus reassociateInternal();
	SupplicantStatus reconnectInternal();
	SupplicantStatus disconnectInternal();
	SupplicantStatus setPowerSaveInternal(bool enable);
	SupplicantStatus initiateTdlsDiscoverInternal(
	    const std::array<uint8_t, 6>& mac_address);
	SupplicantStatus initiateTdlsSetupInternal(
	    const std::array<uint8_t, 6>& mac_address);
	SupplicantStatus initiateTdlsTeardownInternal(
	    const std::array<uint8_t, 6>& mac_address);
	SupplicantStatus initiateAnqpQueryInternal(
	    const std::array<uint8_t, 6>& mac_address,
	    const std::vector<ISupplicantStaIface::AnqpInfoId>& info_elements,
	    const std::vector<ISupplicantStaIface::Hs20AnqpSubtypes>&
		sub_types);
	SupplicantStatus initiateHs20IconQueryInternal(
	    const std::array<uint8_t, 6>& mac_address,
	    const std::string& file_name);
	std::pair<SupplicantStatus, std::array<uint8_t, 6>>
	getMacAddressInternal();
	SupplicantStatus startRxFilterInternal();
	SupplicantStatus stopRxFilterInternal();
	SupplicantStatus addRxFilterInternal(
	    ISupplicantStaIface::RxFilterType type);
	SupplicantStatus removeRxFilterInternal(
	    ISupplicantStaIface::RxFilterType type);
	SupplicantStatus setBtCoexistenceModeInternal(
	    ISupplicantStaIface::BtCoexistenceMode mode);
	SupplicantStatus setBtCoexistenceScanModeEnabledInternal(bool enable);
	SupplicantStatus setSuspendModeEnabledInternal(bool enable);
	SupplicantStatus setCountryCodeInternal(
	    const std::array<int8_t, 2>& code);
	SupplicantStatus startWpsRegistrarInternal(
	    const std::array<uint8_t, 6>& bssid, const std::string& pin);
	SupplicantStatus startWpsPbcInternal(
	    const std::array<uint8_t, 6>& bssid);
	SupplicantStatus startWpsPinKeypadInternal(const std::string& pin);
	std::pair<SupplicantStatus, std::string> startWpsPinDisplayInternal(
	    const std::array<uint8_t, 6>& bssid);
	SupplicantStatus cancelWpsInternal();
	SupplicantStatus setWpsDeviceNameInternal(const std::string& name);
	SupplicantStatus setWpsDeviceTypeInternal(
	    const std::array<uint8_t, 8>& type);
	SupplicantStatus setWpsManufacturerInternal(
	    const std::string& manufacturer);
	SupplicantStatus setWpsModelNameInternal(const std::string& model_name);
	SupplicantStatus setWpsModelNumberInternal(
	    const std::string& model_number);
	SupplicantStatus setWpsSerialNumberInternal(
	    const std::string& serial_number);
	SupplicantStatus setWpsConfigMethodsInternal(uint16_t config_methods);
	SupplicantStatus setExternalSimInternal(bool useExternalSim);
	std::pair<SupplicantStatus, uint32_t> addExtRadioWorkInternal(
	    const std::string& name, uint32_t freq_in_mhz,
	    uint32_t timeout_in_sec);
	SupplicantStatus removeExtRadioWorkInternal(uint32_t id);
	SupplicantStatus enableAutoReconnectInternal(bool enable);
	std::pair<SupplicantStatus, uint32_t> getKeyMgmtCapabilitiesInternal();
	std::pair<SupplicantStatus, uint32_t> addDppPeerUriInternal(const std::string& uri);
	SupplicantStatus removeDppUriInternal(uint32_t bootstrap_id);
	SupplicantStatus startDppConfiguratorInitiatorInternal(uint32_t peer_bootstrap_id,
			uint32_t own_bootstrap_id,
		    const std::string& ssid, const std::string& password,
			const std::string& psk, DppNetRole net_role, DppAkm security_akm);
	SupplicantStatus startDppEnrolleeInitiatorInternal(uint32_t peer_bootstrap_id,
			uint32_t own_bootstrap_id);
	SupplicantStatus stopDppInitiatorInternal();
	std::pair<SupplicantStatus, ConnectionCapabilities> getConnectionCapabilitiesInternal();
	std::pair<SupplicantStatus, uint32_t> getWpaDriverCapabilitiesInternal();
	SupplicantStatus setMboCellularDataStatusInternal(bool available);
	std::pair<SupplicantStatus, uint32_t> getKeyMgmtCapabilitiesInternal_1_3();

	struct wpa_supplicant* retrieveIfacePtr();

	// Reference to the global wpa_struct. This is assumed to be valid for
	// the lifetime of the process.
	struct wpa_global* wpa_global_;
	// Name of the iface this hidl object controls
	const std::string ifname_;
	bool is_valid_;

	DISALLOW_COPY_AND_ASSIGN(StaIface);
};

}  // namespace implementation
}  // namespace V1_3
}  // namespace supplicant
}  // namespace wifi
}  // namespace hardware
}  // namespace android

#endif  // WPA_SUPPLICANT_HIDL_STA_IFACE_H
