/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "resolv"

#include "DnsTlsTransport.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdutils/ThreadUtil.h>

#include "DnsTlsSocketFactory.h"
#include "IDnsTlsSocketFactory.h"

using android::base::StringPrintf;
using android::netdutils::setThreadName;

namespace android {
namespace net {

std::future<DnsTlsTransport::Result> DnsTlsTransport::query(const netdutils::Slice query) {
    std::lock_guard guard(mLock);

    auto record = mQueries.recordQuery(query);
    if (!record) {
        return std::async(std::launch::deferred, []{
            return (Result) { .code = Response::internal_error };
        });
    }

    if (!mSocket) {
        LOG(DEBUG) << "No socket for query.  Opening socket and sending.";
        doConnect();
    } else {
        sendQuery(record->query);
    }

    return std::move(record->result);
}

int DnsTlsTransport::getConnectCounter() const {
    std::lock_guard guard(mLock);
    return mConnectCounter;
}

bool DnsTlsTransport::sendQuery(const DnsTlsQueryMap::Query& q) {
    // Strip off the ID number and send the new ID instead.
    const bool sent = mSocket->query(q.newId, netdutils::drop(netdutils::makeSlice(q.query), 2));
    if (sent) {
        mQueries.markTried(q.newId);
    }
    return sent;
}

void DnsTlsTransport::doConnect() {
    LOG(DEBUG) << "Constructing new socket";
    mSocket = mFactory->createDnsTlsSocket(mServer, mMark, this, &mCache);
    mConnectCounter++;

    if (mSocket) {
        auto queries = mQueries.getAll();
        LOG(DEBUG) << "Initialization succeeded.  Reissuing " << queries.size() << " queries.";
        for(auto& q : queries) {
            if (!sendQuery(q)) {
                break;
            }
        }
    } else {
        LOG(DEBUG) << "Initialization failed.";
        mSocket.reset();
        LOG(DEBUG) << "Failing all pending queries.";
        mQueries.clear();
    }
}

void DnsTlsTransport::onResponse(std::vector<uint8_t> response) {
    mQueries.onResponse(std::move(response));
}

void DnsTlsTransport::onClosed() {
    std::lock_guard guard(mLock);
    if (mClosing) {
        return;
    }
    // Move remaining operations to a new thread.
    // This is necessary because
    // 1. onClosed is currently running on a thread that blocks mSocket's destructor
    // 2. doReconnect will call that destructor
    if (mReconnectThread) {
        // Complete cleanup of a previous reconnect thread, if present.
        mReconnectThread->join();
        // Joining a thread that is trying to acquire mLock, while holding mLock,
        // looks like it risks a deadlock.  However, a deadlock will not occur because
        // once onClosed is called, it cannot be called again until after doReconnect
        // acquires mLock.
    }
    mReconnectThread.reset(new std::thread(&DnsTlsTransport::doReconnect, this));
}

void DnsTlsTransport::doReconnect() {
    std::lock_guard guard(mLock);
    setThreadName(StringPrintf("TlsReconn_%u", mMark & 0xffff).c_str());
    if (mClosing) {
        return;
    }
    mQueries.cleanup();
    if (!mQueries.empty()) {
        LOG(DEBUG) << "Fast reconnect to retry remaining queries";
        doConnect();
    } else {
        LOG(DEBUG) << "No pending queries.  Going idle.";
        mSocket.reset();
    }
}

DnsTlsTransport::~DnsTlsTransport() {
    LOG(DEBUG) << "Destructor";
    {
        std::lock_guard guard(mLock);
        LOG(DEBUG) << "Locked destruction procedure";
        mQueries.clear();
        mClosing = true;
    }
    // It's possible that a reconnect thread was spawned and waiting for mLock.
    // It's safe for that thread to run now because mClosing is true (and mQueries is empty),
    // but we need to wait for it to finish before allowing destruction to proceed.
    if (mReconnectThread) {
        LOG(DEBUG) << "Waiting for reconnect thread to terminate";
        mReconnectThread->join();
        mReconnectThread.reset();
    }
    // Ensure that the socket is destroyed, and can clean up its callback threads,
    // before any of this object's fields become invalid.
    mSocket.reset();
    LOG(DEBUG) << "Destructor completed";
}

// static
// TODO: Use this function to preheat the session cache.
// That may require moving it to DnsTlsDispatcher.
bool DnsTlsTransport::validate(const DnsTlsServer& server, unsigned netid, uint32_t mark) {
    LOG(DEBUG) << "Beginning validation on " << netid;
    // Generate "<random>-dnsotls-ds.metric.gstatic.com", which we will lookup through |ss| in
    // order to prove that it is actually a working DNS over TLS server.
    static const char kDnsSafeChars[] =
            "abcdefhijklmnopqrstuvwxyz"
            "ABCDEFHIJKLMNOPQRSTUVWXYZ"
            "0123456789";
    const auto c = [](uint8_t rnd) -> uint8_t {
        return kDnsSafeChars[(rnd % std::size(kDnsSafeChars))];
    };
    uint8_t rnd[8];
    arc4random_buf(rnd, std::size(rnd));
    // We could try to use res_mkquery() here, but it's basically the same.
    uint8_t query[] = {
        rnd[6], rnd[7],  // [0-1]   query ID
        1, 0,  // [2-3]   flags; query[2] = 1 for recursion desired (RD).
        0, 1,  // [4-5]   QDCOUNT (number of queries)
        0, 0,  // [6-7]   ANCOUNT (number of answers)
        0, 0,  // [8-9]   NSCOUNT (number of name server records)
        0, 0,  // [10-11] ARCOUNT (number of additional records)
        17, c(rnd[0]), c(rnd[1]), c(rnd[2]), c(rnd[3]), c(rnd[4]), c(rnd[5]),
            '-', 'd', 'n', 's', 'o', 't', 'l', 's', '-', 'd', 's',
        6, 'm', 'e', 't', 'r', 'i', 'c',
        7, 'g', 's', 't', 'a', 't', 'i', 'c',
        3, 'c', 'o', 'm',
        0,  // null terminator of FQDN (root TLD)
        0, ns_t_aaaa,  // QTYPE
        0, ns_c_in     // QCLASS
    };
    const int qlen = std::size(query);

    int replylen = 0;
    DnsTlsSocketFactory factory;
    DnsTlsTransport transport(server, mark, &factory);
    auto r = transport.query(netdutils::Slice(query, qlen)).get();
    if (r.code != Response::success) {
        LOG(DEBUG) << "query failed";
        return false;
    }

    const std::vector<uint8_t>& recvbuf = r.response;
    if (recvbuf.size() < NS_HFIXEDSZ) {
        LOG(WARNING) << "short response: " << replylen;
        return false;
    }

    const int qdcount = (recvbuf[4] << 8) | recvbuf[5];
    if (qdcount != 1) {
        LOG(WARNING) << "reply query count != 1: " << qdcount;
        return false;
    }

    const int ancount = (recvbuf[6] << 8) | recvbuf[7];
    LOG(DEBUG) << netid << " answer count: " << ancount;

    // TODO: Further validate the response contents (check for valid AAAA record, ...).
    // Note that currently, integration tests rely on this function accepting a
    // response with zero records.

    return true;
}

}  // end of namespace net
}  // end of namespace android
