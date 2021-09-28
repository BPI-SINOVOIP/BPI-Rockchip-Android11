/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* this program is used to read a set of system properties and their values
 * from the emulator program and set them in the currently-running emulated
 * system. It does so by connecting to the 'boot-properties' qemud service.
 *
 * This program should be run as root and called from
 * /system/etc/init.goldfish.rc exclusively.
 */

#define LOG_TAG  "qemu-props"

#define DEBUG  1

#if DEBUG
#  include <log/log.h>
#  define  DD(...)    ALOGI(__VA_ARGS__)
#else
#  define  DD(...)    ((void)0)
#endif

#include <cutils/properties.h>
#include <unistd.h>
#include <qemu_pipe_bp.h>
#include <qemud.h>
#include <string.h>
#include <stdio.h>

/* Name of the qemud service we want to connect to.
 */
#define  QEMUD_SERVICE  "boot-properties"

#define  MAX_TRIES      5

#define QEMU_MISC_PIPE "QemuMiscPipe"

int s_QemuMiscPipe = -1;
void static notifyHostBootComplete();
void static sendHeartBeat();
void static sendMessage(const char* mesg);

int  main(void)
{
    int  qemud_fd, count = 0;

    /* try to connect to the qemud service */
    {
        int  tries = MAX_TRIES;

        while (1) {
            qemud_fd = qemud_channel_open( "boot-properties" );
            if (qemud_fd >= 0)
                break;

            if (--tries <= 0) {
                DD("Could not connect after too many tries. Aborting");
                return 1;
            }

            DD("waiting 1s to wait for qemud.");
            sleep(1);
        }
    }

    DD("connected to '%s' qemud service.", QEMUD_SERVICE);

    /* send the 'list' command to the service */
    if (qemud_channel_send(qemud_fd, "list", -1) < 0) {
        DD("could not send command to '%s' service", QEMUD_SERVICE);
        return 1;
    }

    /* read each system property as a single line from the service,
     * until exhaustion.
     */
    for (;;)
    {
#define  BUFF_SIZE   (PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX + 2)
        DD("receiving..");
        char* q;
        char  temp[BUFF_SIZE];
        char  vendortemp[BUFF_SIZE];
        int   len = qemud_channel_recv(qemud_fd, temp, sizeof(temp) - 1);

        /* lone NUL-byte signals end of properties */
        if (len < 0 || len > BUFF_SIZE-1 || temp[0] == '\0')
            break;

        temp[len] = '\0';  /* zero-terminate string */

        DD("received: %.*s", len, temp);

        /* separate propery name from value */
        q = strchr(temp, '=');
        if (q == NULL) {
            DD("invalid format, ignored.");
            continue;
        }
        *q++ = '\0';

        char* final_prop_name = NULL;
        if (strcmp(temp, "qemu.sf.lcd.density") == 0 ) {
            final_prop_name = temp;
        } else if (strcmp(temp, "qemu.hw.mainkeys") == 0 ) {
            final_prop_name = temp;
        } else if (strcmp(temp, "qemu.cmdline") == 0 ) {
            final_prop_name = temp;
        } else if (strcmp(temp, "dalvik.vm.heapsize") == 0 ) {
            continue; /* cannot set it here */
        } else if (strcmp(temp, "ro.opengles.version") == 0 ) {
            continue; /* cannot set it here */
        } else {
            snprintf(vendortemp, sizeof(vendortemp), "vendor.%s", temp);
            final_prop_name = vendortemp;
        }
        if (property_set(temp, q) < 0) {
            ALOGW("could not set property '%s' to '%s'", final_prop_name, q);
        } else {
            ALOGI("successfully set property '%s' to '%s'", final_prop_name, q);
            count += 1;
        }
    }

    close(qemud_fd);

    char temp[BUFF_SIZE];
    sendHeartBeat();
    while (s_QemuMiscPipe >= 0) {
        usleep(5000000); /* 5 seconds */
        sendHeartBeat();
        property_get("vendor.qemu.dev.bootcomplete", temp, "");
        int is_boot_completed = (strncmp(temp, "1", 1) == 0) ? 1 : 0;
        if (is_boot_completed) {
            ALOGI("tell the host boot completed");
            notifyHostBootComplete();
            break;
        }
    }

    while (s_QemuMiscPipe >= 0) {
        usleep(30*1000000); /* 30 seconds */
        sendHeartBeat();
    }

    /* finally, close the channel and exit */
    if (s_QemuMiscPipe >= 0) {
        close(s_QemuMiscPipe);
        s_QemuMiscPipe = -1;
    }
    DD("exiting (%d properties set).", count);
    return 0;
}

void sendHeartBeat() {
    sendMessage("heartbeat");
}

void notifyHostBootComplete() {
    sendMessage("bootcomplete");
}

void sendMessage(const char* mesg) {
   if (s_QemuMiscPipe < 0) {
        s_QemuMiscPipe = qemu_pipe_open_ns(NULL, QEMU_MISC_PIPE, O_RDWR);
        if (s_QemuMiscPipe < 0) {
            ALOGE("failed to open %s", QEMU_MISC_PIPE);
            return;
        }
    }
    char set[64];
    snprintf(set, sizeof(set), "%s", mesg);
    int pipe_command_length = strlen(set)+1; //including trailing '\0'
    qemu_pipe_write_fully(s_QemuMiscPipe, &pipe_command_length, sizeof(pipe_command_length));
    qemu_pipe_write_fully(s_QemuMiscPipe, set, pipe_command_length);
    qemu_pipe_read_fully(s_QemuMiscPipe, &pipe_command_length, sizeof(pipe_command_length));
    if (pipe_command_length > (int)(sizeof(set)) || pipe_command_length <= 0)
        return;
    qemu_pipe_read_fully(s_QemuMiscPipe, set, pipe_command_length);
}
