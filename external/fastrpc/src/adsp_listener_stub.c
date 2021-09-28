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
#ifndef _ADSP_LISTENER_STUB_H
#define _ADSP_LISTENER_STUB_H
#include "adsp_listener.h"
#include "remote.h"
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

typedef struct _heap _heap;
struct _heap {
   _heap* pPrev;
   const char* loc;
   uint64_t buf;
};

typedef struct allocator {
   _heap* pheap;
   uint8_t* stack;
   uint8_t* stackEnd;
   int nSize;
} allocator;

static __inline int _heap_alloc(_heap** ppa, const char* loc, int size, void** ppbuf) {
   _heap* pn = 0;
   pn = malloc(size + sizeof(_heap) - sizeof(uint64_t));
   if(pn != 0) {
      pn->pPrev = *ppa;
      pn->loc = loc;
      *ppa = pn;
      *ppbuf = (void*)&(pn->buf);
      return 0;
   } else {
      return -1;
   }
}
#define _ALIGN_SIZE(x, y) (((x) + (y-1)) & ~(y-1))


static __inline int allocator_alloc(allocator* me,
                                    const char* loc,
                                    int size,
                                    unsigned int al,
                                    void** ppbuf) {
   if(size < 0) {
      return -1;
   } else if (size == 0) {
      *ppbuf = 0;
      return 0;
   }
   if((_ALIGN_SIZE((uintptr_t)me->stackEnd, al) + size) < (uintptr_t)me->stack + me->nSize) {
      *ppbuf = (uint8_t*)_ALIGN_SIZE((uintptr_t)me->stackEnd, al);
      me->stackEnd = (uint8_t*)_ALIGN_SIZE((uintptr_t)me->stackEnd, al) + size;
      return 0;
   } else {
      return _heap_alloc(&me->pheap, loc, size, ppbuf);
   }
}


static __inline void allocator_deinit(allocator* me) {
   _heap* pa = me->pheap;
   while(pa != 0) {
      _heap* pn = pa;
      const char* loc = pn->loc;
      (void)loc;
      pa = pn->pPrev;
      free(pn);
   }
}

static __inline void allocator_init(allocator* me, uint8_t* stack, int stackSize) {
   me->stack =  stack;
   me->stackEnd =  stack + stackSize;
   me->nSize = stackSize;
   me->pheap = 0;
}


#endif // ALLOCATOR_H

#ifndef SLIM_H
#define SLIM_H

#include <stdint.h>

//a C data structure for the idl types that can be used to implement
//static and dynamic language bindings fairly efficiently.
//
//the goal is to have a minimal ROM and RAM footprint and without
//doing too many allocations.  A good way to package these things seemed
//like the module boundary, so all the idls within  one module can share
//all the type references.


#define PARAMETER_IN       0x0
#define PARAMETER_OUT      0x1
#define PARAMETER_INOUT    0x2
#define PARAMETER_ROUT     0x3
#define PARAMETER_INROUT   0x4

//the types that we get from idl
#define TYPE_OBJECT             0x0
#define TYPE_INTERFACE          0x1
#define TYPE_PRIMITIVE          0x2
#define TYPE_ENUM               0x3
#define TYPE_STRING             0x4
#define TYPE_WSTRING            0x5
#define TYPE_STRUCTURE          0x6
#define TYPE_UNION              0x7
#define TYPE_ARRAY              0x8
#define TYPE_SEQUENCE           0x9

//these require the pack/unpack to recurse
//so it's a hint to those languages that can optimize in cases where
//recursion isn't necessary.
#define TYPE_COMPLEX_STRUCTURE  (0x10 | TYPE_STRUCTURE)
#define TYPE_COMPLEX_UNION      (0x10 | TYPE_UNION)
#define TYPE_COMPLEX_ARRAY      (0x10 | TYPE_ARRAY)
#define TYPE_COMPLEX_SEQUENCE   (0x10 | TYPE_SEQUENCE)


typedef struct Type Type;

#define INHERIT_TYPE\
   int32_t nativeSize;                /*in the simple case its the same as wire size and alignment*/\
   union {\
      struct {\
         const uintptr_t         p1;\
         const uintptr_t         p2;\
      } _cast;\
      struct {\
         uint32_t  iid;\
         uint32_t  bNotNil;\
      } object;\
      struct {\
         const Type  *arrayType;\
         int32_t      nItems;\
      } array;\
      struct {\
         const Type *seqType;\
         int32_t      nMaxLen;\
      } seqSimple; \
      struct {\
         uint32_t bFloating;\
         uint32_t bSigned;\
      } prim; \
      const SequenceType* seqComplex;\
      const UnionType  *unionType;\
      const StructType *structType;\
      int32_t         stringMaxLen;\
      uint8_t        bInterfaceNotNil;\
   } param;\
   uint8_t    type;\
   uint8_t    nativeAlignment\

typedef struct UnionType UnionType;
typedef struct StructType StructType;
typedef struct SequenceType SequenceType;
struct Type {
   INHERIT_TYPE;
};

