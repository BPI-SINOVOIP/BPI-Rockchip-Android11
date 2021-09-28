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
#ifndef __WIFI_HAL_CPP_BINDINGS_H__
#define __WIFI_HAL_CPP_BINDINGS_H__

#include "wifi_hal.h"
#include "common.h"
#include "sync.h"

class WifiEvent
{
    /* TODO: remove this when nl headers are updated */
    static const unsigned NL80211_ATTR_MAX_INTERNAL = 256;
private:
    struct nl_msg *mMsg;
    struct genlmsghdr *mHeader;
    struct nlattr *mAttributes[NL80211_ATTR_MAX_INTERNAL + 1];

public:
    WifiEvent(nl_msg *msg) {
        mMsg = msg;
        mHeader = NULL;
        memset(mAttributes, 0, sizeof(mAttributes));
    }
    ~WifiEvent() {
        /* don't destroy mMsg; it doesn't belong to us */
    }

    void log();

    int parse();

    genlmsghdr *header() {
        return mHeader;
    }

    int get_cmd() {
        return mHeader->cmd;
    }

    int get_vendor_id() {
        return get_u32(NL80211_ATTR_VENDOR_ID);
    }

    int get_vendor_subcmd() {
        return get_u32(NL80211_ATTR_VENDOR_SUBCMD);
    }

    void *get_vendor_data() {
        return get_data(NL80211_ATTR_VENDOR_DATA);
    }

    int get_vendor_data_len() {
        return get_len(NL80211_ATTR_VENDOR_DATA);
    }

    const char *get_cmdString();

    nlattr ** attributes() {
        return mAttributes;
    }

    nlattr *get_attribute(int attribute) {
        return mAttributes[attribute];
    }

    uint8_t get_u8(int attribute) {
        return mAttributes[attribute] ? nla_get_u8(mAttributes[attribute]) : 0;
    }

    uint16_t get_u16(int attribute) {
        return mAttributes[attribute] ? nla_get_u16(mAttributes[attribute]) : 0;
    }

    uint32_t get_u32(int attribute) {
        return mAttributes[attribute] ? nla_get_u32(mAttributes[attribute]) : 0;
    }

    uint64_t get_u64(int attribute) {
        return mAttributes[attribute] ? nla_get_u64(mAttributes[attribute]) : 0;
    }

    int get_len(int attribute) {
        return mAttributes[attribute] ? nla_len(mAttributes[attribute]) : 0;
    }

    void *get_data(int attribute) {
        return mAttributes[attribute] ? nla_data(mAttributes[attribute]) : NULL;
    }

private:
    WifiEvent(const WifiEvent&);        // hide copy constructor to prevent copies
};

class nl_iterator {
    struct nlattr *pos;
    int rem;
public:
    nl_iterator(struct nlattr *attr) {
        pos = (struct nlattr *)nla_data(attr);
        rem = nla_len(attr);
    }
    bool has_next() {
        return nla_ok(pos, rem);
    }
    void next() {
        pos = (struct nlattr *)nla_next(pos, &(rem));
    }
    struct nlattr *get() {
        return pos;
    }
    uint16_t get_type() {
        return pos->nla_type;
    }
    uint8_t get_u8() {
        return nla_get_u8(pos);
    }
    uint16_t get_u16() {
        return nla_get_u16(pos);
    }
    uint32_t get_u32() {
        return nla_get_u32(pos);
    }
    uint64_t get_u64() {
        return nla_get_u64(pos);
    }
    void* get_data() {
        return nla_data(pos);
    }
    int get_len() {
        return nla_len(pos);
    }
private:
    nl_iterator(const nl_iterator&);    // hide copy constructor to prevent copies
};

class WifiRequest
{
private:
    int mFamily;
    int mIface;
    struct nl_msg *mMsg;

public:
    WifiRequest(int family) {
        mMsg = NULL;
        mFamily = family;
        mIface = -1;
    }

    WifiRequest(int family, int iface) {
        mMsg = NULL;
        mFamily = family;
        mIface = iface;
    }

    ~WifiRequest() {
        destroy();
    }

    void destroy() {
        if (mMsg) {
            nlmsg_free(mMsg);
            mMsg = NULL;
        }
    }

    nl_msg *getMessage() {
        return mMsg;
    }

    /* Command assembly helpers */
    wifi_error create(int family, uint8_t cmd, int flags, int hdrlen);
    wifi_error create(uint8_t cmd, int flags, int hdrlen) {
        return create(mFamily, cmd, flags, hdrlen);
    }
    wifi_error create(uint8_t cmd) {
        return create(mFamily, cmd, 0, 0);
    }

    wifi_error create(uint32_t id, int subcmd);

