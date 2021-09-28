/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "proxy_resolver_v8_wrapper.h"
#include "proxy_resolver_v8.h"

using namespace net;

// Implement C interface of ProxyResolverV8
extern "C" {

ProxyResolverV8Handle* ProxyResolverV8Handle_new() {
  ProxyResolverV8* proxyResolver = new ProxyResolverV8(ProxyResolverJSBindings::CreateDefault());
  return reinterpret_cast<ProxyResolverV8Handle*>(proxyResolver);
}

char16_t* ProxyResolverV8Handle_GetProxyForURL(ProxyResolverV8Handle* handle,
                                               const char16_t* spec,
                                               const char16_t* host) {
  ProxyResolverV8* proxyResolver = reinterpret_cast<ProxyResolverV8*>(handle);
  std::u16string specStr(spec);
  std::u16string hostStr(host);
  std::u16string proxies;
  int code = proxyResolver->GetProxyForURL(specStr, hostStr, &proxies);
  if (code != OK) {
    return NULL;
  }
  auto len = proxies.length();
  char16_t* result = (char16_t*) malloc(sizeof(char16_t) * (len + 1));
  if (result == 0) { // Failed to allocate
    return NULL;
  }
  proxies.copy(result, len);
  result[len] = u'\0';
  return result;
}

int ProxyResolverV8Handle_SetPacScript(ProxyResolverV8Handle* handle,
        const char16_t* script_data) {
  ProxyResolverV8* proxyResolver = reinterpret_cast<ProxyResolverV8*>(handle);
  std::u16string script(script_data);
  return proxyResolver->SetPacScript(script);
}

void ProxyResolverV8Handle_delete(ProxyResolverV8Handle* handle) {
  ProxyResolverV8* proxyResolver = reinterpret_cast<ProxyResolverV8*>(handle);
  delete proxyResolver;
}

} // extern "C"