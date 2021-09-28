/*
 * Copyright (C) 2010-2014 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * TML I2C port implementation for linux
 */
/*#ifndef PHTMLNFCI2C_H
#define PHTMLNFCI2C_H*/
/* Basic type definitions */
#include <phNfcTypes.h>
#include <phTmlNfc.h>

#define PN544_MAGIC 0xE9

/* Function declarations */
void phTmlNfc_i2c_close(void* pDevHandle);
NFCSTATUS phTmlNfc_i2c_open_and_configure(pphTmlNfc_Config_t pConfig,
                                          void** pLinkHandle);
int phTmlNfc_i2c_read(void* pDevHandle, uint8_t* pBuffer, int nNbBytesToRead);
int phTmlNfc_i2c_write(void* pDevHandle, uint8_t* pBuffer, int nNbBytesToWrite);
int phTmlNfc_i2c_reset(void* pDevHandle, long level);
bool_t getDownloadFlag(void);
extern phTmlNfc_i2cfragmentation_t fragmentation_enabled;

/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN544_SET_PWR _IOW(PN544_MAGIC, 0x01, unsigned int)

NFCSTATUS phTmlNfc_i2c_get_p61_power_state(void* pDevHandle);
NFCSTATUS phTmlNfc_i2c_set_p61_power_state(void* pDevHandle, long arg);
NFCSTATUS phTmlNfc_set_pid(void* pDevHandle, long pid);
NFCSTATUS phTmlNfc_set_power_scheme(void* pDevHandle, long id);
NFCSTATUS phTmlNfc_get_ese_access(void* pDevHandle, long timeout);
NFCSTATUS phTmlNfc_i2c_set_Jcop_dwnld_state(void* pDevHandle, long level);
NFCSTATUS phTmlNfc_i2c_set_spm_state(void* pa_data, void* pDevHandle);
NFCSTATUS phTmlNfc_i2c_reset_spm_state(void* pa_data, void* pDevHandle);
NFCSTATUS phTmlNfc_rel_svdd_wait(void* pDevHandle);
NFCSTATUS phTmlNfc_rel_dwpOnOff_wait(void* pDevHandle);
/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPI_PWR _IOW(PN544_MAGIC, 0x02, unsigned int)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
*/
#define P61_GET_PWR_STATUS _IOR(PN544_MAGIC, 0x03, unsigned int)

/* DWP side this ioctl will be called
 * level 1 = Wired access is enabled/ongoing
 * level 0 = Wired access is disalbed/stopped
*/
#define P61_SET_WIRED_ACCESS _IOW(PN544_MAGIC, 0x04, unsigned int)

/*
  NFC Init will call the ioctl to register the PID with the i2c driver
*/
#define P544_SET_NFC_SERVICE_PID _IOW(PN544_MAGIC, 0x05, unsigned int)

/*
  NFC and SPI will call the ioctl to get the i2c/spi bus access
*/
#define P544_GET_ESE_ACCESS _IOW(PN544_MAGIC, 0x06, unsigned int)

/*
  NFC and SPI will call the ioctl to update the power scheme
*/
#define P544_SET_POWER_SCHEME _IOW(PN544_MAGIC, 0x07, unsigned int)
/*
  NFC will call the ioctl to release the svdd protection
*/
#define P544_REL_SVDD_WAIT _IOW(PN544_MAGIC, 0x08, unsigned int)
/* SPI or DWP can call this ioctl to set the JCOP download
 * state of P61
 *
*/
#define P544_SECURE_TIMER_SESSION _IOW(PN544_MAGIC, 0x0A, unsigned int)

#define PN544_SET_DWNLD_STATUS _IOW(PN544_MAGIC, 0x09, unsigned int)
/*
 * NFC will call the ioctlto release the dwp on/off protection
 */
#define P544_REL_DWPONOFF_WAIT _IOW(PN544_MAGIC, 0x0A, unsigned int)
//#endif
