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

#ifndef NET_PROXY_PROXY_RESOLVER_V8_WRAPPER_H_
#define NET_PROXY_PROXY_RESOLVER_V8_WRAPPER_H_

// This header should be compatible with C or C++ compiler
#ifdef __cplusplus
extern "C" {
#endif

#define OK 0
#define ERR_PAC_SCRIPT_FAILED -1
#define ERR_FAILED -2

/**
* Declare an incomplete type ProxyResolverV8Handle. A pointer of this should always be
* either null or pointing to a C++ ProxyResolverV8 object.
*/
typedef void* ProxyResolverV8Handle;

/**
 * Create a ProxyResolverV8Handle. The caller must call ProxyResolverV8Handle_delete to release
 * the memory.
 */
ProxyResolverV8Handle* ProxyResolverV8Handle_new();

/**
 * @return the result in char16_t array if the run is successful. The result contains a list of
 *         proxies according to the PAC specification. Otherwise, return NULL if the run fails.
 *         The memory is allocated with malloc() and the caller should use free() to release it
 *         after use.
 */
char16_t* ProxyResolverV8Handle_GetProxyForURL(ProxyResolverV8Handle* handle,
        const char16_t* spec, const char16_t* host);
/**
 * @return OK if setting the pac script successfully.
 */
int ProxyResolverV8Handle_SetPacScript(ProxyResolverV8Handle* handle,
        const char16_t* script_data);

void ProxyResolverV8Handle_delete(ProxyResolverV8Handle* handle);

#ifdef __cplusplus
}
#endif

#endif // NET_PROXY_PROXY_RESOLVER_V8_WRAPPER_H_