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

#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif// _CRT_SECURE_NO_WARNINGS

#pragma warning( disable : 4996 )
#define strtok_r strtok_s
#define S_ISDIR(mode) (mode & S_IFDIR)
#endif //_WIN32

#ifndef VERIFY_PRINT_ERROR
#define VERIFY_PRINT_ERROR
#endif //VERIFY_PRINT_ERROR
#define FARF_ERROR 1

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "remote.h"
#include "verify.h"
#include "apps_std.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include "AEEatomic.h"
#include "AEEQList.h"
#include "platform_libs.h"
#include "fastrpc_apps_user.h"
#include "HAP_farf.h"
#include "rpcmem.h"

#ifndef _WIN32
#include <unistd.h>
#endif // _WiN32

#ifndef C_ASSERT
#define C_ASSERT(test) \
    switch(0) {\
      case 0:\
      case test:;\
    }
#endif //C_ASSERT

#define APPS_FD_BASE 100
#define ERRNO (errno == 0 ? -1 : errno)
#define APPS_STD_STREAM_FILE 1
#define APPS_STD_STREAM_BUF 2

#define ION_HEAP_ID_QSEECOM 27

#define FREEIF(pv) \
   do {\
      if(pv) { \
         void* tmp = (void*)pv;\
         pv = 0;\
         free(tmp);\
         tmp = 0;\
      } \
   } while(0)

extern int get_domain_id();

struct apps_std_buf_info {
   char* fbuf;
   int flen;
   int pos;
};

struct apps_std_info {
   QNode qn;
   int type;
   union {
      FILE* stream;
      struct apps_std_buf_info binfo;
   } u;
   apps_std_FILE fd;
};

static QList apps_std_qlst;
static pthread_mutex_t apps_std_mt;

int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

int apps_std_init(void) {
   QList_Ctor(&apps_std_qlst);
   pthread_mutex_init(&apps_std_mt, 0);
   return AEE_SUCCESS;
}

void apps_std_deinit(void) {
   pthread_mutex_destroy(&apps_std_mt);
}

PL_DEFINE(apps_std, apps_std_init, apps_std_deinit);

static int apps_std_FILE_free(struct apps_std_info* sfree) {
   int nErr = AEE_SUCCESS;

   pthread_mutex_lock(&apps_std_mt);
   QNode_Dequeue(&sfree->qn);
   pthread_mutex_unlock(&apps_std_mt);

   free(sfree);
   sfree = NULL;
   return nErr;
}

static int apps_std_FILE_alloc(FILE *stream, apps_std_FILE *fd) {
   struct apps_std_info *sinfo = 0, *info;
   QNode *pn = 0;
   apps_std_FILE prevfd = APPS_FD_BASE - 1;
   int nErr = AEE_SUCCESS;

   VERIFYC(0 != (sinfo = calloc(1, sizeof(*sinfo))), AEE_ENOMEMORY);
   QNode_CtorZ(&sinfo->qn);
   sinfo->type = APPS_STD_STREAM_FILE;
   pthread_mutex_lock(&apps_std_mt);
   pn = QList_GetFirst(&apps_std_qlst);
   if(pn) {
      info = STD_RECOVER_REC(struct apps_std_info, qn, pn);
      prevfd = info->fd;
      QLIST_FOR_REST(&apps_std_qlst, pn) {
         info = STD_RECOVER_REC(struct apps_std_info, qn, pn);
         if (info->fd != prevfd + 1) {
            sinfo->fd = prevfd + 1;
            QNode_InsPrev(pn, &sinfo->qn);
            break;
         }
         prevfd = info->fd;
      }
   }
   if(!QNode_IsQueuedZ(&sinfo->qn)) {
      sinfo->fd = prevfd + 1;
      QList_AppendNode(&apps_std_qlst, &sinfo->qn);
   }
   pthread_mutex_unlock(&apps_std_mt);

   sinfo->u.stream = stream;
   *fd = sinfo->fd;

bail:
   if(nErr) {
      FREEIF(sinfo);
      VERIFY_EPRINTF("Error %x: apps_std_FILE_alloc failed\n", nErr);
   }
   return nErr;
}

