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
#include "AEEstd.h"
#include "AEEsmath.h"



static int xMinSize(int a, int b)
{
   if (b < a) {
      a = b;
   }
   return (a >= 0 ? a : 0);
}


static void xMoveBytes(byte *pbDest, const byte *pbSrc, int cb)
{
   if (pbDest != pbSrc) {
      (void) std_memmove(pbDest, pbSrc, cb);
   }
}


#ifdef AEE_BIGENDIAN
#  define STD_COPY       std_CopyBE
#  define STD_COPY_SWAP  std_CopyLE
#else
#  define STD_COPY       std_CopyLE
#  define STD_COPY_SWAP  std_CopyBE
#endif


// See std_CopyLE/BE for documentation.  This function implements the case
// where host ordering != target byte ordering.
//
int STD_COPY_SWAP(void *      pvDest, int nDestSize,
                  const void *pvSrc,  int nSrcSize,
                  const char *pszFields)
{
   byte* pbDest = (byte*)pvDest;
   byte* pbSrc  = (byte*)pvSrc;
   int cbCopied = xMinSize(nDestSize, nSrcSize);
   const char * pszNextField;
   int cb, nSize;

   nSize = 0;  // avoid warning when using RVCT2.2 with -O1

   pszNextField = pszFields;

   for (cb = cbCopied; cb > 0; cb -= nSize) {
      char  ch;

      ch = *pszNextField++;
      if ('\0' == ch) {
         ch = *pszFields;
         pszNextField = pszFields+1;
      }

      if (ch == 'S') {

         // S = 2 bytes

         nSize = 2;
         if (cb < nSize) {
            break;
         } else {
            byte by   = pbSrc[0];
            pbDest[0] = pbSrc[1];
            pbDest[1] = by;
         }
      } else if (ch == 'L') {

         // L = 4 bytes

         nSize = 4;
         if (cb < nSize) {
            break;
         } else {
            byte by   = pbSrc[0];
            pbDest[0] = pbSrc[3];
            pbDest[3] = by;
            by        = pbSrc[1];
            pbDest[1] = pbSrc[2];
            pbDest[2] = by;
         }
      } else if (ch == 'Q') {

         // Q = 8 bytes

         nSize = 8;
         if (cb < nSize) {
            break;
         } else {
            byte by   = pbSrc[0];
            pbDest[0] = pbSrc[7];
            pbDest[7] = by;
            by        = pbSrc[1];
            pbDest[1] = pbSrc[6];
            pbDest[6] = by;
            by        = pbSrc[2];
            pbDest[2] = pbSrc[5];
            pbDest[5] = by;
            by        = pbSrc[3];
            pbDest[3] = pbSrc[4];
            pbDest[4] = by;
         }
      } else {

         // None of the above => read decimal and copy without swap

         if (ch >= '0' && ch <= '9') {
            nSize = (int) (ch - '0');
            while ( (ch = *pszNextField) >= '0' && ch <= '9') {
               nSize = nSize*10 + (int)(ch - '0');
               ++pszNextField;
            }
            // Check bounds & ensure progress
            if (nSize > cb || nSize <= 0) {
               nSize = cb;
            }
         } else {
            // Unexpected character: copy rest of data
            nSize = cb;
         }

         xMoveBytes(pbDest, pbSrc, nSize);
      }

      pbDest += nSize;
      pbSrc += nSize;
   }

   if (cb > 0) {

      // Swap could not be completed:  0 < cb < nSize <= 8

      byte byBuf[8];

      // If entire value is available in source, use swapped version
      if (nSrcSize - (pbSrc - (byte*)pvSrc) >= nSize) {
         int i;
         for (i=0; i<cb; ++i) {
            byBuf[i] = pbSrc[nSize-1-i];
         }
         pbSrc = byBuf;
      }
      std_memmove(pbDest, pbSrc, cb);
   }

   return cbCopied;
}


// See std_CopyLE/BE for documentation.  This function implements the case
// where host ordering == target byte ordering.
//
int STD_COPY(void *pvDest, int nDestSize,
             const void *pvSrc,  int nSrcSize,
             const char *pszFields)
{
   int cb = xMinSize(nDestSize, nSrcSize);
   (void)pszFields;
   xMoveBytes(pvDest, pvSrc, cb);
   return cb;
}


