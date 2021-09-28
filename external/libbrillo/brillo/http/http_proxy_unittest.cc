// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/http/http_proxy.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <brillo/http/http_transport.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;

namespace {
constexpr char kTestUrl[] = "http://www.example.com/test";
}  // namespace

namespace brillo {
namespace http {

class HttpProxyTest : public testing::Test {
 public:
  void ResolveProxyHandlerAsync(dbus::MethodCall* method_call,
                                int timeout_msec,
                                dbus::ObjectProxy::ResponseCallback callback) {
    if (null_dbus_response_) {
      callback.Run(nullptr);
      return;
    }
    callback.Run(CreateDBusResponse(method_call).get());
  }

  dbus::Response* ResolveProxyHandler(dbus::MethodCall* method_call,
                                      int timeout_msec) {
    if (null_dbus_response_) {
      return nullptr;
    }
    // The mock wraps this back into a std::unique_ptr in the function calling
    // us.
    return CreateDBusResponse(method_call).release();
  }

  MOCK_METHOD2(GetProxiesCallback, void(bool, const std::vector<std::string>&));

 protected:
  HttpProxyTest() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), chromeos::kNetworkProxyServiceName,
        dbus::ObjectPath(chromeos::kNetworkProxyServicePath));
    EXPECT_CALL(*bus_, GetObjectProxy(chromeos::kNetworkProxyServiceName,
                                      dbus::ObjectPath(
                                          chromeos::kNetworkProxyServicePath)))
        .WillOnce(Return(object_proxy_.get()));
  }

  std::unique_ptr<dbus::Response> CreateDBusResponse(
      dbus::MethodCall* method_call) {
    EXPECT_EQ(method_call->GetInterface(),
              chromeos::kNetworkProxyServiceInterface);
    EXPECT_EQ(method_call->GetMember(),
              chromeos::kNetworkProxyServiceResolveProxyMethod);
    method_call->SetSerial(1);  // Needs to be non-zero or it fails.
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    writer.AppendString(proxy_info_);
    if (invalid_dbus_response_) {
      return response;
    }
    writer.AppendString(proxy_err_);
    return response;
  }

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> object_proxy_;

  std::string proxy_info_;
  std::string proxy_err_;
  bool null_dbus_response_ = false;
  bool invalid_dbus_response_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpProxyTest);
};

TEST_F(HttpProxyTest, DBusNullResponseFails) {
  std::vector<std::string> proxies;
  null_dbus_response_ = true;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandler));
  EXPECT_FALSE(GetChromeProxyServers(bus_, kTestUrl, &proxies));
}

TEST_F(HttpProxyTest, DBusInvalidResponseFails) {
  std::vector<std::string> proxies;
  invalid_dbus_response_ = true;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandler));
  EXPECT_FALSE(GetChromeProxyServers(bus_, kTestUrl, &proxies));
}

TEST_F(HttpProxyTest, NoProxies) {
  std::vector<std::string> proxies;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandler));
  EXPECT_TRUE(GetChromeProxyServers(bus_, kTestUrl, &proxies));
  EXPECT_THAT(proxies, ElementsAre(kDirectProxy));
}

TEST_F(HttpProxyTest, MultipleProxiesWithoutDirect) {
  proxy_info_ = "proxy example.com; socks5 foo.com;";
  std::vector<std::string> proxies;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandler));
  EXPECT_TRUE(GetChromeProxyServers(bus_, kTestUrl, &proxies));
  EXPECT_THAT(proxies, ElementsAre("http://example.com", "socks5://foo.com",
                                   kDirectProxy));
}

TEST_F(HttpProxyTest, MultipleProxiesWithDirect) {
  proxy_info_ = "socks foo.com; Https example.com ; badproxy example2.com ; "
                "socks5 test.com  ; proxy foobar.com; DIRECT ";
  std::vector<std::string> proxies;
  EXPECT_CALL(*object_proxy_, MockCallMethodAndBlock(_, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandler));
  EXPECT_TRUE(GetChromeProxyServers(bus_, kTestUrl, &proxies));
  EXPECT_THAT(proxies, ElementsAre("socks4://foo.com", "https://example.com",
                                   "socks5://test.com", "http://foobar.com",
                                   kDirectProxy));
}

TEST_F(HttpProxyTest, DBusNullResponseFailsAsync) {
  null_dbus_response_ = true;
  EXPECT_CALL(*object_proxy_, CallMethod(_, _, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandlerAsync));
  EXPECT_CALL(*this, GetProxiesCallback(false, _));
  GetChromeProxyServersAsync(
      bus_, kTestUrl,
      base::Bind(&HttpProxyTest::GetProxiesCallback, base::Unretained(this)));
}

TEST_F(HttpProxyTest, DBusInvalidResponseFailsAsync) {
  invalid_dbus_response_ = true;
  EXPECT_CALL(*object_proxy_, CallMethod(_, _, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandlerAsync));
  EXPECT_CALL(*this, GetProxiesCallback(false, _));
  GetChromeProxyServersAsync(
      bus_, kTestUrl,
      base::Bind(&HttpProxyTest::GetProxiesCallback, base::Unretained(this)));
}

// We don't need to test all the proxy cases with async because that will be
// using the same internal parsing code.
TEST_F(HttpProxyTest, MultipleProxiesWithDirectAsync) {
  proxy_info_ = "socks foo.com; Https example.com ; badproxy example2.com ; "
                "socks5 test.com  ; proxy foobar.com; DIRECT ";
  std::vector<std::string> expected = {
      "socks4://foo.com", "https://example.com", "socks5://test.com",
      "http://foobar.com", kDirectProxy};
  EXPECT_CALL(*object_proxy_, CallMethod(_, _, _))
      .WillOnce(Invoke(this, &HttpProxyTest::ResolveProxyHandlerAsync));
  EXPECT_CALL(*this, GetProxiesCallback(true, expected));
  GetChromeProxyServersAsync(
      bus_, kTestUrl,
      base::Bind(&HttpProxyTest::GetProxiesCallback, base::Unretained(this)));
}

}  // namespace http
}  // namespace brillo
