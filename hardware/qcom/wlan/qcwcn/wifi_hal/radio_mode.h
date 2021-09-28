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

#ifndef __WIFI_HAL_RADIO_MODE_COMMAND_H__
#define __WIFI_HAL_RADIO_MODE_COMMAND_H__

#include "cpp_bindings.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

class RADIOModeCommand: public WifiVendorCommand
{
private:
    static RADIOModeCommand *mRADIOModeCommandInstance;
    wifi_radio_mode_change_handler mHandler;
    wifi_request_id mreqId;
    wifi_mac_info *mwifi_iface_mac_info;
    RADIOModeCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);

public:
    virtual ~RADIOModeCommand();
    static RADIOModeCommand* instance(wifi_handle handle, wifi_request_id id);
    virtual int handleEvent(WifiEvent &event);
    virtual void setReqId(wifi_request_id reqid);
    virtual void setCallbackHandler(wifi_radio_mode_change_handler nHandler);
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
