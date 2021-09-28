/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/**
 * @file mem_req_and_acq.h
 * @brief Interface for mem request, acquiring and freeing
 * @author K. J. Nitthilan
 * @version 1.0
 * @date 2009-04-21
 */
#ifndef _MEM_REQ_AND_ACQ_H_
#define _MEM_REQ_AND_ACQ_H_

typedef enum
{
    ALIGN_BYTE = 1,
    ALIGN_WORD16 = 2,
    ALIGN_WORD32 = 4,
    ALIGN_WORD64 = 8,
    ALIGN_128_BYTE = 128
} ITT_MEM_ALIGNMENT_TYPE_E;

typedef enum
{
    SCRATCH = 0,
    PERSISTENT = 1,
    WRITEONCE = 2
} ITT_MEM_USAGE_TYPE_E;

typedef enum
{
    L1D = 0,
    SL2 = 1,
    DDR = 3
} ITT_MEM_REGION_E;

typedef enum
{
    GET_NUM_MEMTAB = 0,
    FILL_MEMTAB = 1,
    USE_BASE = 2,
    FILL_BASE = 3
} ITT_FUNC_TYPE_E;

/*NOTE : This should be an exact replica of IALG_MemRec, any change in IALG_MemRec
         must be replected here*/
typedef struct
{
    UWORD32 u4_size; /* Size in bytes */
    WORD32 i4_alignment; /* Alignment in bytes */
    ITT_MEM_REGION_E
    e_mem_region; /* decides which memory region to be placed */
    ITT_MEM_USAGE_TYPE_E e_usage; /* memory is scratch or persistent */
    void *pv_base; /* Base pointer for allocated memory */
} itt_memtab_t;

static __inline void fill_memtab(
    itt_memtab_t *ps_mem_tab,
    WORD32 u4_size,
    WORD32 i4_alignment,
    ITT_MEM_USAGE_TYPE_E e_usage,
    ITT_MEM_REGION_E e_mem_region)
{
    /* Make the size next multiple of alignment */
    WORD32 i4_aligned_size = (((u4_size) + (i4_alignment - 1)) & (~(i4_alignment - 1)));

    /* Fill the memtab */
    ps_mem_tab->u4_size = i4_aligned_size;
    ps_mem_tab->i4_alignment = i4_alignment;
    ps_mem_tab->e_usage = e_usage;
    ps_mem_tab->e_mem_region = e_mem_region;
}

static __inline WORD32
    use_or_fill_base(itt_memtab_t *ps_mem_tab, void **ptr_to_be_filled, ITT_FUNC_TYPE_E e_func_type)
{
    /* Fill base for freeing the allocated memory */
    if(e_func_type == FILL_BASE)
    {
        if(ptr_to_be_filled[0] != 0)
        {
            ps_mem_tab->pv_base = ptr_to_be_filled[0];
            return (0);
        }
        else
        {
            return (-1);
        }
    }
    /* obtain the allocated memory from base pointer */
    if(e_func_type == USE_BASE)
    {
        if(ps_mem_tab->pv_base != 0)
        {
            ptr_to_be_filled[0] = ps_mem_tab->pv_base;
            return (0);
        }
        else
        {
            return (-1);
        }
    }
    return (0);
}

#endif /* _MEM_REQ_AND_ACQ_H_*/
