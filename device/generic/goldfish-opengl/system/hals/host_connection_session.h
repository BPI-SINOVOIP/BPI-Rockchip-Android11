/*
* Copyright (C) 2020 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef GOLDFISH_OPENGL_SYSTEM_HALS_HOST_CONNECTION_SESSION_H_INCLUDED
#define GOLDFISH_OPENGL_SYSTEM_HALS_HOST_CONNECTION_SESSION_H_INCLUDED

#include "HostConnection.h"

class HostConnectionSession {
public:
    explicit HostConnectionSession(HostConnection* hc) : conn(hc) {
        hc->lock();
    }

    ~HostConnectionSession() {
        if (conn) {
            conn->unlock();
        }
     }

    HostConnectionSession(HostConnectionSession&& rhs) : conn(rhs.conn) {
        rhs.conn = nullptr;
    }

    HostConnectionSession& operator=(HostConnectionSession&& rhs) {
        if (this != &rhs) {
            std::swap(conn, rhs.conn);
        }
        return *this;
    }

    HostConnectionSession(const HostConnectionSession&) = delete;
    HostConnectionSession& operator=(const HostConnectionSession&) = delete;

    ExtendedRCEncoderContext* getRcEncoder() const {
        return conn->rcEncoder();
    }

private:
    HostConnection* conn;
};

#endif  // GOLDFISH_OPENGL_SYSTEM_HALS_HOST_CONNECTION_SESSION_H_INCLUDED