struct SequenceType {
   const Type *         seqType;
   uint32_t               nMaxLen;
   uint32_t               inSize;
   uint32_t               routSizePrimIn;
   uint32_t               routSizePrimROut;
};

//byte offset from the start of the case values for
//this unions case value array.  it MUST be aligned
//at the alignment requrements for the descriptor
//
//if negative it means that the unions cases are
//simple enumerators, so the value read from the descriptor
//can be used directly to find the correct case
typedef union CaseValuePtr CaseValuePtr;
union CaseValuePtr {
   const uint8_t*   value8s;
   const uint16_t*  value16s;
   const uint32_t*  value32s;
   const uint64_t*  value64s;
};

//these are only used in complex cases
//so I pulled them out of the type definition as references to make
//the type smaller
struct UnionType {
   const Type           *descriptor;
   uint32_t               nCases;
   const CaseValuePtr   caseValues;
   const Type * const   *cases;
   int32_t               inSize;
   int32_t               routSizePrimIn;
   int32_t               routSizePrimROut;
   uint8_t                inAlignment;
   uint8_t                routAlignmentPrimIn;
   uint8_t                routAlignmentPrimROut;
   uint8_t                inCaseAlignment;
   uint8_t                routCaseAlignmentPrimIn;
   uint8_t                routCaseAlignmentPrimROut;
   uint8_t                nativeCaseAlignment;
   uint8_t              bDefaultCase;
};

struct StructType {
   uint32_t               nMembers;
   const Type * const   *members;
   int32_t               inSize;
   int32_t               routSizePrimIn;
   int32_t               routSizePrimROut;
   uint8_t                inAlignment;
   uint8_t                routAlignmentPrimIn;
   uint8_t                routAlignmentPrimROut;
};

typedef struct Parameter Parameter;
struct Parameter {
   INHERIT_TYPE;
   uint8_t    mode;
   uint8_t  bNotNil;
};

#define SLIM_SCALARS_IS_DYNAMIC(u) (((u) & 0x00ffffff) == 0x00ffffff)

typedef struct Method Method;
struct Method {
   uint32_t                    uScalars;            //no method index
   int32_t                     primInSize;
   int32_t                     primROutSize;
   int                         maxArgs;
   int                         numParams;
   const Parameter * const     *params;
   uint8_t                       primInAlignment;
   uint8_t                       primROutAlignment;
};

typedef struct Interface Interface;

struct Interface {
   int                            nMethods;
   const Method  * const          *methodArray;
   int                            nIIds;
   const uint32_t                   *iids;
   const uint16_t*                  methodStringArray;
   const uint16_t*                  methodStrings;
   const char*                    strings;
};


#endif //SLIM_H


#ifndef _ADSP_LISTENER_SLIM_H
#define _ADSP_LISTENER_SLIM_H
#include "remote.h"
#include <stdint.h>

#ifndef __QAIC_SLIM
#define __QAIC_SLIM(ff) ff
#endif
#ifndef __QAIC_SLIM_EXPORT
#define __QAIC_SLIM_EXPORT
#endif

