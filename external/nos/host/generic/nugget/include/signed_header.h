/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_UTIL_SIGNER_COMMON_SIGNED_HEADER_H
#define __EC_UTIL_SIGNER_COMMON_SIGNED_HEADER_H

#ifdef __cplusplus
#include <endian.h>
#include <stdio.h>
#include <time.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#define FUSE_MAX 128
#define INFO_MAX 128
#define FUSE_PADDING 0x55555555

// B chips
#define FUSE_IGNORE_B 0xa3badaac  // baked in rom!
#define INFO_IGNORE_B 0xaa3c55c3  // baked in rom!

// Citadel chips
#define FUSE_IGNORE_C 0x3aabadac  // baked in rom!
#define INFO_IGNORE_C 0xa5c35a3c  // baked in rom!

// Dauntless chips
#define FUSE_IGNORE_D 0xdaa3baca  // baked in rom!
#define INFO_IGNORE_D 0x5a3ca5c3  // baked in rom!

#if defined(CHIP_D)
#define FUSE_IGNORE FUSE_IGNORE_D
#define INFO_IGNORE INFO_IGNORE_D
#elif defined(CHIP_C)
#define FUSE_IGNORE FUSE_IGNORE_C
#define INFO_IGNORE INFO_IGNORE_C
#else
#define FUSE_IGNORE FUSE_IGNORE_B
#define INFO_IGNORE INFO_IGNORE_B
#endif

#define SIGNED_HEADER_MAGIC_HAVEN (-1u)
#define SIGNED_HEADER_MAGIC_CITADEL (-2u)
#define SIGNED_HEADER_MAGIC_DAUNTLESS (-3u)

/* Default value for _pad[] words */
#define SIGNED_HEADER_PADDING 0x33333333

typedef struct SignedHeader {
#ifdef __cplusplus
  SignedHeader()
      : magic(SIGNED_HEADER_MAGIC_HAVEN),
        image_size(0),
        epoch_(0x1337),
        major_(0),
        minor_(0xbabe),
        p4cl_(0),
        applysec_(0),
        config1_(0),
        err_response_(0),
        expect_response_(0),
        swap_mark({0, 0}),
        dev_id0_(0),
        dev_id1_(0) {
    memset(signature, 'S', sizeof(signature));
    memset(tag, 'T', sizeof(tag));
    memset(fusemap, 0, sizeof(fusemap));
    memset(infomap, 0, sizeof(infomap));
    memset(&_pad, SIGNED_HEADER_PADDING, sizeof(_pad));
    // Below all evolved out of _pad, thus must also be initialized to '3'
    // for backward compatibility.
    memset(&rw_product_family_, SIGNED_HEADER_PADDING,
           sizeof(rw_product_family_));
    memset(&u, SIGNED_HEADER_PADDING, sizeof(u));
    memset(&board_id_, SIGNED_HEADER_PADDING, sizeof(board_id_));
  }

  void markFuse(uint32_t n) {
    assert(n < FUSE_MAX);
    fusemap[n / 32] |= 1 << (n & 31);
  }

  void markInfo(uint32_t n) {
    assert(n < INFO_MAX);
    infomap[n / 32] |= 1 << (n & 31);
  }

  static uint32_t fuseIgnore(bool c, bool d) {
    return d ? FUSE_IGNORE_D : c ? FUSE_IGNORE_C : FUSE_IGNORE_B;
  }

  static uint32_t infoIgnore(bool c, bool d) {
    return d ? INFO_IGNORE_D : c ? INFO_IGNORE_C : INFO_IGNORE_B;
  }

  bool plausible() const {
    switch (magic) {
      case SIGNED_HEADER_MAGIC_HAVEN:
      case SIGNED_HEADER_MAGIC_CITADEL:
      case SIGNED_HEADER_MAGIC_DAUNTLESS:
        break;
      default:
        return false;
    }
    if (keyid == -1u) return false;
    if (ro_base >= ro_max) return false;
    if (rx_base >= rx_max) return false;
    if (_pad[0] != SIGNED_HEADER_PADDING) return false;
    return true;
  }

  void print() const {
    printf("hdr.magic          : %08x (", magic);
    switch (magic) {
      case SIGNED_HEADER_MAGIC_HAVEN:
        printf("Haven B");
        break;
      case SIGNED_HEADER_MAGIC_CITADEL:
        printf("Citadel");
        break;
      case SIGNED_HEADER_MAGIC_DAUNTLESS:
        printf("Dauntless");
        break;
      default:
        printf("?");
        break;
    }
    printf(")\n");
    printf("hdr.ro_base        : %08x\n", ro_base);
    printf("hdr.keyid          : %08x\n", keyid);
    printf("hdr.tag            : ");
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&tag);
    for (size_t i = 0; i < sizeof(tag); ++i) {
      printf("%02x", p[i] & 255);
    }
    printf("\n");
    printf("hdr.epoch          : %08x\n", epoch_);
    printf("hdr.major          : %08x\n", major_);
    printf("hdr.minor          : %08x\n", minor_);
    printf("hdr.timestamp      : %016" PRIx64 ", %s", timestamp_,
           asctime(localtime(reinterpret_cast<const time_t*>(&timestamp_))));
    printf("hdr.img_size       : %08x\n", image_size);
    printf("hdr.img_chk        : %08x\n", be32toh(img_chk_));
    printf("hdr.fuses_chk      : %08x\n", be32toh(fuses_chk_));
    printf("hdr.info_chk       : %08x\n", be32toh(info_chk_));
    printf("hdr.applysec       : %08x\n", applysec_);
    printf("hdr.config1        : %08x\n", config1_);
    printf("hdr.err_response   : %08x\n", err_response_);
    printf("hdr.expect_response: %08x\n", expect_response_);

    if (dev_id0_)
      printf("hdr.dev_id0        : %08x (%d)\n", dev_id0_, dev_id0_);
    if (dev_id1_)
      printf("hdr.dev_id1        : %08x (%d)\n", dev_id1_, dev_id1_);

    printf("hdr.fusemap        : ");
    for (size_t i = 0; i < sizeof(fusemap) / sizeof(fusemap[0]); ++i) {
      printf("%08X", fusemap[i]);
    }
    printf("\n");
    printf("hdr.infomap        : ");
    for (size_t i = 0; i < sizeof(infomap) / sizeof(infomap[0]); ++i) {
      printf("%08X", infomap[i]);
    }
    printf("\n");

    printf("hdr.board_id       : %08x %08x %08x\n",
           SIGNED_HEADER_PADDING ^ board_id_.type,
           SIGNED_HEADER_PADDING ^ board_id_.type_mask,
           SIGNED_HEADER_PADDING ^ board_id_.flags);
  }
