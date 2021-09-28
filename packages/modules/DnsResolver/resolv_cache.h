/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <unordered_map>
#include <vector>

#include <aidl/android/net/IDnsResolver.h>
#include <aidl/android/net/ResolverOptionsParcel.h>

#include <netdutils/DumpWriter.h>
#include <netdutils/InternetAddresses.h>
#include <stats.pb.h>

#include "ResolverStats.h"
#include "params.h"
#include "stats.h"

// Sets the name server addresses to the provided ResState.
// The name servers are retrieved from the cache which is associated
// with the network to which ResState is associated.
struct ResState;

typedef std::multimap<std::string /* hostname */, std::string /* IPv4/IPv6 address */> HostMapping;

void resolv_populate_res_for_net(ResState* statp);

std::vector<unsigned> resolv_list_caches();

std::vector<std::string> resolv_cache_dump_subsampling_map(unsigned netid);
uint32_t resolv_cache_get_subsampling_denom(unsigned netid, int return_code);

typedef enum {
    RESOLV_CACHE_UNSUPPORTED, /* the cache can't handle that kind of queries */
                              /* or the answer buffer is too small */
    RESOLV_CACHE_NOTFOUND,    /* the cache doesn't know about this query */
    RESOLV_CACHE_FOUND,       /* the cache found the answer */
    RESOLV_CACHE_SKIP         /* Don't do anything on cache */
} ResolvCacheStatus;

ResolvCacheStatus resolv_cache_lookup(unsigned netid, const void* query, int querylen, void* answer,
                                      int answersize, int* answerlen, uint32_t flags);

// add a (query,answer) to the cache. If the pair has been in the cache, no new entry will be added
// in the cache.
int resolv_cache_add(unsigned netid, const void* query, int querylen, const void* answer,
                     int answerlen);

/* Notify the cache a request failed */
void _resolv_cache_query_failed(unsigned netid, const void* query, int querylen, uint32_t flags);

// Get a customized table for a given network.
std::vector<std::string> getCustomizedTableByName(const size_t netid, const char* hostname);

// Sets name servers for a given network.
// TODO: Pass all of ResolverParamsParcel and remove the res_params argument.
int resolv_set_nameservers(unsigned netid, const std::vector<std::string>& servers,
                           const std::vector<std::string>& domains, const res_params& params,
                           const aidl::android::net::ResolverOptionsParcel& resolverOptions =
                                   {{} /* hosts */,
                                    aidl::android::net::IDnsResolver::TC_MODE_DEFAULT,
                                    false /* enforceDnsUid */},
                           const std::vector<int32_t>& transportTypes = {});

// Creates the cache associated with the given network.
int resolv_create_cache_for_net(unsigned netid);

// Deletes the cache associated with the given network.
void resolv_delete_cache_for_net(unsigned netid);

// Flushes the cache associated with the given network.
int resolv_flush_cache_for_net(unsigned netid);

// Get transport types to a given network.
android::net::NetworkType resolv_get_network_types_for_net(unsigned netid);

// For test only.
// Return true if the cache is existent in the given network, false otherwise.
bool has_named_cache(unsigned netid);

// For test only.
// Get the expiration time of a cache entry. Return 0 on success; otherwise, an negative error is
// returned if the expiration time can't be acquired.
int resolv_cache_get_expiration(unsigned netid, const std::vector<char>& query, time_t* expiration);

// Set private DNS servers to DnsStats for a given network.
int resolv_stats_set_servers_for_dot(unsigned netid, const std::vector<std::string>& servers);

// Add a statistics record to DnsStats for a given network.
bool resolv_stats_add(unsigned netid, const android::netdutils::IPSockAddr& server,
                      const android::net::DnsQueryEvent* record);

/* Retrieve a local copy of the stats for the given netid. The buffer must have space for
 * MAXNS __resolver_stats. Returns the revision id of the resolvers used.
 */
int resolv_cache_get_resolver_stats(
        unsigned netid, res_params* params, res_stats stats[MAXNS],
        const std::vector<android::netdutils::IPSockAddr>& serverSockAddrs);

/* Add a sample to the shared struct for the given netid and server, provided that the
 * revision_id of the stored servers has not changed.
 */
void resolv_cache_add_resolver_stats_sample(unsigned netid, int revision_id,
                                            const android::netdutils::IPSockAddr& serverSockAddr,
                                            const res_sample& sample, int max_samples);

// Convert TRANSPORT_* to NT_*. It's public only for unit testing.
android::net::NetworkType convert_network_type(const std::vector<int32_t>& transportTypes);

// Dump net configuration log for a given network.
void resolv_netconfig_dump(android::netdutils::DumpWriter& dw, unsigned netid);
