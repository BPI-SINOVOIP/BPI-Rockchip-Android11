/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#define LOG_TAG  "WifiHAL"

#include <utils/Log.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"
#include "wifihal_vendorcommand.h"

//Singleton Static Instance
NUDStatsCommand* NUDStatsCommand::mNUDStatsCommandInstance  = NULL;

// This function implements creation of Vendor command
// For NUDStats just call base Vendor command create
wifi_error NUDStatsCommand::create() {
    wifi_error ret = mMsg.create(NL80211_CMD_VENDOR, 0, 0);
    if (ret != WIFI_SUCCESS) {
        return ret;
    }
    // insert the oui in the msg
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_ID, mVendor_id);
    if (ret != WIFI_SUCCESS)
        goto out;

    // insert the subcmd in the msg
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_SUBCMD, mSubcmd);
    if (ret != WIFI_SUCCESS)
        goto out;

out:
    return ret;
}

NUDStatsCommand::NUDStatsCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mStats, 0,sizeof(nud_stats));
}

NUDStatsCommand::~NUDStatsCommand()
{
    mNUDStatsCommandInstance = NULL;
}

NUDStatsCommand* NUDStatsCommand::instance(wifi_handle handle)
{
    if (handle == NULL) {
        ALOGE("Interface Handle is invalid");
        return NULL;
    }
    if (mNUDStatsCommandInstance == NULL) {
        mNUDStatsCommandInstance = new NUDStatsCommand(handle, 0,
                OUI_QCA,
                QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET);
        return mNUDStatsCommandInstance;
    }
    else
    {
        if (handle != getWifiHandle(mNUDStatsCommandInstance->mInfo))
        {
            /* upper layer must have cleaned up the handle and reinitialized,
               so we need to update the same */
            ALOGE("Handle different, update the handle");
            mNUDStatsCommandInstance->mInfo = (hal_info *)handle;
        }
    }
    return mNUDStatsCommandInstance;
}

void NUDStatsCommand::setSubCmd(u32 subcmd)
{
    mSubcmd = subcmd;
}

wifi_error NUDStatsCommand::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

int NUDStatsCommand::handleResponse(WifiEvent &reply)
{
    int status = WIFI_ERROR_NONE;
    WifiVendorCommand::handleResponse(reply);

    // Parse the vendordata and get the attribute

    switch(mSubcmd)
    {
        case QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_GET:
        {
            struct nlattr *tb_vendor[QCA_ATTR_NUD_STATS_GET_MAX + 1];
            nud_stats *stats = &mStats;

            memset(stats, 0, sizeof(nud_stats));
            nla_parse(tb_vendor, QCA_ATTR_NUD_STATS_GET_MAX,
                      (struct nlattr *)mVendorData, mDataLen, NULL);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_FROM_NETDEV])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_FROM_NETDEV"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_req_count_from_netdev = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_FROM_NETDEV]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TO_LOWER_MAC])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TO_LOWER_MAC"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_req_count_to_lower_mac = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TO_LOWER_MAC]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_REQ_RX_COUNT_BY_LOWER_MAC])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_REQ_RX_COUNT_BY_LOWER_MAC"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_req_rx_count_by_lower_mac = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_REQ_RX_COUNT_BY_LOWER_MAC]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TX_SUCCESS])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TX_SUCCESS"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_req_count_tx_success = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_REQ_COUNT_TX_SUCCESS]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_LOWER_MAC])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_LOWER_MAC"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_rsp_rx_count_by_lower_mac = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_LOWER_MAC]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_UPPER_MAC])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_UPPER_MAC"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_rsp_rx_count_by_upper_mac = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_RSP_RX_COUNT_BY_UPPER_MAC]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_TO_NETDEV])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_TO_NETDEV"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_rsp_count_to_netdev = nla_get_u16(tb_vendor[
                            QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_TO_NETDEV]);

            if (!tb_vendor[QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_OUT_OF_ORDER_DROP])
            {
                ALOGE("%s: QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_OUT_OF_ORDER_DROP"
                      " not found", __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }
            stats->arp_rsp_count_out_of_order_drop = nla_get_u16(tb_vendor[
                           QCA_ATTR_NUD_STATS_ARP_RSP_COUNT_OUT_OF_ORDER_DROP]);

            if (tb_vendor[QCA_ATTR_NUD_STATS_AP_LINK_ACTIVE])
                stats->ap_link_active = 1;

            if (tb_vendor[QCA_ATTR_NUD_STATS_IS_DAD])
                stats->is_duplicate_addr_detection = 1;

            ALOGV(" req_from_netdev %d count_to_lower :%d"
                  " count_by_lower :%d"
                  " count_tx_succ :%d rsp_count_lower :%d"
                  " rsp_count_upper :%d  rsp_count_netdev :%d"
                  " out_of_order_drop :%d active_aplink %d"
                  " DAD %d ",
                  stats->arp_req_count_from_netdev,
                  stats->arp_req_count_to_lower_mac,
                  stats->arp_req_rx_count_by_lower_mac,
                  stats->arp_req_count_tx_success,
                  stats->arp_rsp_rx_count_by_lower_mac,
                  stats->arp_rsp_rx_count_by_upper_mac,
                  stats->arp_rsp_count_to_netdev,
                  stats->arp_rsp_count_out_of_order_drop,
                  stats->ap_link_active,
                  stats->is_duplicate_addr_detection);
        }
    }