static const Type types[3];
static const SequenceType sequenceTypes[1] = {{&(types[0]),0x0,0x4,0x4,0x0}};
static const Type types[3] = {{0x8,{{(const uintptr_t)&(types[1]),(const uintptr_t)0x0}}, 9,0x4},{0x1,{{(const uintptr_t)0,(const uintptr_t)0}}, 2,0x1},{0x4,{{(const uintptr_t)0,(const uintptr_t)1}}, 2,0x4}};
static const Parameter parameters[9] = {{0x4,{{(const uintptr_t)0,(const uintptr_t)0}}, 2,0x4,0,0},{0x4,{{(const uintptr_t)0,(const uintptr_t)1}}, 2,0x4,0,0},{0x8,{{(const uintptr_t)&(sequenceTypes[0]),0}}, 25,0x4,0,0},{0x4,{{(const uintptr_t)0,(const uintptr_t)0}}, 2,0x4,3,0},{0x8,{{(const uintptr_t)&(sequenceTypes[0]),0}}, 25,0x4,3,0},{0x8,{{(const uintptr_t)&(types[2]),(const uintptr_t)0x0}}, 9,0x4,3,0},{0x8,{{(const uintptr_t)&(types[1]),(const uintptr_t)0x0}}, 9,0x4,0,0},{0x8,{{(const uintptr_t)&(types[1]),(const uintptr_t)0x0}}, 9,0x4,3,0},{0x4,{{(const uintptr_t)0,(const uintptr_t)1}}, 2,0x4,3,0}};
static const Parameter* const parameterArrays[23] = {(&(parameters[0])),(&(parameters[1])),(&(parameters[2])),(&(parameters[3])),(&(parameters[3])),(&(parameters[3])),(&(parameters[4])),(&(parameters[5])),(&(parameters[5])),(&(parameters[0])),(&(parameters[1])),(&(parameters[6])),(&(parameters[3])),(&(parameters[3])),(&(parameters[3])),(&(parameters[7])),(&(parameters[8])),(&(parameters[0])),(&(parameters[1])),(&(parameters[7])),(&(parameters[8])),(&(parameters[0])),(&(parameters[4]))};
static const Method methods[5] = {{REMOTE_SCALARS_MAKEX(0,0,255,255,15,15),0x18,0xc,16,9,(&(parameterArrays[0])),0x4,0x4},{REMOTE_SCALARS_MAKEX(0,0,255,255,15,15),0x8,0x0,4,2,(&(parameterArrays[21])),0x4,0x1},{REMOTE_SCALARS_MAKEX(0,0,0x0,0x0,0x0,0x0),0x0,0x0,0,0,0,0x0,0x0},{REMOTE_SCALARS_MAKEX(0,0,0x2,0x2,0x0,0x0),0x10,0x10,11,8,(&(parameterArrays[9])),0x4,0x4},{REMOTE_SCALARS_MAKEX(0,0,0x1,0x2,0x0,0x0),0xc,0x4,6,4,(&(parameterArrays[17])),0x4,0x4}};
static const Method* const methodArrays[6] = {&(methods[0]),&(methods[1]),&(methods[2]),&(methods[2]),&(methods[3]),&(methods[4])};
static const char strings[165] = "invoke_get_in_bufs\0routBufLenReq\0get_in_bufs2\0inBufLenReq\0next_invoke\0bufsLenReq\0prevResult\0inBuffers\0prevbufs\0outBufs\0prevCtx\0offset\0handle\0next2\0init2\0init\0ctx\0sc\0";
static const uint16_t methodStrings[29] = {58,119,81,111,158,134,162,92,46,19,141,119,81,102,158,134,162,14,70,33,158,127,14,70,0,158,92,147,153};
static const uint16_t methodStringsArrays[6] = {0,24,28,27,10,19};
__QAIC_SLIM_EXPORT const Interface __QAIC_SLIM(adsp_listener_slim) = {6,&(methodArrays[0]),0,0,&(methodStringsArrays [0]),methodStrings,strings};
#endif //_ADSP_LISTENER_SLIM_H
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifndef __QAIC_REMOTE
#define __QAIC_REMOTE(ff) ff
#endif //__QAIC_REMOTE

#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE

#ifndef __QAIC_STUB
#define __QAIC_STUB(ff) ff
#endif //__QAIC_STUB

#ifndef __QAIC_STUB_EXPORT
#define __QAIC_STUB_EXPORT
#endif // __QAIC_STUB_EXPORT

#ifndef __QAIC_STUB_ATTRIBUTE
#define __QAIC_STUB_ATTRIBUTE
#endif // __QAIC_STUB_ATTRIBUTE

#ifndef __QAIC_SKEL
#define __QAIC_SKEL(ff) ff
#endif //__QAIC_SKEL__

#ifndef __QAIC_SKEL_EXPORT
#define __QAIC_SKEL_EXPORT
#endif // __QAIC_SKEL_EXPORT

#ifndef __QAIC_SKEL_ATTRIBUTE
#define __QAIC_SKEL_ATTRIBUTE
#endif // __QAIC_SKEL_ATTRIBUTE

#ifdef __QAIC_DEBUG__
   #ifndef __QAIC_DBG_PRINTF__
   #define __QAIC_DBG_PRINTF__( ee ) do { printf ee ; } while(0)
   #endif
#else
   #define __QAIC_DBG_PRINTF__( ee ) (void)0
#endif


#define _OFFSET(src, sof)  ((void*)(((char*)(src)) + (sof)))

#define _COPY(dst, dof, src, sof, sz)  \
   do {\
         struct __copy { \
            char ar[sz]; \
         };\
         *(struct __copy*)_OFFSET(dst, dof) = *(struct __copy*)_OFFSET(src, sof);\
   } while (0)

#define _ASSIGN(dst, src, sof)  \
   do {\
      dst = OFFSET(src, sof); \
   } while (0)

#define _STD_STRLEN_IF(str) (str == 0 ? 0 : strlen(str))

#include "AEEStdErr.h"

#define _TRY(ee, func) \
   do { \
      if (AEE_SUCCESS != ((ee) = func)) {\
         __QAIC_DBG_PRINTF__((__FILE_LINE__  ": error: %d\n", (int)(ee)));\
         goto ee##bail;\
      } \
   } while (0)

#define _CATCH(exception) exception##bail: if (exception != AEE_SUCCESS)

#define _ASSERT(nErr, ff) _TRY(nErr, 0 == (ff) ? AEE_EBADPARM : AEE_SUCCESS)

#ifdef __QAIC_DEBUG__
#define _ALLOCATE(nErr, pal, size, alignment, pv) _TRY(nErr, allocator_alloc(pal, __FILE_LINE__, size, alignment, (void**)&pv))
#else
#define _ALLOCATE(nErr, pal, size, alignment, pv) _TRY(nErr, allocator_alloc(pal, 0, size, alignment, (void**)&pv))
#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifndef _const_adsp_listener_handle
#define _const_adsp_listener_handle ((remote_handle)-1)
#endif //_const_adsp_listener_handle

