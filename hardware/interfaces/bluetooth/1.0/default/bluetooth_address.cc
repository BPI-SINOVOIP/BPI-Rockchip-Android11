//
// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "bluetooth_address.h"

#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utils/Log.h>
#include <sys/time.h>

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

void BluetoothAddress::bytes_to_string(const uint8_t* addr, char* addr_str) {
  sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
          addr[3], addr[4], addr[5]);
}

bool BluetoothAddress::string_to_bytes(const char* addr_str, uint8_t* addr) {
  if (addr_str == NULL) return false;
  if (strnlen(addr_str, kStringLength) != kStringLength) return false;
  unsigned char trailing_char = '\0';

  return (sscanf(addr_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx%1c",
                 &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5],
                 &trailing_char) == kBytes);
}

#include <sys/ioctl.h>

typedef     unsigned short      uint16;
typedef     unsigned int       uint32;
typedef     unsigned char       uint8;

#define VENDOR_REQ_TAG      0x56524551
#define VENDOR_READ_IO      _IOW('v', 0x01, unsigned int)
#define VENDOR_WRITE_IO     _IOW('v', 0x02, unsigned int)

#define VENDOR_SN_ID        1
#define VENDOR_WIFI_MAC_ID  2
#define VENDOR_LAN_MAC_ID   3
#define VENDOR_BLUETOOTH_ID 4

struct rk_vendor_req {
    uint32 tag;
    uint16 id;
    uint16 len;
    uint8 data[1];
};

// rw: 0, read; 1, write
// return 0 is success, other fail
static int bt_addr_vendor_storage_read_or_write(int rw, char *buf, int len)
{
    int ret ;
    uint8 p_buf[64];
    struct rk_vendor_req *req;

    req = (struct rk_vendor_req *)p_buf;
    int sys_fd = open("/dev/vendor_storage",O_RDWR,0);
    if(sys_fd < 0){
        ALOGE("vendor_storage open fail\n");
        return -1;
    }

    req->tag = VENDOR_REQ_TAG;
    req->id = VENDOR_BLUETOOTH_ID;

    if (rw == 0) {
        req->len = len;
        ret = ioctl(sys_fd, VENDOR_READ_IO, req);
        if (!ret) {
            memcpy(buf, req->data, 6);
        }
    } else {
        req->len = len;
        for (int i = 0; i < 6; i++)
            req->data[i] = buf[i];
        ret = ioctl(sys_fd, VENDOR_WRITE_IO, req);
    }
    if(ret){
        ALOGE("vendor storage %s error\n", rw ? "write":"read");
        close(sys_fd);
        return -2;
    }

    ALOGE("vendor storage %s success\n", rw ? "write":"read");
    close(sys_fd);
    return 0;
}

