// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_HTTP_HTTP_PROXY_H_
#define LIBBRILLO_BRILLO_HTTP_HTTP_PROXY_H_

#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/memory/ref_counted.h>
#include <brillo/brillo_export.h>

namespace dbus {
class Bus;
}  // namespace dbus

namespace brillo {
namespace http {

using GetChromeProxyServersCallback =
    base::Callback<void(bool success, const std::vector<std::string>& proxies)>;

// Gets the list of proxy servers that are configured in Chrome by sending a
// D-Bus message to Chrome. The list will always be at least one in size, with
// the last element always being the direct option. The target URL should be
// passed in for the |url| parameter. The proxy servers are set in
// |proxies_out|. The format of the strings in |proxies_out| is
// scheme://[[user:pass@]host:port] with the last element being "direct://".
// Valid schemes it will return are socks4, socks5, http, https or direct.
// Even if this function returns false, it will still set |proxies_out| to be
// just the direct proxy. This function will only return false if there is an
// error in the D-Bus communication itself.
BRILLO_EXPORT bool GetChromeProxyServers(scoped_refptr<dbus::Bus> bus,
                                         const std::string& url,
                                         std::vector<std::string>* proxies_out);

// Async version of GetChromeProxyServers.
BRILLO_EXPORT void GetChromeProxyServersAsync(
    scoped_refptr<dbus::Bus> bus,
    const std::string& url,
    const GetChromeProxyServersCallback& callback);

}  // namespace http
}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_HTTP_HTTP_PROXY_H_