static int apps_std_FILE_get(apps_std_FILE fd, struct apps_std_info** info) {
   struct apps_std_info *sinfo = 0;
   QNode *pn, *pnn;
   FILE* stream = NULL;
   int nErr = AEE_ENOSUCHSTREAM;

   pthread_mutex_lock(&apps_std_mt);
   QLIST_NEXTSAFE_FOR_ALL(&apps_std_qlst, pn, pnn) {
      sinfo = STD_RECOVER_REC(struct apps_std_info, qn, pn);
      if(sinfo->fd == fd) {
         *info = sinfo;
         nErr = AEE_SUCCESS;
         break;
      }
   }
   pthread_mutex_unlock(&apps_std_mt);

   return nErr;
}

static void apps_std_FILE_set_buffer_stream(struct apps_std_info* sinfo, char* fbuf, int flen, int pos) {
   pthread_mutex_lock(&apps_std_mt);
   fclose(sinfo->u.stream);
   sinfo->type = APPS_STD_STREAM_BUF;
   sinfo->u.binfo.fbuf = fbuf;
   sinfo->u.binfo.flen = flen;
   sinfo->u.binfo.pos = pos;
   pthread_mutex_unlock(&apps_std_mt);
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fopen)(const char* name, const char* mode, apps_std_FILE* psout) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   FILE* stream = fopen(name, mode);
   if(stream) {
      return apps_std_FILE_alloc(stream, psout);
   } else {
      nErr = AEE_ENOSUCHFILE;
   }
   if(nErr != AEE_SUCCESS) {
      // Ignoring this error, as fopen happens on all ADSP_LIBRARY_PATHs
      VERIFY_IPRINTF("Error %x: fopen for %s failed. errno: %s\n", nErr, name, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_freopen)(apps_std_FILE sin, const char* name, const char* mode, apps_std_FILE* psout) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;
   FILE* stream;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   VERIFY(sinfo->type == APPS_STD_STREAM_FILE);
   stream = freopen(name, mode, sinfo->u.stream);
   if(stream) {
      FARF(HIGH, "freopen success: %s %x\n", name, stream);
      return apps_std_FILE_alloc(stream, psout);
   } else {
      nErr = AEE_EFOPEN;
   }
bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: freopen for %s failed. errno: %s\n", nErr, name, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fflush)(apps_std_FILE sin) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      VERIFYC(0 == fflush(sinfo->u.stream), AEE_EFFLUSH);
   }
bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: fflush for %x failed. errno: %s\n", nErr, sin, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fclose)(apps_std_FILE sin) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      VERIFYC(0 == fclose(sinfo->u.stream), AEE_EFCLOSE);
   } else {
      if(sinfo->u.binfo.fbuf) {
         rpcmem_free_internal(sinfo->u.binfo.fbuf);
         sinfo->u.binfo.fbuf = NULL;
      }
   }
   apps_std_FILE_free(sinfo);
bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: freopen for %x failed. errno: %s\n", nErr, sin, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fread)(apps_std_FILE sin, byte* buf, int bufLen, int* bytesRead, int* bEOF) __QAIC_IMPL_ATTRIBUTE {
   int out = 0, nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      out = fread(buf, 1, bufLen, sinfo->u.stream);
      *bEOF = FALSE;
      if(out <= bufLen) {
         int err;
         if(0 == out && (0 != (err = ferror(sinfo->u.stream)))) {
            nErr = AEE_EFREAD;
            VERIFY_EPRINTF("Error %x: fread returning %d bytes, requested was %d bytes, errno is %x\n", nErr, out, bufLen, err);
            return nErr;
         }
         *bEOF = feof(sinfo->u.stream);
      }
      *bytesRead = out;
   } else {
      *bytesRead = std_memscpy(buf, bufLen, sinfo->u.binfo.fbuf + sinfo->u.binfo.pos,
                               sinfo->u.binfo.flen - sinfo->u.binfo.pos);
      sinfo->u.binfo.pos += *bytesRead;
      *bEOF = sinfo->u.binfo.pos == sinfo->u.binfo.flen ? TRUE : FALSE;
   }
   FARF(HIGH, "fread returning %d %d 0", out, bufLen);
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fwrite)(apps_std_FILE sin, const byte* buf, int bufLen, int* bytesRead, int* bEOF) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      int out = fwrite(buf, 1, bufLen, sinfo->u.stream);
      *bEOF = FALSE;
      if(out <= bufLen) {
         int err;
         if(0 == out && (0 != (err = ferror(sinfo->u.stream)))) {
            nErr = AEE_EFWRITE;
            VERIFY_EPRINTF("Error %x: fwrite returning %d bytes, requested was %d bytes, errno is %x\n", nErr, out, bufLen, err);
            return nErr;
         }
         *bEOF = feof(sinfo->u.stream);
      }
      *bytesRead = out;
   } else {
      nErr = AEE_EFWRITE;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fgetpos)(apps_std_FILE sin, byte* pos, int posLen, int* posLenReq) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   fpos_t fpos;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      if(0 == fgetpos(sinfo->u.stream, &fpos)) {
         std_memmove(pos, &fpos, STD_MIN((int)sizeof(fpos), posLen));
         *posLenReq = sizeof(fpos);
         return AEE_SUCCESS;
      } else {
         nErr = AEE_EFGETPOS;
      }
   } else {
      nErr = AEE_EFGETPOS;
   }
