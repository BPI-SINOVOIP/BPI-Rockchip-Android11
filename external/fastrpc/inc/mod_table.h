/**
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MOD_TABLE_H
#define MOD_TABLE_H

#include "remote64.h"
#include "AEEStdDef.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * multi-domain support
 *
 * multi domain modules return remote_handl64 on open/close, but mod_table
 * creates uint32 handles as the "remote" facing handle.  These fit nicely
 * into the transport layer.
 *
 */

/**
  * register a static component for invocations
  * this can be called at any time including from a static constructor
  *
  * name, name of the interface to register
  * pfn, function pointer to the skel invoke function
  *
  * for example:
  *   __attribute__((constructor)) static void my_module_ctor(void) {
  *      mod_table_register_static("my_module", my_module_skel_invoke);
  *   }
  *
  */
int mod_table_register_static(const char* name, int (*pfn)(uint32 sc, remote_arg* pra));

/**
  * same as register_static, but module with user defined handle lifetimes.
  */
int mod_table_register_static1(const char* uri, int (*pfn)(remote_handle64 h,uint32 sc, remote_arg* pra));

/**
  * register a static component for invocations
  * this can be called at any time including from a static constructor
  *
  * overrides will be tried first, then dynamic modules, then regular
  * static modules.  This api should only be use by system components
  * that will never be upgradable.
  *
  * name, name of the interface to register
  * pfn, function pointer to the skel invoke function
  *
  * for example:
  *   __attribute__((constructor)) static void my_module_ctor(void) {
  *      mod_table_register_static("my_module", my_module_skel_invoke);
  *   }
  *
  */
int mod_table_register_static_override(const char* name, int(*pfn)(uint32 sc, remote_arg* pra));

/**
  * same as register_static, but module with user defined handle lifetimes.
  */
int mod_table_register_static_override1(const char* uri, int(*pfn)(remote_handle64,uint32 sc, remote_arg* pra));

/**
 * Open a module and get a handle to it
 *
 * in_name, name of module to open
 * handle, Output handle
 * dlerr, Error String (if an error occurs)
 * dlerrorLen, Length of error String (if an error occurs)
 * pdlErr, Error identifier
 */
int mod_table_open(const char* in_name, remote_handle* handle, char* dlerr, int dlerrorLen, int* pdlErr);

/**
 * invoke a handle in the mod table
 *
 * handle, handle to invoke
 * sc, scalars, see remote.h for documentation.
 * pra, args, see remote.h for documentation.
 */
int mod_table_invoke(remote_handle handle, uint32 sc, remote_arg* pra);

/**
 * Closes a handle in the mod table
 *
 * handle, handle to close
 * errStr, Error String (if an error occurs)
 * errStrLen, Length of error String (if an error occurs)
 * pdlErr, Error identifier
 */
int mod_table_close(remote_handle handle, char* errStr, int errStrLen, int* pdlErr);

/**
 * internal use only
 */
int mod_table_register_const_handle(remote_handle handle, const char* in_name, int (*pfn)(uint32 sc, remote_arg* pra));
/**
 * @param remote, the handle we should expect from the transport layer
 * @param local, the local handle that will be passed to pfn
 */
int mod_table_register_const_handle1(remote_handle remote, remote_handle64 local, const char* uri, int (*pfn)(remote_handle64 h, uint32 sc, remote_arg* pra));

#ifdef __cplusplus
}
#endif

#endif // MOD_TABLE_H
