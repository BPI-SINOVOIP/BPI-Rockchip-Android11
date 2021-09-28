/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * wpa_supplicant/hostapd control interface library
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name(s) of the above-listed copyright holder(s) nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WIFIHAL_CTRL_H
#define WIFIHAL_CTRL_H

#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include "stdlib.h"

#ifdef ANDROID
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cutils/sockets.h>
#endif /* ANDROID */

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * struct wifihal_ctrl - Internal structure for control interface library
 *
 * This structure is used by the clients to interface with WiFi Hal
 * library to store internal data. Programs using the library should not touch
 * this data directly. They can only use the pointer to the data structure as
 * an identifier for the control interface connection and use this as one of
 * the arguments for most of the control interface library functions.
 */

struct wifihal_ctrl {
    int s;
    struct sockaddr_un local;
    struct sockaddr_un dest;
};

#ifndef CONFIG_CTRL_IFACE_CLIENT_DIR
#define CONFIG_CTRL_IFACE_CLIENT_DIR "/dev/socket/wifihal"
#endif /* CONFIG_CTRL_IFACE_CLIENT_DIR */
#ifndef CONFIG_CTRL_IFACE_CLIENT_PREFIX
#define CONFIG_CTRL_IFACE_CLIENT_PREFIX "wifihal_ctrl_cli_"
#endif /* CONFIG_CTRL_IFACE_CLIENT_PREFIX */

#define DEFAULT_PAGE_SIZE         4096

enum nl_family_type
{
  //! gen netlink family
  GENERIC_NL_FAMILY = 1,
  //! Cld80211 family
  CLD80211_FAMILY
};


enum wifihal_ctrl_cmd
{
    /** attach monitor sock */
    WIFIHAL_CTRL_MONITOR_ATTACH,
    /** dettach monitor sock */
    WIFIHAL_CTRL_MONITOR_DETTACH,
    /** Send data over Netlink Sock */
    WIFIHAL_CTRL_SEND_NL_DATA,
};

//! WIFIHAL Control Request
typedef struct wifihal_ctrl_req_s {
    //! ctrl command
    uint32_t ctrl_cmd;
    //! Family name
    uint32_t family_name;
    //! command ID
    uint32_t cmd_id;
    //! monitor sock len
    uint32_t monsock_len;
    //! monitor sock
    struct sockaddr_un monsock;
    //! data buff length
    uint32_t data_len;
    //! reserved
    uint32_t reserved[4];
    //! data
    char data[0];
}wifihal_ctrl_req_t;


//! WIFIHAL Sync Response
typedef struct wifihal_ctrl_sync_rsp_s {
    //! ctrl command
    uint32_t ctrl_cmd;
    //! Family name
    uint32_t family_name;
    //! command ID
    uint32_t cmd_id;
    //! status for the request
    int status;
    //! reserved
    uint32_t reserved[4];
}wifihal_ctrl_sync_rsp_t;

//! WIFIHAL Async Response
typedef struct wifihal_ctrl_event_s {
    //! Family name
    uint32_t family_name;
    //! command ID
    uint32_t cmd_id;
    //! data buff length
    uint32_t data_len;
    //! reserved
    uint32_t reserved;
    //! data
    char data[0];
}wifihal_ctrl_event_t;

/* WiFi Hal control interface access */

/**
 * wifihal_ctrl_open - Open a control interface to WiFi-Hal
 * @ctrl_path: Path for UNIX domain sockets; ignored if UDP sockets are used.
 * Returns: Pointer to abstract control interface data or %NULL on failure
 *
 * This function is used to open a control interface to WiFi-Hal.
 * ctrl_path is usually /var/run/wifihal. This path
 * is configured in WiFi-Hal and other programs using the control
 * interface need to use matching path configuration.
 */
struct wifihal_ctrl * wifihal_ctrl_open(const char *ctrl_path);

/**
 * wifihal_ctrl_open2 - Open a control interface to wifihal
 * @ctrl_path: Path for UNIX domain sockets; ignored if UDP sockets are used.
 * @cli_path: Path for client UNIX domain sockets; ignored if UDP socket
 *            is used.
 * Returns: Pointer to abstract control interface data or %NULL on failure
 *
 * This function is used to open a control interface to wifihal
 * when the socket path for client need to be specified explicitly. Default
 * ctrl_path is usually /var/run/wifihal and client
 * socket path is /tmp.
 */
struct wifihal_ctrl * wifihal_ctrl_open2(const char *ctrl_path, const char *cli_path);


/**
 * wifihal_ctrl_close - Close a control interface to wifihal
 * @ctrl: Control interface data from wifihal_ctrl_open()
 *
 * This function is used to close a control interface.
 */
void wifihal_ctrl_close(struct wifihal_ctrl *ctrl);


/**
 * wifihal_ctrl_request - Send a command to wifihal
 * @ctrl: Control interface data from wifihal_ctrl_open()
 * @cmd: Command; usually, ASCII text, e.g., "PING"
 * @cmd_len: Length of the cmd in bytes
 * @reply: Buffer for the response
 * @reply_len: Reply buffer length
 * @msg_cb: Callback function for unsolicited messages or %NULL if not used
 * Returns: 0 on success, -1 on error (send or receive failed), -2 on timeout
 *
 * This function is used to send commands to wifihal. Received
 * response will be written to reply and reply_len is set to the actual length
 * of the reply. This function will block for up to two seconds while waiting
 * for the reply. If unsolicited messages are received, the blocking time may
 * be longer.
 *
 * msg_cb can be used to register a callback function that will be called for
 * unsolicited messages received while waiting for the command response. These
 * messages may be received if wifihal_ctrl_request() is called at the same time as
 * wifihal is sending such a message.
 * FIXME : Change the comment below.
 * This can happen only if
 * the program has used wpa_ctrl_attach() to register itself as a monitor for
 * event messages. Alternatively to msg_cb, programs can register two control
 * interface connections and use one of them for commands and the other one for
 * receiving event messages, in other words, call wpa_ctrl_attach() only for
 * the control interface connection that will be used for event messages.
 */
int wifihal_ctrl_request(struct wifihal_ctrl *ctrl, const char *cmd, size_t cmd_len,
                         char *reply, size_t *reply_len);


#ifdef  __cplusplus
}
#endif

#endif