    wifi_error wifi_nla_put(struct nl_msg *msg, int attr,
                            int attrlen, const void *data)
    {
        int status;

        status = nla_put(msg, attr, attrlen, data);
	if (status < 0)
            ALOGE("Failed to put attr with size = %d, type = %d, error = %d",
                  attrlen, attr, status);
        return mapKernelErrortoWifiHalError(status);
    }
    wifi_error put_u8(int attribute, uint8_t value) {
        return wifi_nla_put(mMsg, attribute, sizeof(value), &value);
    }
    wifi_error put_u16(int attribute, uint16_t value) {
        return wifi_nla_put(mMsg, attribute, sizeof(value), &value);
    }
    wifi_error put_u32(int attribute, uint32_t value) {
        return wifi_nla_put(mMsg, attribute, sizeof(value), &value);
    }

    wifi_error put_u64(int attribute, uint64_t value) {
        return wifi_nla_put(mMsg, attribute, sizeof(value), &value);
    }

    wifi_error put_s8(int attribute, s8 value) {
        return wifi_nla_put(mMsg, attribute, sizeof(int8_t), &value);
    }
    wifi_error put_s16(int attribute, s16 value) {
        return wifi_nla_put(mMsg, attribute, sizeof(int16_t), &value);
    }
    wifi_error put_s32(int attribute, s32 value) {
        return wifi_nla_put(mMsg, attribute, sizeof(int32_t), &value);
    }
    wifi_error put_s64(int attribute, s64 value) {
        return wifi_nla_put(mMsg, attribute, sizeof(int64_t), &value);
    }
    wifi_error put_flag(int attribute) {
        int status;

        status =  nla_put_flag(mMsg, attribute);
        if(status < 0)
           ALOGE("Failed to put flag attr of type = %d, error = %d",
                  attribute, status);
        return mapKernelErrortoWifiHalError(status);
    }

    u8 get_u8(const struct nlattr *nla)
    {
        return *(u8 *) nla_data(nla);
    }
    u16 get_u16(const struct nlattr *nla)
    {
        return *(u16 *) nla_data(nla);
    }
    u32 get_u32(const struct nlattr *nla)
    {
        return *(u32 *) nla_data(nla);
    }
    u64 get_u64(const struct nlattr *nla)
    {
        return *(u64 *) nla_data(nla);
    }

    s8 get_s8(const struct nlattr *nla)
    {
        return *(s8 *) nla_data(nla);
    }

    s16 get_s16(const struct nlattr *nla)
    {
        return *(s16 *) nla_data(nla);
    }
    s32 get_s32(const struct nlattr *nla)
    {
        return *(s32 *) nla_data(nla);
    }
    s64 get_s64(const struct nlattr *nla)
    {
        return *(s64 *) nla_data(nla);
    }

    wifi_error put_string(int attribute, const char *value) {
        return wifi_nla_put(mMsg, attribute, strlen(value) + 1, value);
    }
    wifi_error put_addr(int attribute, mac_addr value) {
        return wifi_nla_put(mMsg, attribute, sizeof(mac_addr), value);
    }

    struct nlattr * attr_start(int attribute) {
        return nla_nest_start(mMsg, attribute);
    }
    void attr_end(struct nlattr *attr) {
        nla_nest_end(mMsg, attr);
    }

    wifi_error set_iface_id(int ifindex) {
        return put_u32(NL80211_ATTR_IFINDEX, ifindex);
    }

    wifi_error put_bytes(int attribute, const char *data, int len) {
        return wifi_nla_put(mMsg, attribute, len, data);
    }

private:
    WifiRequest(const WifiRequest&);        // hide copy constructor to prevent copies

};

class WifiCommand
{
protected:
    hal_info *mInfo;
    WifiRequest mMsg;
    Condition mCondition;
    wifi_request_id mId;
    interface_info *mIfaceInfo;
public:
    WifiCommand(wifi_handle handle, wifi_request_id id)
            : mMsg(getHalInfo(handle)->nl80211_family_id), mId(id)
    {
        mIfaceInfo = NULL;
        mInfo = getHalInfo(handle);
    }

    WifiCommand(wifi_interface_handle iface, wifi_request_id id)
            : mMsg(getHalInfo(iface)->nl80211_family_id, getIfaceInfo(iface)->id), mId(id)
    {
        mIfaceInfo = getIfaceInfo(iface);
        mInfo = getHalInfo(iface);
    }

    virtual ~WifiCommand() {
    }

    wifi_request_id id() {
        return mId;
    }

