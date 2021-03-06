/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#if !defined(AVB_INSIDE_LIBAVB_AFTL_H) && !defined(AVB_COMPILATION)
#error "Never include this file directly, include libavb_aftl.h instead."
#endif

#ifndef AVB_AFTL_VERIFY_H_
#define AVB_AFTL_VERIFY_H_

#include <libavb/libavb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The entry point of AFTL validation. Takes an AvbSlotVerifyData
   struct generated by a prior call to avb_slot_verify and the transparency
   log's public key and validates the inclusion proof. It does this by
   performing the following three validation steps for an AFTL
   descriptor:
   1. Ensure the vbmeta image hash matches that in the descriptor.
   2. Ensure the root hash of the Merkle tree matches that in the descriptor.
   3. Verify the signature using the transparency log public key.
   It returns AVB_SLOT_VERIFY_RESULT_OK upon success,
   AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA if at least one VBMeta did not
   have an AftlDescriptor and AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION
   otherwise.
*/

AvbSlotVerifyResult aftl_slot_verify(AvbOps* ops,
                                     AvbSlotVerifyData* asv_data,
                                     uint8_t* key_bytes,
                                     size_t key_size);
#ifdef __cplusplus
}
#endif

#endif /* AVB_AFTL_VERIFY_H_ */
