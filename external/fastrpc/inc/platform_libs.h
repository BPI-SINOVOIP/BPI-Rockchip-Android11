#ifndef PLATFORM_LIBS_H
#define PLATFORM_LIBS_H

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

/*
Platfrom Library Loader
-----------------------

proj/platform_libs is a library to help manage interdependencies between
other libraries.


to use you will need to add

incs = { 'proj/platform_libs' }

in source of the library implementation define the entry point

#include "platform_libs.h"
PL_DEFINE(svfs, svfs_init, svfs_deinit);

Platfrom Library List
---------------------

This list contains top level libraries to be initialized at boot.  To make
sure that this library is loaded you would need to call PLRegister or
PLRegisterDefault in your scons build command.


Handling Interdependencies and Order
------------------------------------

To make sure that library A is loaded before library B, library B's
constructor and destructor should initialize and cleanup libraryB.

#include "platform_libs.h"

PL_DEP(libraryB)
void libraryA_deinit(void) {
 PL_DEINIT(libraryB);
}

int libraryA_init(void) {
 int nErr = 0;
 // my init code
 VERIFY(0 == PL_INIT(libraryB));
bail:
 if(nErr) { libraryA_deinit() }
 return nErr;
}


libraryB does not need to appear in the platform library list.
*/

#include <stdint.h>

struct platform_lib {
  const char* name;
  uint32_t uRefs;
  int nErr;
  int (*init)(void);
  void (*deinit)(void);
};

/**
 * use this macro to pull in external dependencies
 */
#ifdef __cplusplus
#define PL_DEP(name)\
   extern "C" {\
     extern struct platform_lib*  _pl_##name(void); \
   }
#else
#define PL_DEP(name)\
   extern struct platform_lib*  _pl_##name(void);
#endif /* __cplusplus */

/**
 * should be declared in source in the library
 * if constructor fails, destructor is not called
 */
#ifdef __cplusplus
#define PL_DEFINE(name, init, deinit) \
   extern "C" {\
      struct platform_lib* _pl_##name(void) {\
         static struct platform_lib  _gpl_##name = { #name, 0, -1, init, deinit };\
         return &_gpl_##name;\
      }\
   }
#else
#define PL_DEFINE(name, init, deinit) \
   struct platform_lib* _pl_##name(void) {\
      static struct platform_lib  _gpl_##name = { #name, 0, -1, init, deinit };\
      return &_gpl_##name;\
   }
#endif /* __cplusplus */

/**
 * should be added to platform_libs_list.c pl_list table
 * for all top level modules
 */
#define PL_ENTRY(name) _pl_##name

/**
 * should be called within a constructor to ensure that a
 * dependency has been initialized.  so if foo depends on bar
 *
 *    #include "bar.h"
 *    int foo_init() {
 *       return PL_INIT(bar);
 *    }
 */
#define PL_INIT(name) pl_lib_init(PL_ENTRY(name))

/**
 * should be called within a destructor to ensure that a
 * dependency has been cleaned up
 */
#define PL_DEINIT(name) pl_lib_deinit(PL_ENTRY(name))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * initalize the static list of library constructors and destructors
 * should be called from main()
 *
 * see platform_libs_list.h to add high level libraries
 *
 */
int pl_init(void);

/**
 * calls all the destructors.
 */
void pl_deinit(void);

/**
 * initialize a single library.  called via PL_INIT
 */
int pl_lib_init(struct platform_lib* (*pl)(void));

/**
 * deinitialize a single library called via PL_DEINIT
 */
void pl_lib_deinit(struct platform_lib* (*pl)(void));

#ifdef __cplusplus
}
#endif

#endif //PLATFORM_LIBS