    virtual wifi_error create() {
        /* by default there is no way to cancel */
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    virtual wifi_error cancel() {
        /* by default there is no way to cancel */
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    wifi_error requestResponse();
    wifi_error requestEvent(int cmd);
    wifi_error requestVendorEvent(uint32_t id, int subcmd);
    wifi_error requestResponse(WifiRequest& request);

protected:
    wifi_handle wifiHandle() {
        return getWifiHandle(mInfo);
    }

    wifi_interface_handle ifaceHandle() {
        return getIfaceHandle(mIfaceInfo);
    }

    int familyId() {
        return mInfo->nl80211_family_id;
    }

    int ifaceId() {
        return mIfaceInfo->id;
    }

    /* Override this method to parse reply and dig out data; save it in the object */
    virtual int handleResponse(WifiEvent& reply) {
        UNUSED(reply);
        return NL_SKIP;
    }

    /* Override this method to parse event and dig out data; save it in the object */
    virtual int handleEvent(WifiEvent& event) {
        UNUSED(event);
        return NL_SKIP;
    }

    int registerHandler(int cmd) {
        return wifi_register_handler(wifiHandle(), cmd, &event_handler, this);
    }

    void unregisterHandler(int cmd) {
        wifi_unregister_handler(wifiHandle(), cmd);
    }

    wifi_error registerVendorHandler(uint32_t id, int subcmd) {
        return wifi_register_vendor_handler(wifiHandle(), id, subcmd, &event_handler, this);
    }

    void unregisterVendorHandler(uint32_t id, int subcmd) {
        wifi_unregister_vendor_handler(wifiHandle(), id, subcmd);
    }

private:
    WifiCommand(const WifiCommand& );           // hide copy constructor to prevent copies

    /* Event handling */
    static int response_handler(struct nl_msg *msg, void *arg);

    static int event_handler(struct nl_msg *msg, void *arg);

    /* Other event handlers */
    static int valid_handler(struct nl_msg *msg, void *arg);

    static int ack_handler(struct nl_msg *msg, void *arg);

    static int finish_handler(struct nl_msg *msg, void *arg);

    static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg);
};

//WifiVendorCommand class
class WifiVendorCommand: public WifiCommand
{
protected:
    u32 mVendor_id;
    u32 mSubcmd;
    char *mVendorData;
    u32 mDataLen;


public:
    WifiVendorCommand(wifi_handle handle, wifi_request_id id, u32 vendor_id, u32 subcmd);

    virtual ~WifiVendorCommand();

    virtual wifi_error create();

    virtual wifi_error requestResponse();

    virtual wifi_error requestEvent();

    virtual wifi_error put_u8(int attribute, uint8_t value);

    virtual wifi_error put_u16(int attribute, uint16_t value);

    virtual wifi_error put_u32(int attribute, uint32_t value);

    virtual wifi_error put_u64(int attribute, uint64_t value);

    virtual wifi_error put_s8(int attribute, s8 value);

    virtual wifi_error put_s16(int attribute, s16 value);

    virtual wifi_error put_s32(int attribute, s32 value);

    virtual wifi_error put_s64(int attribute, s64 value);

    wifi_error put_flag(int attribute);

    virtual u8 get_u8(const struct nlattr *nla);
    virtual u16 get_u16(const struct nlattr *nla);
    virtual u32 get_u32(const struct nlattr *nla);
    virtual u64 get_u64(const struct nlattr *nla);

    virtual s8 get_s8(const struct nlattr *nla);
    virtual s16 get_s16(const struct nlattr *nla);
    virtual s32 get_s32(const struct nlattr *nla);
    virtual s64 get_s64(const struct nlattr *nla);

    virtual wifi_error put_string(int attribute, const char *value);

    virtual wifi_error put_addr(int attribute, mac_addr value);

    virtual struct nlattr * attr_start(int attribute);

    virtual void attr_end(struct nlattr *attribute);

    virtual wifi_error set_iface_id(const char* name);

    virtual wifi_error put_bytes(int attribute, const char *data, int len);

    virtual wifi_error get_mac_addr(struct nlattr **tb_vendor,
                                int attribute,
                                mac_addr addr);

protected:

    /* Override this method to parse reply and dig out data; save it in the corresponding
       object */
    virtual int handleResponse(WifiEvent &reply);

    /* Override this method to parse event and dig out data; save it in the object */
    virtual int handleEvent(WifiEvent &event);
};

/* nl message processing macros (required to pass C++ type checks) */

#define for_each_attr(pos, nla, rem) \
    for (pos = (nlattr *)nla_data(nla), rem = nla_len(nla); \
        nla_ok(pos, rem); \
        pos = (nlattr *)nla_next(pos, &(rem)))

wifi_error initialize_vendor_cmd(wifi_interface_handle iface,
                                 wifi_request_id id,
                                 u32 subcmd,
                                 WifiVendorCommand **vCommand);
#endif
