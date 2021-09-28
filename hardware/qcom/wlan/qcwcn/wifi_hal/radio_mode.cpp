/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sync.h"

#include <utils/Log.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"
#include "radio_mode.h"
#include "vendor_definitions.h"
#include <netlink/genl/genl.h>
#include <string.h>
#include <net/if.h>

/* Used to handle radio mode command events from driver/firmware.*/
void RADIOModeCommand::setCallbackHandler(wifi_radio_mode_change_handler handler)
{
    mHandler = handler;
}

void RADIOModeCommand::setReqId(wifi_request_id id)
{
    mreqId =  id;
}

RADIOModeCommand::RADIOModeCommand(wifi_handle handle, int id,
                                   u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mHandler, 0, sizeof(mHandler));
    if (registerVendorHandler(vendor_id, subcmd)) {
        /* Error case should not happen print log */
        ALOGE("%s: Unable to register Vendor Handler Vendor Id=0x%x subcmd=%u",
              __FUNCTION__, vendor_id, subcmd);
    }
}

RADIOModeCommand::~RADIOModeCommand()
{
    unregisterVendorHandler(mVendor_id, mSubcmd);
}


RADIOModeCommand* RADIOModeCommand::instance(wifi_handle handle,
                                             wifi_request_id id)
{
    RADIOModeCommand* mRADIOModeCommandInstance;

    if (handle == NULL) {
        ALOGE("Interface Handle is invalid");
        return NULL;
    }
    hal_info *info = getHalInfo(handle);
    if (!info) {
        ALOGE("hal_info is invalid");
        return NULL;
    }
    mRADIOModeCommandInstance = new RADIOModeCommand(handle, id,
                OUI_QCA,
                QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO);
    return mRADIOModeCommandInstance;
}

/* This function will be the main handler for incoming event.
 * Call the appropriate callback handler after parsing the vendor data.
 */
