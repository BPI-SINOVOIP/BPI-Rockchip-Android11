/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WIFI_HAL_LLSTATSCOMMAND_H__
#define __WIFI_HAL_LLSTATSCOMMAND_H__

#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <net/if.h>

#include "nl80211_copy.h"
#include "common.h"
#include "cpp_bindings.h"
#include "link_layer_stats.h"

#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct{
    u32 stats_clear_rsp_mask;
    u8 stop_rsp;
} LLStatsClearRspParams;

typedef struct{
    wifi_iface_stat *iface_stat;
    int num_radios;
    wifi_radio_stat *radio_stat;
} LLStatsResultsParams;

typedef enum{
    eLLStatsSetParamsInvalid = 0,
    eLLStatsClearRspParams,
} eLLStatsRspRarams;

class LLStatsCommand: public WifiVendorCommand
{
private:
    static LLStatsCommand *mLLStatsCommandInstance;

    LLStatsClearRspParams mClearRspParams;

    LLStatsResultsParams mResultsParams;

    wifi_stats_result_handler mHandler;

    wifi_request_id mRequestId;

    u32 mRadioStatsSize;

    u8 mNumRadios;

    LLStatsCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);

public:
    static LLStatsCommand* instance(wifi_handle handle);

    virtual ~LLStatsCommand();

    // This function implements creation of LLStats specific Request
    // based on  the request type
    virtual wifi_error create();

    virtual void setSubCmd(u32 subcmd);

    virtual void initGetContext(u32 reqid);

    virtual wifi_error requestResponse();

    virtual wifi_error notifyResponse();

    virtual int handleResponse(WifiEvent &reply);

    virtual void getClearRspParams(u32 *stats_clear_rsp_mask, u8 *stop_rsp);

    virtual wifi_error get_wifi_iface_stats(wifi_iface_stat *stats,
                                            struct nlattr **tb_vendor);

    virtual void setHandler(wifi_stats_result_handler handler);

    virtual void clearStats();
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