#endif  // __cplusplus

  uint32_t magic;  // -1 (thanks, boot_sys!)
  uint32_t signature[96];
  uint32_t img_chk_;  // top 32 bit of expected img_hash
  // --------------------- everything below is part of img_hash
  uint32_t tag[7];   // words 0-6 of RWR/FWR
  uint32_t keyid;    // word 7 of RWR
  uint32_t key[96];  // public key to verify signature with
  uint32_t image_size;
  uint32_t ro_base;  // readonly region
  uint32_t ro_max;
  uint32_t rx_base;  // executable region
  uint32_t rx_max;
  uint32_t fusemap[FUSE_MAX / (8 * sizeof(uint32_t))];
  uint32_t infomap[INFO_MAX / (8 * sizeof(uint32_t))];
  uint32_t epoch_;  // word 7 of FWR
  uint32_t major_;  // keyladder count
  uint32_t minor_;
  uint64_t timestamp_;  // time of signing
  uint32_t p4cl_;
  uint32_t applysec_;      // bits to and with FUSE_FW_DEFINED_BROM_APPLYSEC
  uint32_t config1_;       // bits to mesh with FUSE_FW_DEFINED_BROM_CONFIG1
  uint32_t err_response_;  // bits to or with FUSE_FW_DEFINED_BROM_ERR_RESPONSE
  uint32_t expect_response_;  // action to take when expectation is violated

  union {
    // 2nd FIPS signature (cr51/cr52 RW)
    struct {
      uint32_t keyid;
      uint32_t r[8];
      uint32_t s[8];
    } ext_sig;
  } u;

  // Spare space
  uint32_t _pad[5];

  struct {
    unsigned size : 12;
    unsigned offset : 20;
  } swap_mark;
  uint32_t rw_product_family_;  // 0 == PRODUCT_FAMILY_ANY
                                // Stored as (^SIGNED_HEADER_PADDING)
                                // TODO(ntaha): add reference to product family
                                // enum when available.

  struct {
    // CR50 board class locking
    uint32_t type;       // Board type
    uint32_t type_mask;  // Mask of board type bits to use.
    uint32_t flags;      // Flags
  } board_id_;

  uint32_t dev_id0_;  // node id, if locked
  uint32_t dev_id1_;
  uint32_t fuses_chk_;  // top 32 bit of expected fuses hash
  uint32_t info_chk_;   // top 32 bit of expected info hash
} SignedHeader;

#ifdef __cplusplus
static_assert(sizeof(SignedHeader) == 1024,
              "SignedHeader should be 1024 bytes");
#ifndef GOOGLE3
static_assert(offsetof(SignedHeader, info_chk_) == 1020,
              "SignedHeader should be 1024 bytes");
#endif  // GOOGLE3
#else
_Static_assert(sizeof(SignedHeader) == 1024,
              "SignedHeader should be 1024 bytes");
#endif  // __cplusplus

#endif  // __EC_UTIL_SIGNER_COMMON_SIGNED_HEADER_H