int RADIOModeCommand::handleEvent(WifiEvent &event)
{
    wifi_error ret = WIFI_SUCCESS;
    int num_of_mac = 0;
    wifi_mac_info mode_info;

    memset(&mode_info, 0, sizeof(mode_info));
    WifiVendorCommand::handleEvent(event);

    /* Parse the vendordata and get the attribute */
    switch(mSubcmd)
    {
        case QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO:
        {
            struct nlattr *mtb_vendor[QCA_WLAN_VENDOR_ATTR_MAC_MAX + 1];
            struct nlattr *modeInfo;
            int rem;

            nla_parse(mtb_vendor, QCA_WLAN_VENDOR_ATTR_MAC_MAX,
                      (struct nlattr *)mVendorData,
                      mDataLen, NULL);

            if (mtb_vendor[QCA_WLAN_VENDOR_ATTR_MAC_INFO])
            {
                for (modeInfo = (struct nlattr *) nla_data(mtb_vendor[QCA_WLAN_VENDOR_ATTR_MAC_INFO]),
                     rem = nla_len(mtb_vendor[QCA_WLAN_VENDOR_ATTR_MAC_INFO]);
                     nla_ok(modeInfo, rem);modeInfo = nla_next(modeInfo, &(rem))) {

                     struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAX+ 1];
                     nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAX,
                                (struct nlattr *) nla_data(modeInfo), nla_len(modeInfo), NULL);
                     if (!tb2[QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAC_ID])
                     {
                        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAC_ID"
                               " not found", __FUNCTION__);
                        return WIFI_ERROR_INVALID_ARGS;
                     }
                     mode_info.wlan_mac_id = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_MAC_INFO_MAC_ID]);
                     ALOGV("mac_id[%d]: %d ", num_of_mac, mode_info.wlan_mac_id);

                     if (!tb2[QCA_WLAN_VENDOR_ATTR_MAC_INFO_BAND])
                     {
                         ALOGE("%s: QCA_WLAN_VENDOR_ATTR_MAC_INFO_BAND"
                               " NOT FOUND", __FUNCTION__);
                         return WIFI_ERROR_INVALID_ARGS;
                     }
                     mode_info.mac_band = (wlan_mac_band) nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_MAC_INFO_BAND]);
                     ALOGV("mac_band[%d]: %d ", num_of_mac, mode_info.mac_band);

                     if (tb2[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO])
                     {
                       int num_of_iface = 0;
                       struct nlattr *tb_iface;
                       int rem_info;

                       for (tb_iface = (struct nlattr *) nla_data(tb2[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO]),
                            rem_info = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO]);
                            nla_ok(tb_iface, rem_info);tb_iface = nla_next(tb_iface, &(rem_info))) {

                            struct nlattr *tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_MAX+ 1];
                            wifi_iface_info miface_info;

                            nla_parse(tb3, QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_MAX,
                                      (struct nlattr *) nla_data(tb_iface), nla_len(tb_iface), NULL);

                            if (!tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_IFINDEX])
                            {
                                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_IFINDEX"
                                      " NOT FOUND", __FUNCTION__);
                                return WIFI_ERROR_INVALID_ARGS;
                            }
                            if (if_indextoname(nla_get_u32(tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_IFINDEX]),
                                               miface_info.iface_name) == NULL)
                            {
                                ALOGE("%s: Failed to convert %d IFINDEX to IFNAME", __FUNCTION__,
                                      nla_get_u32(tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_IFINDEX]));
                            }
                            ALOGV("ifname[%d]: %s ", num_of_iface, miface_info.iface_name);

                            if (!tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_FREQ])
                            {
                                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_FREQ"
                                      " NOT FOUND", __FUNCTION__);
                                return WIFI_ERROR_INVALID_ARGS;
                            }
                            miface_info.channel = nla_get_u32(tb3[QCA_WLAN_VENDOR_ATTR_MAC_IFACE_INFO_FREQ]);
                            ALOGV("channel[%d]: %d ", num_of_iface, miface_info.channel);

                            if (!num_of_iface)
                               mode_info.iface_info = (wifi_iface_info *)
                                         malloc(sizeof(wifi_iface_info));
                            else
                               mode_info.iface_info = (wifi_iface_info *)
                                         realloc(mode_info.iface_info, (num_of_iface + 1) * sizeof(wifi_iface_info));

                            memcpy(&mode_info.iface_info[num_of_iface], &miface_info, sizeof(wifi_iface_info));
                            num_of_iface++;
                            mode_info.num_iface = num_of_iface;
                       }
                    }
                    if (!num_of_mac)
                       mwifi_iface_mac_info = (wifi_mac_info *)
                          malloc(sizeof(wifi_mac_info));
                    else
                       mwifi_iface_mac_info = (wifi_mac_info *)
                          realloc(mwifi_iface_mac_info, (num_of_mac + 1) * (sizeof(wifi_mac_info)));

                    memcpy(&mwifi_iface_mac_info[num_of_mac], &mode_info, sizeof(wifi_mac_info));
                    num_of_mac++;
                }
            }

            if (mHandler.on_radio_mode_change && num_of_mac) {
                (*mHandler.on_radio_mode_change)(mreqId, num_of_mac, mwifi_iface_mac_info);
                free(mwifi_iface_mac_info);
                mwifi_iface_mac_info = NULL;
            }
            else {
                  ALOGE("No Callback registered: on radio mode change");
                  free(mwifi_iface_mac_info);
                  mwifi_iface_mac_info = NULL;
            }
        }
        break;

        default:
            /* Error case should not happen print log */
            ALOGE("%s: Wrong subcmd received %d", __FUNCTION__, mSubcmd);
    }

    return ret;
}

wifi_error wifi_set_radio_mode_change_handler(wifi_request_id id,
                                      wifi_interface_handle iface,
                                      wifi_radio_mode_change_handler eh)
{
    wifi_error ret;
    WifiVendorCommand *vCommand = NULL;
    wifi_handle wifiHandle = getWifiHandle(iface);
    RADIOModeCommand *radiomodeCommand;

    ret = initialize_vendor_cmd(iface, id,
                                QCA_NL80211_VENDOR_SUBCMD_WLAN_MAC_INFO,
                                &vCommand);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Initialization failed", __FUNCTION__);
        return ret;
    }

    radiomodeCommand = RADIOModeCommand::instance(wifiHandle, id);
    if (radiomodeCommand == NULL) {
        ALOGE("%s: Error RadioModeCommand NULL", __FUNCTION__);
        ret = WIFI_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    radiomodeCommand->setCallbackHandler(eh);
    radiomodeCommand->setReqId(id);

cleanup:
    delete vCommand;
    return mapKernelErrortoWifiHalError(ret);
}