static void _adsp_listener_pls_dtor(void* data) {
   remote_handle* ph = (remote_handle*)data;
   if(_const_adsp_listener_handle != *ph) {
      (void)__QAIC_REMOTE(remote_handle_close)(*ph);
      *ph = _const_adsp_listener_handle;
   }
}

static int _adsp_listener_pls_ctor(void* ctx, void* data) {
   remote_handle* ph = (remote_handle*)data;
   *ph = _const_adsp_listener_handle;
   if(*ph == (remote_handle)-1) {
      return __QAIC_REMOTE(remote_handle_open)((const char*)ctx, ph);
   }
   return 0;
}

#if (defined __qdsp6__) || (defined __hexagon__)
#pragma weak  adsp_pls_add_lookup
extern int adsp_pls_add_lookup(uint32_t type, uint32_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void* ctx), void** ppo);
#pragma weak  HAP_pls_add_lookup
extern int HAP_pls_add_lookup(uint32_t type, uint32_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void* ctx), void** ppo);

__QAIC_STUB_EXPORT remote_handle _adsp_listener_handle(void) {
   remote_handle* ph;
   if(adsp_pls_add_lookup) {
      if(0 == adsp_pls_add_lookup((uint32_t)_adsp_listener_handle, 0, sizeof(*ph),  _adsp_listener_pls_ctor, "adsp_listener",  _adsp_listener_pls_dtor, (void**)&ph))  {
         return *ph;
      }
      return (remote_handle)-1;
   } else if(HAP_pls_add_lookup) {
      if(0 == HAP_pls_add_lookup((uint32_t)_adsp_listener_handle, 0, sizeof(*ph),  _adsp_listener_pls_ctor, "adsp_listener",  _adsp_listener_pls_dtor, (void**)&ph))  {
         return *ph;
      }
      return (remote_handle)-1;
   }
   return(remote_handle)-1;
}

#else //__qdsp6__ || __hexagon__

uint32_t _adsp_listener_atomic_CompareAndExchange(uint32_t * volatile puDest, uint32_t uExchange, uint32_t uCompare);

#ifdef _WIN32
#include "Windows.h"
uint32_t _adsp_listener_atomic_CompareAndExchange(uint32_t * volatile puDest, uint32_t uExchange, uint32_t uCompare) {
   return (uint32_t)InterlockedCompareExchange((volatile LONG*)puDest, (LONG)uExchange, (LONG)uCompare);
}
#elif __GNUC__
uint32_t _adsp_listener_atomic_CompareAndExchange(uint32_t * volatile puDest, uint32_t uExchange, uint32_t uCompare) {
   return __sync_val_compare_and_swap(puDest, uCompare, uExchange);
}
#endif //_WIN32


__QAIC_STUB_EXPORT remote_handle _adsp_listener_handle(void) {
   static remote_handle handle = _const_adsp_listener_handle;
   if((remote_handle)-1 != handle) {
      return handle;
   } else {
      remote_handle tmp;
      int nErr = _adsp_listener_pls_ctor("adsp_listener", (void*)&tmp);
      if(nErr) {
         return (remote_handle)-1;
      }
      if(((remote_handle)-1 != handle) || ((remote_handle)-1 != (remote_handle)_adsp_listener_atomic_CompareAndExchange((uint32_t*)&handle, (uint32_t)tmp, (uint32_t)-1))) {
         _adsp_listener_pls_dtor(&tmp);
      }
      return handle;
   }
}

#endif //__qdsp6__