cleanup:
    if (status == WIFI_ERROR_INVALID_ARGS)
       memset(&mStats,0,sizeof(nud_stats));

    return status;
}

void NUDStatsCommand::copyStats(nud_stats *stats)
{
    memcpy(stats, &mStats, sizeof(nud_stats));
}

wifi_error wifi_set_nud_stats(wifi_interface_handle iface, u32 gw_addr)
{
    wifi_error ret;
    NUDStatsCommand *NUDCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);

    ALOGV("gw_addr : %x", gw_addr);
    NUDCommand = NUDStatsCommand::instance(handle);
    if (NUDCommand == NULL) {
        ALOGE("%s: Error NUDStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    NUDCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET);

    /* create the message */
    ret = NUDCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = NUDCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*add the attributes*/
    nl_data = NUDCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data)
        goto cleanup;
    /**/
    ret = NUDCommand->put_flag(QCA_ATTR_NUD_STATS_SET_START);

    ret = NUDCommand->put_u32(QCA_ATTR_NUD_STATS_GW_IPV4, gw_addr);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /**/
    NUDCommand->attr_end(nl_data);

    ret = NUDCommand->requestResponse();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);
    }

cleanup:
    return ret;
}


wifi_error wifi_get_nud_stats(wifi_interface_handle iface,
                              nud_stats *stats)
{
    wifi_error ret;
    NUDStatsCommand *NUDCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);

    if (stats == NULL) {
        ALOGE("%s: Error stats is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    NUDCommand = NUDStatsCommand::instance(handle);
    if (NUDCommand == NULL) {
        ALOGE("%s: Error NUDStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    NUDCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_GET);

    /* create the message */
    ret = NUDCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = NUDCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /*add the attributes*/
    nl_data = NUDCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data)
        goto cleanup;
    /**/
    NUDCommand->attr_end(nl_data);

    ret = NUDCommand->requestResponse();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);
        goto cleanup;
    }

    NUDCommand->copyStats(stats);

cleanup:
    return ret;
}


wifi_error wifi_clear_nud_stats(wifi_interface_handle iface)
{
    wifi_error ret;
    NUDStatsCommand *NUDCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);

    NUDCommand = NUDStatsCommand::instance(handle);
    if (NUDCommand == NULL) {
        ALOGE("%s: Error NUDStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    NUDCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET);

    /* create the message */
    ret = NUDCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = NUDCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*add the attributes*/
    nl_data = NUDCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data)
        goto cleanup;

    NUDCommand->attr_end(nl_data);

    ret = NUDCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);

cleanup:
    return ret;
}
