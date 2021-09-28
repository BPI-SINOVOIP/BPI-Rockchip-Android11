// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/http/http_proxy.h>

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>
#include <brillo/http/http_transport.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace {
bool ParseProxyInfo(dbus::Response* response,
                    std::vector<std::string>* proxies_out) {
  DCHECK(proxies_out);
  if (!response) {
    LOG(ERROR) << chromeos::kNetworkProxyServiceName << " D-Bus call to "
               << chromeos::kNetworkProxyServiceResolveProxyMethod
               << " failed";
    proxies_out->assign({brillo::http::kDirectProxy});
    return false;
  }
  dbus::MessageReader reader(response);
  std::string proxy_info;
  std::string proxy_err;
  if (!reader.PopString(&proxy_info) || !reader.PopString(&proxy_err)) {
    LOG(ERROR) << chromeos::kNetworkProxyServiceName << " D-Bus call to "
               << chromeos::kNetworkProxyServiceResolveProxyMethod
               << " returned an invalid D-Bus response";
    proxies_out->assign({brillo::http::kDirectProxy});
    return false;
  }
  if (!proxy_err.empty()) {
    // This case occurs when on the Chrome side of things it can't connect to
    // the proxy resolver service, we just let this fall through and will end
    // up returning success with only the direct proxy listed.
    LOG(WARNING) << "Got error resolving proxy: " << proxy_err;
  }

  base::StringTokenizer toker(proxy_info, ";");
  while (toker.GetNext()) {
    std::string token = toker.token();
    base::TrimWhitespaceASCII(token, base::TRIM_ALL, &token);

    // Start by finding the first space (if any).
    std::string::iterator space;
    for (space = ++token.begin(); space != token.end(); ++space) {
      if (base::IsAsciiWhitespace(*space)) {
        break;
      }
    }

    std::string scheme = base::ToLowerASCII(std::string(token.begin(), space));
    // Chrome uses "socks" to mean socks4 and "proxy" to mean http.
    if (scheme == "socks") {
      scheme += "4";
    } else if (scheme == "proxy") {
      scheme = "http";
    } else if (scheme != "https" && scheme != "socks4" && scheme != "socks5" &&
               scheme != "direct") {
      LOG(ERROR) << "Invalid proxy scheme found of: " << scheme;
      continue;
    }

    std::string host_and_port = std::string(space, token.end());
    base::TrimWhitespaceASCII(host_and_port, base::TRIM_ALL, &host_and_port);
    if (scheme != "direct" && host_and_port.empty()) {
      LOG(ERROR) << "Invalid host/port information for proxy: " << token;
      continue;
    }
    proxies_out->push_back(scheme + "://" + host_and_port);
  }
  // Always add the direct proxy (i.e. no proxy) as a last resort if not there.
  if (proxies_out->empty() ||
      proxies_out->back() != brillo::http::kDirectProxy) {
    proxies_out->push_back(brillo::http::kDirectProxy);
  }
  return true;
}

void OnResolveProxy(const brillo::http::GetChromeProxyServersCallback& callback,
                    dbus::Response* response) {
  std::vector<std::string> proxies;
  bool result = ParseProxyInfo(response, &proxies);
  callback.Run(result, std::move(proxies));
}
}  // namespace

namespace brillo {
namespace http {

bool GetChromeProxyServers(scoped_refptr<dbus::Bus> bus, const std::string& url,
                           std::vector<std::string>* proxies_out) {
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(chromeos::kNetworkProxyServiceName,
                          dbus::ObjectPath(chromeos::kNetworkProxyServicePath));
  dbus::MethodCall method_call(
      chromeos::kNetworkProxyServiceInterface,
      chromeos::kNetworkProxyServiceResolveProxyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(url);
  std::unique_ptr<dbus::Response> response = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  return ParseProxyInfo(response.get(), proxies_out);
}

void GetChromeProxyServersAsync(scoped_refptr<dbus::Bus> bus,
                                const std::string& url,
                                const GetChromeProxyServersCallback& callback) {
  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      chromeos::kNetworkProxyServiceName,
      dbus::ObjectPath(chromeos::kNetworkProxyServicePath));
  dbus::MethodCall method_call(
      chromeos::kNetworkProxyServiceInterface,
      chromeos::kNetworkProxyServiceResolveProxyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(url);
  proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                    base::Bind(&OnResolveProxy, callback));
}

}  // namespace http
}  // namespace brillo