bool BluetoothAddress::get_local_address(uint8_t* local_addr) {
  char property[PROPERTY_VALUE_MAX] = {0};
  bool valid_bda = false;
  const uint8_t null_bdaddr[kBytes] = {0,0,0,0,0,0};

      if (!valid_bda) {
        int ret;
        char bd_addr[6] = {0};
        ret = bt_addr_vendor_storage_read_or_write(0, bd_addr, 6);
        if (ret == 0) {
            memcpy(local_addr, bd_addr, 6);

            valid_bda = true;
            ALOGE("Got local bdaddr for vendor storage %02X:%02X:%02X:%02X:%02X:%02X",
                local_addr[0], local_addr[1], local_addr[2],
                local_addr[3], local_addr[4], local_addr[5]);
        } /*else if (ret == -2) {
            local_addr[0] = 0x22;
            local_addr[1] = 0x22;
	    local_addr[2] = (uint8_t)rand();
	    local_addr[3] = (uint8_t)rand();
	    local_addr[4] = (uint8_t)rand();
	    local_addr[5] = (uint8_t)rand();

            valid_bda = true;
            memcpy(bd_addr, local_addr, 6);
            ALOGE("Generate random bdaddr and save to vendor storage %02X:%02X:%02X:%02X:%02X:%02X",
                local_addr[0], local_addr[1], local_addr[2],
                local_addr[3], local_addr[4], local_addr[5]);
            bt_addr_vendor_storage_read_or_write(1, bd_addr, 6);
        }*/
    }

    if (!valid_bda) {// cmy@2012-11-28: Get local bdaddr from vflash
        int vflash_fd = open("/dev/vflash", O_RDONLY);
        if (vflash_fd > 0)
        {
            char bd_addr[6] = {0};
            ALOGE("Get local bdaddr from vflash");
            #define VFLASH_READ_BDA  0x01
            if(ioctl(vflash_fd, VFLASH_READ_BDA, (unsigned long)bd_addr) >= 0
                && memcmp(bd_addr, null_bdaddr, kBytes) != 0)
            {
                local_addr[0] = bd_addr[5];
                local_addr[1] = bd_addr[4];
                local_addr[2] = bd_addr[3];
                local_addr[3] = bd_addr[2];
                local_addr[4] = bd_addr[1];
                local_addr[5] = bd_addr[0];

                valid_bda = true;
                ALOGE("Got Factory BDA %02X:%02X:%02X:%02X:%02X:%02X",
                    local_addr[0], local_addr[1], local_addr[2],
                    local_addr[3], local_addr[4], local_addr[5]);
            }
            close(vflash_fd);
        }
    }

  // Get local bdaddr storage path from a system property.
  if (property_get(PROPERTY_BT_BDADDR_PATH, property, NULL)) {
    int addr_fd;

    ALOGD("%s: Trying %s", __func__, property);

    addr_fd = open(property, O_RDONLY);
    if (addr_fd != -1) {
      char address[kStringLength + 1] = {0};
      int bytes_read = read(addr_fd, address, kStringLength);
      if (bytes_read == -1) {
        ALOGE("%s: Error reading address from %s: %s", __func__, property,
              strerror(errno));
      }
      close(addr_fd);

      // Null terminate the string.
      address[kStringLength] = '\0';

      // If the address is not all zeros, then use it.
      const uint8_t zero_bdaddr[kBytes] = {0, 0, 0, 0, 0, 0};
      if ((string_to_bytes(address, local_addr)) &&
          (memcmp(local_addr, zero_bdaddr, kBytes) != 0)) {
        valid_bda = true;
        ALOGD("%s: Got Factory BDA %s", __func__, address);
      } else {
        ALOGE("%s: Got Invalid BDA '%s' from %s", __func__, address, property);
      }
    }
  }

  // No BDADDR found in the file. Look for BDA in a factory property.
  if (!valid_bda && property_get(FACTORY_BDADDR_PROPERTY, property, NULL) &&
      string_to_bytes(property, local_addr)) {
    valid_bda = true;
  }

  // No factory BDADDR found. Look for a previously stored BDA.
  if (!valid_bda && property_get(PERSIST_BDADDR_PROPERTY, property, NULL) &&
      string_to_bytes(property, local_addr)) {
    valid_bda = true;
  }

  /* Generate new BDA if necessary */
  if (!valid_bda) {
    char bdstr[kStringLength + 1];
    struct	timeval    tv;
    struct	timezone   tz;
    gettimeofday(&tv,&tz);

    /* No autogen BDA. Generate one now. */
    local_addr[0] = 0x22;
    local_addr[1] = 0x22;
    //local_addr[2] = (uint8_t)rand();
    //local_addr[3] = (uint8_t)rand();
    //local_addr[4] = (uint8_t)rand();
    //local_addr[5] = (uint8_t)rand();
    local_addr[2]	= (uint8_t) (tv.tv_usec & 0xFF);
    local_addr[3]	= (uint8_t) ((tv.tv_usec >> 8) & 0xFF);
    local_addr[4] = (uint8_t) ((tv.tv_usec >> 16) & 0xFF);
    local_addr[5] = (uint8_t) ((tv.tv_usec >> 24) & 0xFF);

    /* Convert to ascii, and store as a persistent property */
    bytes_to_string(local_addr, bdstr);

    ALOGE("%s: No preset BDA! Generating BDA: %s for prop %s", __func__,
          (char*)bdstr, PERSIST_BDADDR_PROPERTY);
    ALOGE("%s: This is a bug in the platform!  Please fix!", __func__);

    if (property_set(PERSIST_BDADDR_PROPERTY, (char*)bdstr) < 0) {
      ALOGE("%s: Failed to set random BDA in prop %s", __func__,
            PERSIST_BDADDR_PROPERTY);
      valid_bda = false;
    } else {
      valid_bda = true;
    }
  }

  return valid_bda;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