bail:
   VERIFY_EPRINTF("Error %x: fgetpos failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fsetpos)(apps_std_FILE sin, const byte* pos, int posLen) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   fpos_t fpos;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      if(sizeof(fpos) != posLen) {
         return AEE_EBADSIZE;
      }
      std_memmove(&fpos, pos, sizeof(fpos));
      if(0 == fsetpos(sinfo->u.stream, &fpos)) {
         return AEE_SUCCESS;
      } else {
         nErr = AEE_EFSETPOS;
      }
   } else {
      nErr = AEE_EFSETPOS;
   }
bail:
   VERIFY_EPRINTF("Error %x: fsetpos failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_ftell)(apps_std_FILE sin, int* pos) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      if((*pos = ftell(sinfo->u.stream)) >= 0) {
         return AEE_SUCCESS;
      } else {
         nErr = AEE_EFTELL;
      }
   } else {
      *pos = sinfo->u.binfo.pos;
      return AEE_SUCCESS;
   }
bail:
   VERIFY_EPRINTF("Error %x: ftell failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fseek)(apps_std_FILE sin, int offset, apps_std_SEEK whence) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   int op = (int)whence;
   struct apps_std_info* sinfo = 0;

   C_ASSERT(APPS_STD_SEEK_SET == SEEK_SET);
   C_ASSERT(APPS_STD_SEEK_CUR == SEEK_CUR);
   C_ASSERT(APPS_STD_SEEK_END == SEEK_END);
   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      if(0 == fseek(sinfo->u.stream, offset, whence)) {
         return AEE_SUCCESS;
      } else {
         nErr = AEE_EFSEEK;
      }
   } else {
      switch(op) {
      case APPS_STD_SEEK_SET:
         VERIFYC(offset <= sinfo->u.binfo.flen, AEE_EFSEEK);
         sinfo->u.binfo.pos = offset;
         break;
      case APPS_STD_SEEK_CUR:
         VERIFYC(offset + sinfo->u.binfo.pos <= sinfo->u.binfo.flen, AEE_EFSEEK);
         sinfo->u.binfo.pos += offset;
         break;
      case APPS_STD_SEEK_END:
         VERIFYC(offset + sinfo->u.binfo.flen <= sinfo->u.binfo.flen, AEE_EFSEEK);
         sinfo->u.binfo.pos += offset + sinfo->u.binfo.flen;
         break;
      }
   }
bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: fseek failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_rewind)(apps_std_FILE sin) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      rewind(sinfo->u.stream);
   } else {
      sinfo->u.binfo.pos = 0;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_feof)(apps_std_FILE sin, int* bEOF) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      *bEOF = feof(sinfo->u.stream);
   } else {
      nErr = AEE_EUNSUPPORTED;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_ferror)(apps_std_FILE sin, int* err) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      *err = ferror(sinfo->u.stream);
   } else {
      nErr = AEE_EUNSUPPORTED;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_clearerr)(apps_std_FILE sin) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      clearerr(sinfo->u.stream);
   } else {
      nErr = AEE_EUNSUPPORTED;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_flen)(apps_std_FILE sin, uint64* len) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      struct stat st_buf;
      int fd = fileno(sinfo->u.stream);
      C_ASSERT(sizeof(st_buf.st_size) <= sizeof(*len));
      if(fd == -1) {
         nErr = AEE_EFLEN;
         VERIFY_EPRINTF("Error %x: flen failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
         return nErr;
      }
      if(0 != fstat(fd, &st_buf)) {
         nErr = AEE_EFLEN;
         VERIFY_EPRINTF("Error %x: flen failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
         return nErr;
      }
      *len = st_buf.st_size;
   } else {
      *len = sinfo->u.binfo.flen;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_print_string)(const char* str) __QAIC_IMPL_ATTRIBUTE {
   printf("%s", str);
   return AEE_SUCCESS;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_getenv)(const char* name, char* val, int valLen, int* valLenReq) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   char* vv = getenv(name);
   if(vv) {
      *valLenReq = std_strlen(vv) + 1;
      std_strlcpy(val, vv, valLen);
      return AEE_SUCCESS;
   }
   nErr = AEE_EGETENV;
   VERIFY_IPRINTF("Error %x: apps_std getenv failed: %s %s\n", nErr, name, strerror(ERRNO));
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_setenv)(const char* name, const char* val, int override) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
#ifdef _WIN32
   return AEE_EUNSUPPORTED;
#else //_WIN32
   if(0 != setenv(name, val, override)) {
      nErr = AEE_ESETENV;
      VERIFY_EPRINTF("Error %x: setenv failed for %s, errno is %s\n", nErr, name, strerror(ERRNO));
      return nErr;
   }
   return AEE_SUCCESS;
#endif //_WIN32
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_unsetenv)(const char* name) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
#ifdef _WIN32
   return AEE_EUNSUPPORTED;
#else //_WIN32
   if(0 != unsetenv(name)) {
      nErr = AEE_ESETENV;
      VERIFY_EPRINTF("Error %x: unsetenv failed for %s, errno is %s\n", nErr, name, strerror(ERRNO));
      return nErr;
   }
   return AEE_SUCCESS;
#endif //_WIN32
}


#if (defined LE_ENABLE)
static char* ADSP_LIBRARY_PATH=";/usr/lib/rfsa/adsp;/usr/lib;/dsp;/usr/share/fastrpc";
static char* ADSP_AVS_CFG_PATH=";/etc/acdbdata/";
#elif (defined __BRILLO__)
static char* ADSP_LIBRARY_PATH=";/system/etc/lib/rfsa/adsp;/system/vendor/etc/lib/rfsa/adsp;/dsp";
static char* ADSP_AVS_CFG_PATH=";/etc/acdbdata/";
#elif (defined _ANDROID) || (defined ANDROID)
#if (defined ANDROID_P) && (defined FULL_TREBLE)
#ifdef SYSTEM_RPC_LIBRARY
static char* ADSP_LIBRARY_PATH=";/system/lib/rfsa/adsp";
static char* ADSP_AVS_CFG_PATH=";/etc/acdbdata/";
#else
static char* ADSP_LIBRARY_PATH=";/vendor/lib/rfsa/adsp;/vendor/dsp";
static char* ADSP_AVS_CFG_PATH=";/vendor/etc/acdbdata/";
#endif
#else
static char* ADSP_LIBRARY_PATH=";/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp;/vendor/dsp";
static char* ADSP_AVS_CFG_PATH=";/etc/acdbdata/;/vendor/etc/acdbdata/";
#endif
#elif (defined __QNX__)
static char* ADSP_LIBRARY_PATH="/radio/lib/firmware";
#else
static char* ADSP_LIBRARY_PATH="";
#endif

#define     EMTPY_STR      ""
#define     ENV_LEN_GUESS  256

