/*
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
//#ifndef VERIFY_PRINT_ERROR
//#define VERIFY_PRINT_ERROR
//#endif

#define FARF_ERROR 1

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <string.h>
#include "adsp_default_listener.h"
#include "AEEStdErr.h"
#include "verify.h"
#include "remote.h"
#include "HAP_farf.h"
#include "adspmsgd_adsp.h"

int adsp_default_listener_start(int argc, char* argv[]) {
   struct pollfd pfd;
   eventfd_t event = 0;
   remote_handle fd;
   int nErr = AEE_SUCCESS;
   char *name = NULL;
   int namelen = 0;
   (void)argc;
   (void)argv;
   if (argc > 1) {
      namelen = strlen(ITRANSPORT_PREFIX "createstaticpd:") + strlen(argv[1]);
      name = (char *)malloc((namelen + 1) * sizeof(char));
      VERIFYC(NULL != name, AEE_ENOMEMORY);
      std_strlcpy(name,  ITRANSPORT_PREFIX "createstaticpd:", strlen(ITRANSPORT_PREFIX "createstaticpd:")+1);
      std_strlcat(name, argv[1], namelen+1);
   } else {
      namelen = strlen(ITRANSPORT_PREFIX "attachguestos");
      name = (char *)malloc((namelen + 1) * sizeof(char));
      VERIFYC(NULL != name, AEE_ENOMEMORY);
      std_strlcpy(name,  ITRANSPORT_PREFIX "attachguestos", strlen(ITRANSPORT_PREFIX "attachguestos")+1);
   }
   VERIFY_EPRINTF("adsp_default_listener_start started\n");
   VERIFYC(!setenv("ADSP_LISTENER_MEM_CACHE_SIZE", "1048576", 0), AEE_ESETENV);
   VERIFY(0 == (nErr = remote_handle_open(name, &fd)));
   VERIFY(0 == (nErr = adsp_default_listener_register()));
   VERIFY(0 == (nErr = remote_handle_open(ITRANSPORT_PREFIX "geteventfd",
                                  (remote_handle*)&pfd.fd)));
   free(name);
   name = NULL;
   pfd.events = POLLIN;
   pfd.revents = 0;
#ifdef PD_EXCEPTION_LOGGING

if(argc == 1){
	adspmsgd_adsp_init2();
}

#endif
   while (1) {
     VERIFYC(0 < poll(&pfd, 1, -1), AEE_EPOLL);
     VERIFYC(0 == eventfd_read(pfd.fd, &event), AEE_EEVENTREAD);
     if (event) {
       break;
     }
   }
bail:
#ifdef PD_EXCEPTION_LOGGING
if(argc == 1)
	adspmsgd_adsp_deinit();
#endif
   if(nErr != AEE_SUCCESS) {
      if(name != NULL){
         free(name);
         name = NULL;
      }
      //FARF(ERROR, "Error %x, adsp_default_listener_start exiting\n", nErr);
   }
   return nErr;
}
