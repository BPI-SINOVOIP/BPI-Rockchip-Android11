/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#define LOG_TAG "NxpEseHal"
#include <log/log.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ese_config.h>
#include <phEseStatus.h>
#include <phNxpEsePal_spi.h>
#include <string.h>

extern bool ese_debug_enabled;

/*!
 * \brief Normal mode header length
 */
#define NORMAL_MODE_HEADER_LEN 3
/*!
 * \brief Normal mode header offset
 */
#define NORMAL_MODE_LEN_OFFSET 2
/*!
 * \brief Start of frame marker
 */
#define SEND_PACKET_SOF 0x5A
/*!
 * \brief To enable SPI interface for ESE communication
 */
#define SPI_ENABLED 1
/*******************************************************************************
**
** Function         phPalEse_close
**
** Description      Closes PN547 device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phPalEse_close(void* pDevHandle) {
  if (NULL != pDevHandle) {
#ifdef SPI_ENABLED
    phPalEse_spi_close(pDevHandle);
#else
/* RFU */
#endif
  }
  return;
}

/*******************************************************************************
**
** Function         phPalEse_open_and_configure
**
** Description      Open and configure ESE device
**
** Parameters       pConfig     - hardware information
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - open_and_configure operation
*success
**                  ESESTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_open_and_configure(pphPalEse_Config_t pConfig) {
  ESESTATUS status = ESESTATUS_FAILED;
#ifdef SPI_ENABLED
  status = phPalEse_spi_open_and_configure(pConfig);
#else
/* RFU */
#endif
  return status;
}

/*******************************************************************************
**
** Function         phPalEse_read
**
** Description      Reads requested number of bytes from pn547 device into given
*buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
int phPalEse_read(void* pDevHandle, uint8_t* pBuffer, int nNbBytesToRead) {
  int ret = -1;
#ifdef SPI_ENABLED
  ret = phPalEse_spi_read(pDevHandle, pBuffer, nNbBytesToRead);
#else
/* RFU */
#endif
  return ret;
}

/*******************************************************************************
**
** Function         phPalEse_write
**
** Description      Writes requested number of bytes from given buffer into
*pn547 device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phPalEse_write(void* pDevHandle, uint8_t* pBuffer, int nNbBytesToWrite) {
  int numWrote = 0;

  if (NULL == pDevHandle) {
    return -1;
  }
#ifdef SPI_ENABLED
  numWrote = phPalEse_spi_write(pDevHandle, pBuffer, nNbBytesToWrite);
#else
/* RFU */
#endif
  return numWrote;
}

/*******************************************************************************
**
** Function         phPalEse_ioctl
**
** Description      Exposed ioctl by p61 spi driver
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - ioctl operation success
**                  -1   - ioctl operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_ioctl(phPalEse_ControlCode_t eControlCode, void* pDevHandle,
                         long level) {
  ESESTATUS ret = ESESTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "phPalEse_spi_ioctl(), ioctl %x , level %lx",
           eControlCode, level);

  if (NULL == pDevHandle) {
    return ESESTATUS_IOCTL_FAILED;
  }
#ifdef SPI_ENABLED
  ret = phPalEse_spi_ioctl(eControlCode, pDevHandle, level);
#else
/* RFU */
#endif
  return ret;
}

/*******************************************************************************
**
** Function         phPalEse_print_packet
**
** Description      Print packet
**
** Returns          None
**
*******************************************************************************/
void phPalEse_print_packet(const char* pString, const uint8_t* p_data,
                           uint16_t len) {
  uint32_t i;
  char print_buffer[len * 3 + 1];

  memset(print_buffer, 0, sizeof(print_buffer));
  for (i = 0; i < len; i++) {
    snprintf(&print_buffer[i * 2], 3, "%02X", p_data[i]);
  }
  if (0 == memcmp(pString, "SEND", 0x04)) {
    ALOGD_IF(ese_debug_enabled, "NxpEseDataX len = %3d > %s", len,
             print_buffer);
  } else if (0 == memcmp(pString, "RECV", 0x04)) {
    ALOGD_IF(ese_debug_enabled, "NxpEseDataR len = %3d > %s", len,
             print_buffer);
  }

  return;
}

/*******************************************************************************
**
** Function         phPalEse_sleep
**
** Description      This function  suspends execution of the calling thread for
**                  (at least) usec microseconds
**
** Returns          None
**
*******************************************************************************/
void phPalEse_sleep(long usec) {
  usleep(usec);
  return;
}

/**
 * \ingroup eSe_PAL
 * \brief This function updates destination buffer with val
 *                 data in len size
 *
 * \param[in]    buff                - Array to be udpated
 * \param[in]    val                 - value to be updated
 * \param[in]    len                 - length of array to be updated
 *
 * \retval   void
 *
 */
void* phPalEse_memset(void* buff, int val, size_t len) {
  return memset(buff, val, len);
}

/**
 * \ingroup eSe_PAL
 * \brief This function copies source buffer to  destination buffer
 *                 data in len size
 *
 * \param[in]    dest                - Destination array to be updated
 * \param[in]    src                 - Source array to be updated
 * \param[in]    len                 - length of array to be updated
 *
 * \retval   void
 *
 */
void* phPalEse_memcpy(void* dest, const void* src, size_t len) {
  return memcpy(dest, src, len);
}

/**
 * \ingroup eSe_PAL
 * \brief This is utility function for runtime heap memory allocation
 *
 * \param[in]    size                 - number of bytes to be allocated
 *
 * \retval   void
 *
 */
void* phPalEse_memalloc(uint32_t size) { return malloc(size); }

/**
 * \ingroup eSe_PAL
 * \brief This is utility function for runtime heap memory allocation
 *
 * \param[in]    len                 - number of bytes to be allocated
 *
 * \retval   void
 *
 */
void* phPalEse_calloc(size_t datatype, size_t size) {
  return calloc(datatype, size);
}

/**
 * \ingroup eSe_PAL
 * \brief This is utility function for freeeing heap memory allocated
 *
 * \param[in]    ptr                 - Address pointer to previous allocation
 *
 * \retval   void
 *
 */
void phPalEse_free(void* ptr) { return free(ptr); }