static int get_dirlist_from_env(const char* envvarname, char** ppDirList){
   char     *envList    =  NULL;
   char     *envListBuf =  NULL;
   char     *dirList    =  NULL;
   char     *dirListBuf =  NULL;
   char     *srcStr     =  NULL;
   int      nErr        =  AEE_SUCCESS;
   int      envListLen  =  0;
   int      listLen     =  0;
   int      envLenGuess = STD_MAX(ENV_LEN_GUESS, 1 + std_strlen(ADSP_LIBRARY_PATH));

   VERIFYC(NULL != ppDirList, AEE_EMEMPTR);

   VERIFYC(envListBuf = (char*)malloc(sizeof(char) * envLenGuess), AEE_ENOMEMORY);
   envList = envListBuf;
   *envList = '\0';
   if (0 == apps_std_getenv(envvarname, envList, envLenGuess, &envListLen)) {
      if (envLenGuess < envListLen) {
         FREEIF(envListBuf);
         VERIFYC(envListBuf = realloc(envListBuf, sizeof(char) * envListLen), AEE_ENOMEMORY);
         envList = envListBuf;
         VERIFY(0 == (nErr = apps_std_getenv(envvarname, envList, envListLen, &listLen)));
      }
   } else if(std_strncmp(envvarname, "ADSP_LIBRARY_PATH", 17) == 0) {
      envListLen = listLen = 1 + std_strlcpy(envListBuf, ADSP_LIBRARY_PATH, envLenGuess);
   } else if(std_strncmp(envvarname, "ADSP_AVS_CFG_PATH", 17) == 0) {
      envListLen = listLen = 1 + std_strlcpy(envListBuf, ADSP_AVS_CFG_PATH, envLenGuess);
   }

   /*
    * Allocate mem. to copy envvarname.
    * If envvarname not set, allocate mem to create an empty string
    */
   if('\0' != *envList) {
      srcStr = envList;
   } else {
      srcStr = EMTPY_STR;
      envListLen = std_strlen(EMTPY_STR) + 1;
   }
   VERIFYC(dirListBuf = (char*)malloc(sizeof(char) * envListLen), AEE_ENOMEMORY);
   dirList = dirListBuf;
   std_strlcpy(dirList, srcStr, envListLen);
   *ppDirList = dirListBuf;
bail:
   FREEIF(envListBuf);
   if(nErr != AEE_SUCCESS) {
       VERIFY_EPRINTF("Error %x: get dirlist from env failed for %s\n", nErr, envvarname);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fopen_with_env)(const char* envvarname,
        const char* delim, const char* name, const char* mode,
        apps_std_FILE* psout) __QAIC_IMPL_ATTRIBUTE {

   int      nErr        =  AEE_SUCCESS;
   char     *dirName    =  NULL;
   char     *pos        =  NULL;
   char     *dirListBuf =  NULL;
   char     *dirList    =  NULL;
   char     *absName    =  NULL;
   uint16   absNameLen  =  0;
   int domain;

   VERIFYC(NULL != mode, AEE_EINVALIDMODE);
   VERIFYC(NULL != delim, AEE_EINVALIDFORMAT);
   VERIFYC(NULL != name, AEE_EMEMPTR);

   VERIFY(0 == (nErr = get_dirlist_from_env(envvarname, &dirListBuf )));
   VERIFYC(NULL != (dirList = dirListBuf), AEE_EMEMPTR);

   while(dirList)
   {
      pos = strstr(dirList, delim);
      dirName = dirList;
      if (pos) {
         *pos = '\0';
         dirList = pos + std_strlen(delim);
      } else {
         dirList = 0;
      }

      // Account for slash char
      absNameLen = std_strlen(dirName) + std_strlen(name) + 2;
      VERIFYC(NULL != (absName = (char*)malloc(sizeof(char) * absNameLen)), AEE_ENOMEMORY);
      if ('\0' != *dirName) {
         std_strlcpy(absName, dirName, absNameLen);
         std_strlcat(absName, "/", absNameLen);
         std_strlcat(absName, name, absNameLen);
      } else {
         std_strlcpy(absName, name, absNameLen);
      }

      nErr = apps_std_fopen(absName, mode, psout);
      FREEIF(absName);
      if (AEE_SUCCESS == nErr) {
	 // Success
         goto bail;
      }
   }
   domain = get_domain_id() & DOMAIN_ID_MASK;

#ifdef ANDROID_P
   absNameLen = std_strlen("/vendor/dsp/adsp/") + std_strlen(name) + 1;
   VERIFYC(NULL != (absName = (char*)malloc(sizeof(char) * absNameLen)), AEE_ENOMEMORY);

   if (domain == ADSP_DOMAIN_ID){
       std_strlcpy(absName, "/vendor/dsp/adsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == MDSP_DOMAIN_ID){
       std_strlcpy(absName, "/vendor/dsp/mdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == SDSP_DOMAIN_ID){
       std_strlcpy(absName, "/vendor/dsp/sdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == CDSP_DOMAIN_ID) {
       std_strlcpy(absName, "/vendor/dsp/cdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else {
       absName[0] = '\0';
   }
   nErr = apps_std_fopen(absName, mode, psout);
#else
   absNameLen = std_strlen("/dsp/adsp/") + std_strlen(name) + 1;
   VERIFYC(NULL != (absName = (char*)malloc(sizeof(char) * absNameLen)), AEE_ENOMEMORY);

   if (domain == ADSP_DOMAIN_ID){
       std_strlcpy(absName, "/dsp/adsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == MDSP_DOMAIN_ID){
       std_strlcpy(absName, "/dsp/mdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == SDSP_DOMAIN_ID){
       std_strlcpy(absName, "/dsp/sdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else if (domain == CDSP_DOMAIN_ID) {
       std_strlcpy(absName, "/dsp/cdsp/", absNameLen);
       std_strlcat(absName, name,absNameLen);
   } else {
       absName[0] = '\0';
   }
   nErr = apps_std_fopen(absName, mode, psout);
#endif
bail:
   FREEIF(absName);
   FREEIF(dirListBuf);
   if (nErr != AEE_SUCCESS) {
	VERIFY_IPRINTF("Error %x: fopen failed for %s. (%s)", nErr, name, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_HEADER_EXPORT int __QAIC_IMPL(apps_std_get_search_paths_with_env)(
    const char* envvarname, const char* delim, _cstring1_t* paths,
    int pathsLen, uint32* numPaths, uint16* maxPathLen) __QAIC_IMPL_ATTRIBUTE{

   char *path        = NULL;
   int  nErr         = AEE_SUCCESS;
   char *dirListBuf  = NULL;
   int  i            = 0;
   char *saveptr     = NULL;
   struct stat st;

   VERIFYC(NULL != numPaths, AEE_EBADSIZE);
   VERIFYC(NULL != delim, AEE_EINVALIDFORMAT);
   VERIFYC(NULL != maxPathLen, AEE_EBADSIZE);

   VERIFY(AEE_SUCCESS == (nErr = get_dirlist_from_env(envvarname, &dirListBuf )));

   *numPaths = 0;
   *maxPathLen = 0;

   // Get the number of folders
    path = strtok_r (dirListBuf, delim, &saveptr);
    while (path != NULL){
        // If the path exists, add it to the return
        if ((stat(path, &st) == 0) && (S_ISDIR(st.st_mode))) {
            *maxPathLen = STD_MAX(*maxPathLen, std_strlen(path)+1);
            if (paths && i < pathsLen && paths[i].data && paths[i].dataLen >= std_strlen(path)) {
                    std_strlcpy(paths[i].data, path, paths[i].dataLen);
            }
            i++;
         }
        path = strtok_r (NULL, delim, &saveptr);
    }
    *numPaths = i;

bail:
   FREEIF(dirListBuf);
   if (nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: apps_std_get_search_paths_with_env failed\n", nErr);
   }
   return nErr;
}


__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fgets)(apps_std_FILE sin, byte* buf, int bufLen, int* bEOF) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      char* out = fgets((char*)buf, bufLen, sinfo->u.stream);
      *bEOF = FALSE;
      if(!out) {
         int err;
         if(0 != (err = ferror(sinfo->u.stream))) {
	    nErr = AEE_EFGETS;
            VERIFY_EPRINTF("Error %x: fgets failed for %x, errno is %s\n", nErr, sin, strerror(ERRNO));
            return nErr;
         }
         *bEOF = feof(sinfo->u.stream);
      }
   } else {
      nErr = AEE_EUNSUPPORTED;
   }
bail:
   return nErr;
}

__QAIC_HEADER_EXPORT int __QAIC_HEADER(apps_std_fileExists)(const char* path, boolean* exists) __QAIC_HEADER_ATTRIBUTE{
    int nErr = AEE_SUCCESS;
    struct stat   buffer;

    VERIFYC(path != NULL, AEE_EMEMPTR);
    VERIFYC(exists != NULL, AEE_EMEMPTR);

    *exists = (stat (path, &buffer) == 0);

bail:
    if (nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: fileExists failed for path %s\n", nErr, path);
    }
    return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fsync)(apps_std_FILE sin) __QAIC_IMPL_ATTRIBUTE {
    int nErr = AEE_SUCCESS;
   struct apps_std_info* sinfo = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      // This flushes the given sin file stream to user-space buffer.
      // NOTE: this does NOT ensure data is physically sotred on disk
      nErr = fflush(sinfo->u.stream);
      if(nErr != AEE_SUCCESS){
         VERIFY_EPRINTF("Error %x: apps_std fsync failed,errno is %s\n", nErr, strerror(ERRNO));
      }
   } else {
      nErr = AEE_EUNSUPPORTED;
   }

bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fremove)(const char* name) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;

   if(NULL == name){
      return nErr;
   }
   nErr = remove(name);
   if(nErr != AEE_SUCCESS){
      VERIFY_EPRINTF("Error %x: failed to remove file %s,errno is %s\n", nErr, name, strerror(ERRNO));
   }

   return nErr;
}

static int decrypt_int(char* fbuf, int size) {
   int nErr = 0, fd;
   void* handle = 0;
   int32_t (*l_init)(void);
   int32_t (*l_deinit)(void);
   int32_t (*l_decrypt)(int32_t, int32_t);

   VERIFYC(NULL != (handle = dlopen("liblmclient.so", RTLD_NOW)), AEE_EBADHANDLE);
   VERIFYC(NULL != (l_init = dlsym(handle, "license_manager_init")), AEE_ENOSUCHSYMBOL);
   VERIFYC(NULL != (l_deinit = dlsym(handle, "license_manager_deinit")), AEE_ENOSUCHSYMBOL);
   VERIFYC(NULL != (l_decrypt = dlsym(handle, "license_manager_decrypt")), AEE_ENOSUCHSYMBOL);
   VERIFY(0 == (nErr = l_init()));
   VERIFY(-1 != (fd = rpcmem_to_fd_internal(fbuf)));
   VERIFY(0 == (nErr = l_decrypt(fd, size)));
   VERIFY(0 == (nErr = l_deinit()));
bail:
   if(nErr) {
      VERIFY_EPRINTF("Error %x: dlopen for licmgr failed. errno: %s\n", nErr, dlerror());
   }
   if(handle) {
      dlclose(handle);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_fdopen_decrypt)(apps_std_FILE sin, apps_std_FILE *psout) __QAIC_IMPL_ATTRIBUTE {
   int fd, nErr = AEE_SUCCESS;
   struct stat st_buf;
   struct apps_std_info* sinfo = 0;
   int sz, pos;
   char* fbuf = 0;

   VERIFY(0 == (nErr = apps_std_FILE_get(sin, &sinfo)));
   if(sinfo->type == APPS_STD_STREAM_FILE) {
      pos = ftell(sinfo->u.stream);
      VERIFYC(-1 != (fd = fileno(sinfo->u.stream)), AEE_EFLEN);
      VERIFYC(0 == fstat(fd, &st_buf), AEE_EFLEN);
      sz = (int)st_buf.st_size;
      VERIFYC(0 != (fbuf = rpcmem_alloc_internal(ION_HEAP_ID_QSEECOM, 1, sz)), AEE_EMEMPTR);
      VERIFYC(0 == fseek(sinfo->u.stream, 0, SEEK_SET), AEE_EFSEEK);
      VERIFYC(sz == (int)fread(fbuf, 1, sz, sinfo->u.stream), AEE_EFREAD);
      VERIFY(0 == (nErr = decrypt_int(fbuf, sz)));
      apps_std_FILE_set_buffer_stream(sinfo, fbuf, sz, pos);
      *psout = sin;
   } else {
      nErr = AEE_EUNSUPPORTED;
   }
bail:
   if(nErr) {
      if(fbuf) {
         rpcmem_free_internal(fbuf);
         fbuf = NULL;
      }
   }
   return nErr;
}


__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_opendir)(const char* name, apps_std_DIR* dir) __QAIC_IMPL_ATTRIBUTE {
   int nErr = 0;
   DIR *odir;

   if(NULL == dir) {
      return AEE_EBADPARM;
   }
   odir = opendir(name);
   if(odir != NULL) {
      dir->handle = (uint64)odir;
   } else {
      nErr = -1;
      VERIFY_EPRINTF("Error %x: failed to opendir %s,errno is %s\n", nErr, name, strerror(ERRNO));
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_closedir)(const apps_std_DIR* dir) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;

   if((NULL == dir) || (0 == dir->handle)){
      return AEE_EBADPARM;
   }
   nErr = closedir((DIR *)dir->handle);
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: failed to closedir, errno is %s\n", nErr, strerror(ERRNO));
   }

   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_readdir)(const apps_std_DIR* dir, apps_std_DIRENT* dirent, int *bEOF) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct dirent *odirent;

   if((NULL == dir) || (0 == dir->handle)){
      return AEE_EBADPARM;
   }
   *bEOF = 0;
   errno = 0;
   odirent = readdir((DIR *)dir->handle);
   if(odirent != NULL) {
      dirent->ino = (int)odirent->d_ino;
      std_strlcpy(dirent->name, odirent->d_name, sizeof(dirent->name));
   } else {
      if(errno == 0) {
         *bEOF = 1;
      } else {
         nErr = -1;
      }
   }

   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_mkdir)(const char* name, int mode) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;

   if(NULL == name){
      return nErr;
   }
   nErr = mkdir(name, mode);
   if(nErr != AEE_SUCCESS){
      VERIFY_EPRINTF("Error %x: failed to mkdir %s,errno is %s\n", nErr, name, strerror(ERRNO));
   }

   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_rmdir)(const char* name) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;

   if(NULL == name){
      return nErr;
   }
   nErr = rmdir(name);
   if(nErr != AEE_SUCCESS){
      VERIFY_EPRINTF("Error %x: failed to rmdir %s,errno is %s\n", nErr, name, strerror(ERRNO));
   }

   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_HEADER(apps_std_stat)(const char* name, apps_std_STAT* ist) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS, nOpenErr = AEE_SUCCESS, fd = -1;
   apps_std_FILE ps;
   struct apps_std_info* sinfo = 0;
   struct stat st;

   if((NULL == name) || (NULL == ist)) {
      return AEE_EBADPARM;
   }
   VERIFYC(0 == (nOpenErr = apps_std_fopen_with_env("ADSP_LIBRARY_PATH", ";", name, "r", &ps)), AEE_EFOPEN);
   VERIFYC(0 == (nErr = apps_std_FILE_get(ps, &sinfo)), AEE_EBADFD);
   VERIFYC(-1 != (fd = fileno(sinfo->u.stream)), AEE_EBADFD);
   VERIFYC(0 == fstat(fd, &st), AEE_EBADFD);
   ist->dev = st.st_dev;
   ist->ino = st.st_ino;
   ist->mode = st.st_mode;
   ist->nlink = st.st_nlink;
   ist->rdev = st.st_rdev;
   ist->size = st.st_size;
   ist->atime = (int64)st.st_atim.tv_sec;
   ist->atimensec = (int64)st.st_atim.tv_nsec;
   ist->mtime = (int64)st.st_mtim.tv_sec;
   ist->mtimensec =(int64)st.st_mtim.tv_nsec;
   ist->ctime = (int64)st.st_ctim.tv_nsec;
   ist->ctimensec = (int64)st.st_ctim.tv_nsec;
bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: failed to stat %s, file open returned %x, errno is %s\n", nErr, name, nOpenErr, strerror(ERRNO));
   }
   if (nOpenErr == AEE_SUCCESS) {
     apps_std_fclose(ps);
     sinfo = 0;
   }
   if(sinfo) {
      apps_std_FILE_free(sinfo);
   }
   return nErr;
}

__QAIC_HEADER_EXPORT int __QAIC_HEADER(apps_std_ftrunc)(apps_std_FILE sin, int offset) __QAIC_HEADER_ATTRIBUTE {
   int nErr = 0, fd = -1;
   struct apps_std_info* sinfo = 0;

   VERIFYC(0 == (nErr = apps_std_FILE_get(sin, &sinfo)), AEE_EBADFD);
   VERIFYC(-1 != (fd = fileno(sinfo->u.stream)), AEE_EBADFD);

   if(0 != ftruncate(fd, offset)) {
      return ERRNO;
   }
bail:
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_std_frename)(const char* oldname, const char* newname) __QAIC_IMPL_ATTRIBUTE {
  int nErr = AEE_SUCCESS;

  VERIFYC(NULL != oldname && NULL != newname, AEE_EBADPARM);
  nErr = rename(oldname, newname);
bail:
  if(nErr != AEE_SUCCESS) {
    VERIFY_EPRINTF("Error %x: failed to rename file, errno is %s\n", nErr, strerror(ERRNO));
  }
  return nErr;
}
