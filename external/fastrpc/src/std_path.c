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

/*
=======================================================================

FILE:         std_path.c

=======================================================================
=======================================================================
*/

#include "AEEstd.h"
#include "AEEBufBound.h"
#include <string.h>
/*===========================================================================

===========================================================================*/
int std_makepath(const char* cpszDir, const char* cpszFile,
                 char* pszOut, int nOutLen)
{
   BufBound bb;

   BufBound_Init(&bb, pszOut, nOutLen);

   BufBound_Puts(&bb, cpszDir);

   if (('\0' != cpszDir[0]) &&    /* non-empty dir */
       ('/' != cpszDir[std_strlen(cpszDir)-1])) { /* no slash at end of dir */
      BufBound_Putc(&bb, '/');
   }
   if ('/' == cpszFile[0]) {
      cpszFile++;
   }

   BufBound_Puts(&bb, cpszFile);

   BufBound_ForceNullTerm(&bb);

   return BufBound_Wrote(&bb) - 1;

}

/*===========================================================================

===========================================================================*/
char* std_splitpath(const char* cpszPath, const char* cpszDir)
{
   const char* cpsz = cpszPath;

   while ( ! ('\0' == cpszDir[0] ||
              ('/' == cpszDir[0] && '\0' == cpszDir[1])) ){

      if (*cpszDir != *cpsz) {
         return 0;
      }

      ++cpsz;
      ++cpszDir;
   }

   /* Found the filename part of the path.
      It should begin with a '/' unless there is no filename */
   if ('/' == *cpsz) {
      cpsz++;
   }
   else if ('\0' != *cpsz) {
      cpsz = 0;
   }

   return (char*)cpsz;
}

char* std_cleanpath(char* pszPath)
{
   char* pszStart = pszPath;
   char* pc;
   char* pcEnd = pszStart+std_strlen(pszStart);

   /* preserve leading slash */
   if ('/' == pszStart[0]) {
      pszStart++;
   }

   pc = pszStart;

   while ((char*)0 != (pc = std_strstr(pc, "/."))) {
      char* pcDelFrom;

      if ('/' == pc[2] || '\0' == pc[2]) {
         /*  delete "/." */
         pcDelFrom = pc;
         pc += 2;
      } else if ('.' == pc[2] && ('/' == pc[3] || '\0' == pc[3])) {
            /*  delete  "/element/.." */
         pcDelFrom = std_memrchrbegin(pszStart, '/', pc - pszStart);
         pc += 3;
      } else {
         pc += 2;
         continue;
      }

      std_memmove(pcDelFrom, pc, pcEnd-pcDelFrom);

      pc = pcDelFrom;
   }

   /* eliminate leading "../" */
   while (pszStart == std_strstr(pszStart, "../")) {
      std_memmove(pszStart, pszStart+2, pcEnd-pszStart);
   }

   /* eliminate leading "./" */
   while (pszStart == std_strstr(pszStart, "./")) {
      std_memmove(pszStart, pszStart+1, pcEnd-pszStart);
   }

   if (!strncmp(pszStart,"..",2) || !strncmp(pszStart,".",1)) {
      pszStart[0] = '\0';
   }

   /* whack double '/' */
   while ((char*)0 != (pc = std_strstr(pszPath, "//"))) {
      std_memmove(pc, pc+1, pcEnd-pc);
   }

   return pszPath;
}

char* std_basename(const char* cpszFile)
{
   const char* cpsz;

   if ((char*)0 != (cpsz = std_strrchr(cpszFile,'/'))) {
      cpszFile = cpsz+1;
   }

   return (char*)cpszFile;
}
