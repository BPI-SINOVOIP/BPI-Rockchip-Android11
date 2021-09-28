// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/http/http_transport.h>

#include <brillo/http/http_transport_curl.h>

namespace brillo {
namespace http {

const char kErrorDomain[] = "http_transport";
const char kDirectProxy[] = "direct://";

std::shared_ptr<Transport> Transport::CreateDefault() {
  return std::make_shared<http::curl::Transport>(std::make_shared<CurlApi>());
}

std::shared_ptr<Transport> Transport::CreateDefaultWithProxy(
    const std::string& proxy) {
  if (proxy.empty() || proxy == kDirectProxy) {
    return CreateDefault();
  } else {
    return std::make_shared<http::curl::Transport>(std::make_shared<CurlApi>(),
                                                   proxy);
  }
}

}  // namespace http
}  // namespace brillo