__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_skel_invoke)(uint32_t _sc, remote_arg* _pra) __QAIC_STUB_ATTRIBUTE {
   return __QAIC_REMOTE(remote_handle_invoke)(_adsp_listener_handle(), _sc, _pra);
}

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
static __inline int _stub_unpack(remote_arg* _praROutPost, remote_arg* _ppraROutPost[1], void* _primROut, char* _rout0[1], uint32_t _rout0Len[1]) {
   int _nErr = 0;
   remote_arg* _praROutPostStart = _praROutPost;
   remote_arg** _ppraROutPostStart = _ppraROutPost;
   _ppraROutPost = &_praROutPost;
   _ppraROutPostStart[0] += (_praROutPost - _praROutPostStart) +1;
   return _nErr;
}
static __inline int _stub_unpack_1(remote_arg* _praROutPost, remote_arg* _ppraROutPost[1], void* _primROut, char* _in0[1], uint32_t _in0Len[1]) {
   int _nErr = 0;
   remote_arg* _praROutPostStart = _praROutPost;
   remote_arg** _ppraROutPostStart = _ppraROutPost;
   _ppraROutPost = &_praROutPost;
   _ppraROutPostStart[0] += (_praROutPost - _praROutPostStart) +0;
   return _nErr;
}
static __inline int _stub_pack(allocator* _al, remote_arg* _praIn, remote_arg* _ppraIn[1], remote_arg* _praROut, remote_arg* _ppraROut[1], void* _primIn, void* _primROut, char* _rout0[1], uint32_t _rout0Len[1]) {
   int _nErr = 0;
   remote_arg* _praInStart = _praIn;
   remote_arg** _ppraInStart = _ppraIn;
   remote_arg* _praROutStart = _praROut;
   remote_arg** _ppraROutStart = _ppraROut;
   _ppraIn = &_praIn;
   _ppraROut = &_praROut;
   _COPY(_primIn, 0, _rout0Len, 0, 4);
   _praROut[0].buf.pv = _rout0[0];
   _praROut[0].buf.nLen = (1 * _rout0Len[0]);
   _ppraInStart[0] += (_praIn - _praInStart) + 0;
   _ppraROutStart[0] += (_praROut - _praROutStart) +1;
   return _nErr;
}
static __inline int _stub_pack_1(allocator* _al, remote_arg* _praIn, remote_arg* _ppraIn[1], remote_arg* _praROut, remote_arg* _ppraROut[1], void* _primIn, void* _primROut, char* _in0[1], uint32_t _in0Len[1]) {
   int _nErr = 0;
   remote_arg* _praInStart = _praIn;
   remote_arg** _ppraInStart = _ppraIn;
   remote_arg* _praROutStart = _praROut;
   remote_arg** _ppraROutStart = _ppraROut;
   _ppraIn = &_praIn;
   _ppraROut = &_praROut;
   _COPY(_primIn, 0, _in0Len, 0, 4);
   _praIn[0].buf.pv = _in0[0];
   _praIn[0].buf.nLen = (1 * _in0Len[0]);
   _ppraInStart[0] += (_praIn - _praInStart) + 1;
   _ppraROutStart[0] += (_praROut - _praROutStart) +0;
   return _nErr;
}
static __inline void _count(int _numIn[1], int _numROut[1], char* _rout0[1], uint32_t _rout0Len[1]) {
   _numIn[0] += 0;
   _numROut[0] += 1;
}
static __inline void _count_1(int _numIn[1], int _numROut[1], char* _in0[1], uint32_t _in0Len[1]) {
   _numIn[0] += 1;
   _numROut[0] += 0;
}
static __inline int _stub_method(remote_handle _handle, uint32_t _mid, uint32_t _in0[1], uint32_t _in1[1], void* _in2[1], uint32_t _in2Len[1], uint32_t _rout3[1], uint32_t _rout4[1], uint32_t _rout5[1], void* _rout6[1], uint32_t _rout6Len[1], char* _rout7[1], uint32_t _rout7Len[1], char* _rout8[1], uint32_t _rout8Len[1]) {
   remote_arg* _pra;
   int _numIn[1];
   int _numROut[1];
   char* _seq_nat2;
   int _ii;
   char* _seq_nat6;
   allocator _al[1] = {{0}};
   uint32_t _primIn[6];
   uint32_t _primROut[3];
   remote_arg* _praIn;
   remote_arg* _praROut;
   remote_arg* _praROutPost;
   remote_arg** _ppraROutPost = &_praROutPost;
   remote_arg** _ppraIn = &_praIn;
   remote_arg** _ppraROut = &_praROut;
   char* _seq_primIn2;
   int _nErr = 0;
   char* _seq_primIn6;
   _numIn[0] = 2;
   _numROut[0] = 2;
   for(_ii = 0, _seq_nat2 = (char*)_in2[0];_ii < (int)_in2Len[0];++_ii, _seq_nat2 = (_seq_nat2 + 8))
   {
      _count_1(_numIn, _numROut, (char**)&(((uint32_t*)_seq_nat2)[0]), (uint32_t*)&(((uint32_t*)_seq_nat2)[1]));
   }
   for(_ii = 0, _seq_nat6 = (char*)_rout6[0];_ii < (int)_rout6Len[0];++_ii, _seq_nat6 = (_seq_nat6 + 8))
   {
      _count(_numIn, _numROut, (char**)&(((uint32_t*)_seq_nat6)[0]), (uint32_t*)&(((uint32_t*)_seq_nat6)[1]));
   }
   allocator_init(_al, 0, 0);
   _ALLOCATE(_nErr, _al, ((((_numIn[0] + _numROut[0]) + 1) + 1) * sizeof(_pra[0])), 4, _pra);
   _pra[0].buf.pv = (void*)_primIn;
   _pra[0].buf.nLen = sizeof(_primIn);
   _pra[(_numIn[0] + 1)].buf.pv = (void*)_primROut;
   _pra[(_numIn[0] + 1)].buf.nLen = sizeof(_primROut);
   _praIn = (_pra + 1);
   _praROut = (_praIn + _numIn[0] + 1);
   _praROutPost = _praROut;
   _COPY(_primIn, 0, _in0, 0, 4);
   _COPY(_primIn, 4, _in1, 0, 4);
   _COPY(_primIn, 8, _in2Len, 0, 4);
   _ALLOCATE(_nErr, _al, (_in2Len[0] * 4), 4, _praIn[0].buf.pv);
   _praIn[0].buf.nLen = (4 * _in2Len[0]);
   for(_ii = 0, _seq_primIn2 = (char*)_praIn[0].buf.pv, _seq_nat2 = (char*)_in2[0];_ii < (int)_in2Len[0];++_ii, _seq_primIn2 = (_seq_primIn2 + 4), _seq_nat2 = (_seq_nat2 + 8))
   {
      _TRY(_nErr, _stub_pack_1(_al, (_praIn + 1), _ppraIn, (_praROut + 0), _ppraROut, _seq_primIn2, 0, (char**)&(((uint32_t*)_seq_nat2)[0]), (uint32_t*)&(((uint32_t*)_seq_nat2)[1])));
   }
   _COPY(_primIn, 12, _rout6Len, 0, 4);
   _ALLOCATE(_nErr, _al, (_rout6Len[0] * 4), 4, _praIn[1].buf.pv);
   _praIn[1].buf.nLen = (4 * _rout6Len[0]);
   for(_ii = 0, _seq_primIn6 = (char*)_praIn[1].buf.pv, _seq_nat6 = (char*)_rout6[0];_ii < (int)_rout6Len[0];++_ii, _seq_primIn6 = (_seq_primIn6 + 4), _seq_nat6 = (_seq_nat6 + 8))
   {
      _TRY(_nErr, _stub_pack(_al, (_praIn + 2), _ppraIn, (_praROut + 0), _ppraROut, _seq_primIn6, 0, (char**)&(((uint32_t*)_seq_nat6)[0]), (uint32_t*)&(((uint32_t*)_seq_nat6)[1])));
   }
   _COPY(_primIn, 16, _rout7Len, 0, 4);
   _praROut[0].buf.pv = _rout7[0];
   _praROut[0].buf.nLen = (4 * _rout7Len[0]);
   _COPY(_primIn, 20, _rout8Len, 0, 4);
   _praROut[1].buf.pv = _rout8[0];
   _praROut[1].buf.nLen = (4 * _rout8Len[0]);
   _TRY(_nErr, __QAIC_REMOTE(remote_handle_invoke)(_handle, REMOTE_SCALARS_MAKEX(0, _mid, (_numIn[0] + 1), (_numROut[0] + 1), 0, 0), _pra));
   for(_ii = 0, _seq_nat2 = (char*)_in2[0];_ii < (int)_in2Len[0];++_ii, _seq_nat2 = (_seq_nat2 + 8))
   {
      _TRY(_nErr, _stub_unpack_1((_praROutPost + 0), _ppraROutPost, 0, (char**)&(((uint32_t*)_seq_nat2)[0]), (uint32_t*)&(((uint32_t*)_seq_nat2)[1])));
   }
   _COPY(_rout3, 0, _primROut, 0, 4);
   _COPY(_rout4, 0, _primROut, 4, 4);
   _COPY(_rout5, 0, _primROut, 8, 4);
   for(_ii = 0, _seq_nat6 = (char*)_rout6[0];_ii < (int)_rout6Len[0];++_ii, _seq_nat6 = (_seq_nat6 + 8))
   {
      _TRY(_nErr, _stub_unpack((_praROutPost + 0), _ppraROutPost, 0, (char**)&(((uint32_t*)_seq_nat6)[0]), (uint32_t*)&(((uint32_t*)_seq_nat6)[1])));
   }
   _CATCH(_nErr) {}
   allocator_deinit(_al);
   return _nErr;
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_next_invoke)(adsp_listener_invoke_ctx prevCtx, int prevResult, const adsp_listener_buffer* outBufs, int outBufsLen, adsp_listener_invoke_ctx* ctx, adsp_listener_remote_handle* handle, uint32* sc, adsp_listener_buffer* inBuffers, int inBuffersLen, int* inBufLenReq, int inBufLenReqLen, int* routBufLenReq, int routBufLenReqLen) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 0;
   return _stub_method(_adsp_listener_handle(), _mid, (uint32_t*)&prevCtx, (uint32_t*)&prevResult, (void**)&outBufs, (uint32_t*)&outBufsLen, (uint32_t*)ctx, (uint32_t*)handle, (uint32_t*)sc, (void**)&inBuffers, (uint32_t*)&inBuffersLen, (char**)&inBufLenReq, (uint32_t*)&inBufLenReqLen, (char**)&routBufLenReq, (uint32_t*)&routBufLenReqLen);
}
static __inline int _stub_unpack_2(remote_arg* _praROutPost, remote_arg* _ppraROutPost[1], void* _primROut, char* _rout0[1], uint32_t _rout0Len[1]) {
   int _nErr = 0;
   remote_arg* _praROutPostStart = _praROutPost;
   remote_arg** _ppraROutPostStart = _ppraROutPost;
   _ppraROutPost = &_praROutPost;
   _ppraROutPostStart[0] += (_praROutPost - _praROutPostStart) +1;
   return _nErr;
}
static __inline int _stub_pack_2(allocator* _al, remote_arg* _praIn, remote_arg* _ppraIn[1], remote_arg* _praROut, remote_arg* _ppraROut[1], void* _primIn, void* _primROut, char* _rout0[1], uint32_t _rout0Len[1]) {
   int _nErr = 0;
   remote_arg* _praInStart = _praIn;
   remote_arg** _ppraInStart = _ppraIn;
   remote_arg* _praROutStart = _praROut;
   remote_arg** _ppraROutStart = _ppraROut;
   _ppraIn = &_praIn;
   _ppraROut = &_praROut;
   _COPY(_primIn, 0, _rout0Len, 0, 4);
   _praROut[0].buf.pv = _rout0[0];
   _praROut[0].buf.nLen = (1 * _rout0Len[0]);
   _ppraInStart[0] += (_praIn - _praInStart) + 0;
   _ppraROutStart[0] += (_praROut - _praROutStart) +1;
   return _nErr;
}
static __inline int _stub_method_1(remote_handle _handle, uint32_t _mid, uint32_t _in0[1], void* _rout1[1], uint32_t _rout1Len[1]) {
   remote_arg* _pra;
   int _numIn[1];
   int _numROut[1];
   char* _seq_nat1;
   int _ii;
   allocator _al[1] = {{0}};
   uint32_t _primIn[2];
   remote_arg* _praIn;
   remote_arg* _praROut;
   remote_arg* _praROutPost;
   remote_arg** _ppraROutPost = &_praROutPost;
   remote_arg** _ppraIn = &_praIn;
   remote_arg** _ppraROut = &_praROut;
   char* _seq_primIn1;
   int _nErr = 0;
   _numIn[0] = 1;
   _numROut[0] = 0;
   for(_ii = 0, _seq_nat1 = (char*)_rout1[0];_ii < (int)_rout1Len[0];++_ii, _seq_nat1 = (_seq_nat1 + 8))
   {
      _count(_numIn, _numROut, (char**)&(((uint32_t*)_seq_nat1)[0]), (uint32_t*)&(((uint32_t*)_seq_nat1)[1]));
   }
   allocator_init(_al, 0, 0);
   _ALLOCATE(_nErr, _al, ((((_numIn[0] + _numROut[0]) + 1) + 0) * sizeof(_pra[0])), 4, _pra);
   _pra[0].buf.pv = (void*)_primIn;
   _pra[0].buf.nLen = sizeof(_primIn);
   _praIn = (_pra + 1);
   _praROut = (_praIn + _numIn[0] + 0);
   _praROutPost = _praROut;
   _COPY(_primIn, 0, _in0, 0, 4);
   _COPY(_primIn, 4, _rout1Len, 0, 4);
   _ALLOCATE(_nErr, _al, (_rout1Len[0] * 4), 4, _praIn[0].buf.pv);
   _praIn[0].buf.nLen = (4 * _rout1Len[0]);
   for(_ii = 0, _seq_primIn1 = (char*)_praIn[0].buf.pv, _seq_nat1 = (char*)_rout1[0];_ii < (int)_rout1Len[0];++_ii, _seq_primIn1 = (_seq_primIn1 + 4), _seq_nat1 = (_seq_nat1 + 8))
   {
      _TRY(_nErr, _stub_pack_2(_al, (_praIn + 1), _ppraIn, (_praROut + 0), _ppraROut, _seq_primIn1, 0, (char**)&(((uint32_t*)_seq_nat1)[0]), (uint32_t*)&(((uint32_t*)_seq_nat1)[1])));
   }
   _TRY(_nErr, __QAIC_REMOTE(remote_handle_invoke)(_handle, REMOTE_SCALARS_MAKEX(0, _mid, (_numIn[0] + 1), (_numROut[0] + 0), 0, 0), _pra));
   for(_ii = 0, _seq_nat1 = (char*)_rout1[0];_ii < (int)_rout1Len[0];++_ii, _seq_nat1 = (_seq_nat1 + 8))
   {
      _TRY(_nErr, _stub_unpack_2((_praROutPost + 0), _ppraROutPost, 0, (char**)&(((uint32_t*)_seq_nat1)[0]), (uint32_t*)&(((uint32_t*)_seq_nat1)[1])));
   }
   _CATCH(_nErr) {}
   allocator_deinit(_al);
   return _nErr;
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_invoke_get_in_bufs)(adsp_listener_invoke_ctx ctx, adsp_listener_buffer* inBuffers, int inBuffersLen) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 1;
   return _stub_method_1(_adsp_listener_handle(), _mid, (uint32_t*)&ctx, (void**)&inBuffers, (uint32_t*)&inBuffersLen);
}
static __inline int _stub_method_2(remote_handle _handle, uint32_t _mid) {
   remote_arg* _pra = 0;
   int _nErr = 0;
   _TRY(_nErr, __QAIC_REMOTE(remote_handle_invoke)(_handle, REMOTE_SCALARS_MAKEX(0, _mid, 0, 0, 0, 0), _pra));
   _CATCH(_nErr) {}
   return _nErr;
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_init)(void) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 2;
   return _stub_method_2(_adsp_listener_handle(), _mid);
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_init2)(void) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 3;
   return _stub_method_2(_adsp_listener_handle(), _mid);
}
static __inline int _stub_method_3(remote_handle _handle, uint32_t _mid, uint32_t _in0[1], uint32_t _in1[1], char* _in2[1], uint32_t _in2Len[1], uint32_t _rout3[1], uint32_t _rout4[1], uint32_t _rout5[1], char* _rout6[1], uint32_t _rout6Len[1], uint32_t _rout7[1]) {
   int _numIn[1];
   remote_arg _pra[4];
   uint32_t _primIn[4];
   uint32_t _primROut[4];
   remote_arg* _praIn;
   remote_arg* _praROut;
   int _nErr = 0;
   _numIn[0] = 1;
   _pra[0].buf.pv = (void*)_primIn;
   _pra[0].buf.nLen = sizeof(_primIn);
   _pra[(_numIn[0] + 1)].buf.pv = (void*)_primROut;
   _pra[(_numIn[0] + 1)].buf.nLen = sizeof(_primROut);
   _COPY(_primIn, 0, _in0, 0, 4);
   _COPY(_primIn, 4, _in1, 0, 4);
   _COPY(_primIn, 8, _in2Len, 0, 4);
   _praIn = (_pra + 1);
   _praIn[0].buf.pv = _in2[0];
   _praIn[0].buf.nLen = (1 * _in2Len[0]);
   _COPY(_primIn, 12, _rout6Len, 0, 4);
   _praROut = (_praIn + _numIn[0] + 1);
   _praROut[0].buf.pv = _rout6[0];
   _praROut[0].buf.nLen = (1 * _rout6Len[0]);
   _TRY(_nErr, __QAIC_REMOTE(remote_handle_invoke)(_handle, REMOTE_SCALARS_MAKEX(0, _mid, 2, 2, 0, 0), _pra));
   _COPY(_rout3, 0, _primROut, 0, 4);
   _COPY(_rout4, 0, _primROut, 4, 4);
   _COPY(_rout5, 0, _primROut, 8, 4);
   _COPY(_rout7, 0, _primROut, 12, 4);
   _CATCH(_nErr) {}
   return _nErr;
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_next2)(adsp_listener_invoke_ctx prevCtx, int prevResult, const uint8* prevbufs, int prevbufsLen, adsp_listener_invoke_ctx* ctx, adsp_listener_remote_handle* handle, uint32* sc, uint8* bufs, int bufsLen, int* bufsLenReq) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 4;
   return _stub_method_3(_adsp_listener_handle(), _mid, (uint32_t*)&prevCtx, (uint32_t*)&prevResult, (char**)&prevbufs, (uint32_t*)&prevbufsLen, (uint32_t*)ctx, (uint32_t*)handle, (uint32_t*)sc, (char**)&bufs, (uint32_t*)&bufsLen, (uint32_t*)bufsLenReq);
}
static __inline int _stub_method_4(remote_handle _handle, uint32_t _mid, uint32_t _in0[1], uint32_t _in1[1], char* _rout2[1], uint32_t _rout2Len[1], uint32_t _rout3[1]) {
   int _numIn[1];
   remote_arg _pra[3];
   uint32_t _primIn[3];
   uint32_t _primROut[1];
   remote_arg* _praIn;
   remote_arg* _praROut;
   int _nErr = 0;
   _numIn[0] = 0;
   _pra[0].buf.pv = (void*)_primIn;
   _pra[0].buf.nLen = sizeof(_primIn);
   _pra[(_numIn[0] + 1)].buf.pv = (void*)_primROut;
   _pra[(_numIn[0] + 1)].buf.nLen = sizeof(_primROut);
   _COPY(_primIn, 0, _in0, 0, 4);
   _COPY(_primIn, 4, _in1, 0, 4);
   _COPY(_primIn, 8, _rout2Len, 0, 4);
   _praIn = (_pra + 1);
   _praROut = (_praIn + _numIn[0] + 1);
   _praROut[0].buf.pv = _rout2[0];
   _praROut[0].buf.nLen = (1 * _rout2Len[0]);
   _TRY(_nErr, __QAIC_REMOTE(remote_handle_invoke)(_handle, REMOTE_SCALARS_MAKEX(0, _mid, 1, 2, 0, 0), _pra));
   _COPY(_rout3, 0, _primROut, 0, 4);
   _CATCH(_nErr) {}
   return _nErr;
}
__QAIC_STUB_EXPORT int __QAIC_STUB(adsp_listener_get_in_bufs2)(adsp_listener_invoke_ctx ctx, int offset, uint8* bufs, int bufsLen, int* bufsLenReq) __QAIC_STUB_ATTRIBUTE {
   uint32_t _mid = 5;
   return _stub_method_4(_adsp_listener_handle(), _mid, (uint32_t*)&ctx, (uint32_t*)&offset, (char**)&bufs, (uint32_t*)&bufsLen, (uint32_t*)bufsLenReq);
}
#ifdef __cplusplus
}
#endif
#endif //_ADSP_LISTENER_STUB_H
