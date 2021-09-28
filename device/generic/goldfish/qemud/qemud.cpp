/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <qemud.h>
#include <qemu_pipe_bp.h>
#include <unistd.h>

int qemud_channel_open(const char*  name) {
    return qemu_pipe_open_ns("qemud", name, O_RDWR);
}

int qemud_channel_send(int pipe, const void* msg, int size) {
    char header[5];

    if (size < 0)
        size = strlen((const char*)msg);

    if (size == 0)
        return 0;

    snprintf(header, sizeof(header), "%04x", size);
    if (qemu_pipe_write_fully(pipe, header, 4)) {
        return -1;
    }

    if (qemu_pipe_write_fully(pipe, msg, size)) {
        return -1;
    }

    return 0;
}

int qemud_channel_recv(int pipe, void* msg, int maxsize) {
    char header[5];
    int  size;

    if (qemu_pipe_read_fully(pipe, header, 4)) {
        return -1;
    }
    header[4] = 0;

    if (sscanf(header, "%04x", &size) != 1) {
        return -1;
    }
    if (size > maxsize) {
        return -1;
    }

    if (qemu_pipe_read_fully(pipe, msg, size)) {
        return -1;
    }

    return size;
}
