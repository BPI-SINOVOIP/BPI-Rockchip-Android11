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

#include "wifi_hal_ctrl.h"

struct wifihal_ctrl * wifihal_ctrl_open2(const char *ctrl_path,
                                         const char *cli_path)
{
    struct wifihal_ctrl *ctrl;
    static int counter = 0;
    int retval;
    size_t res;
    int tries = 0;
    int flags;
#ifdef ANDROID
    struct group *grp_wifi;
    gid_t gid_wifi;
    struct passwd *pwd_system;
    uid_t uid_system;
#endif

    if (ctrl_path == NULL)
        return NULL;

    ctrl = malloc(sizeof(*ctrl));
    if (ctrl == NULL) {
        return NULL;
    }

    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (ctrl->s < 0) {
         free(ctrl);
         return NULL;
    }

    ctrl->local.sun_family = AF_UNIX;

try_again:
    if (cli_path && cli_path[0] == '/') {
       res = strlcpy(ctrl->local.sun_path, cli_path,
                     sizeof(ctrl->local.sun_path));

       if (res >= sizeof(ctrl->local.sun_path)) {
             close(ctrl->s);
             free(ctrl);
             return NULL;
         }

    } else {
       counter++;
       retval = snprintf(ctrl->local.sun_path,
                      sizeof(ctrl->local.sun_path),
                      CONFIG_CTRL_IFACE_CLIENT_DIR "/"
                      CONFIG_CTRL_IFACE_CLIENT_PREFIX "%d-%d",
                      (int) getpid(), counter);
    }
    tries++;
    if (bind(ctrl->s, (struct sockaddr *) &ctrl->local,
             sizeof(ctrl->local)) < 0) {
       if (errno == EADDRINUSE && tries < 2) {
           /*
            * getpid() returns unique identifier for this instance
            * of wifihal_ctrl, so the existing socket file must have
            * been left by unclean termination of an earlier run.
            * Remove the file and try again.
            */
           unlink(ctrl->local.sun_path);
           goto try_again;
        }
       close(ctrl->s);
       free(ctrl);
       return NULL;
    }

#ifdef ANDROID
    chmod(ctrl->local.sun_path, S_IRWXU | S_IRWXG );

    /* Set group even if we do not have privileges to change owner */
    grp_wifi = getgrnam("wifi");
    gid_wifi = grp_wifi ? grp_wifi->gr_gid : 0;
    pwd_system = getpwnam("system");
    uid_system = pwd_system ? pwd_system->pw_uid : 0;
    if (!gid_wifi || !uid_system) {
        close(ctrl->s);
        unlink(ctrl->local.sun_path);
        free(ctrl);
        return NULL;
    }
    chown(ctrl->local.sun_path, -1, gid_wifi);
    chown(ctrl->local.sun_path, uid_system, gid_wifi);


    if (*ctrl_path != '/') {
            free(ctrl);
            return NULL;
           }
#endif /* ANDROID */

       ctrl->dest.sun_family = AF_UNIX;
       res = strlcpy(ctrl->dest.sun_path, ctrl_path,
                     sizeof(ctrl->dest.sun_path));
       if (res >= sizeof(ctrl->dest.sun_path)) {
           close(ctrl->s);
           free(ctrl);
           return NULL;
       }
       if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest,
            sizeof(ctrl->dest)) < 0) {
        close(ctrl->s);
        unlink(ctrl->local.sun_path);
        free(ctrl);
        return NULL;
       }
       /*
        * Make socket non-blocking so that we don't hang forever if
        * target dies unexpectedly.
        */
       flags = fcntl(ctrl->s, F_GETFL);
       if (flags >= 0) {
        flags |= O_NONBLOCK;
        if (fcntl(ctrl->s, F_SETFL, flags) < 0) {
             perror("fcntl(ctrl->s, O_NONBLOCK)");
             /* Not fatal, continue on.*/
        }
       }
       return ctrl;
}

struct wifihal_ctrl * wifihal_ctrl_open(const char *ctrl_path)
{
    return wifihal_ctrl_open2(ctrl_path, NULL);
}

void wifihal_ctrl_close(struct wifihal_ctrl *ctrl)
{
    if (ctrl == NULL)
        return;
    unlink(ctrl->local.sun_path);
    if (ctrl->s >= 0)
        close(ctrl->s);
    free(ctrl);
}

int wifihal_ctrl_request(struct wifihal_ctrl *ctrl, const char *cmd, size_t cmd_len,
                         char *reply, size_t *reply_len)
{
    struct timeval tv;
    int counter = 0, res;
    fd_set rfds;
    const char *_cmd;
    size_t _cmd_len;
    char *cmd_buf = NULL;

    _cmd = cmd;
    _cmd_len = cmd_len;

    errno = 0;
retry_send:
    if (sendto(ctrl->s, _cmd, _cmd_len, 0, (struct sockaddr *)&ctrl->dest, sizeof(ctrl->dest)) < 0) {
        if (errno == EAGAIN || errno == EBUSY || errno == EWOULDBLOCK)
        {
          /*
           * Must be a non-blocking socket... Try for a bit
           * longer before giving up.
           */
              if(counter == 5) {
                 goto send_err;
                } else {
                 counter++;
                }
                sleep(1);
                goto retry_send;
         }
         send_err:
         free(cmd_buf);
         return -1;
     }
     free(cmd_buf);

     for (;;) {
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(ctrl->s, &rfds);
        res = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
        if (res < 0 && errno == EINTR)
            continue;
        if (res < 0)
            return res;
        if (FD_ISSET(ctrl->s, &rfds)) {
                res = recv(ctrl->s, reply, *reply_len, 0);
                if (res < 0)
                    return res;
                *reply_len = res;
                break;
        } else {
           return -2;
        }
     }
     return 0;
}
