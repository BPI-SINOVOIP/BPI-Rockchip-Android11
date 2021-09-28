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

/**
 * \addtogroup spi_libese
 * \brief ESE Lib layer interface to application
 * @{ */

#ifndef _PHNXPSPILIB_API_H_
#define _PHNXPSPILIB_API_H_

#include <phEseStatus.h>

/**
 * \ingroup spi_libese
 * \brief Ese data buffer
 *
 */
typedef struct phNxpEse_data {
  uint32_t len;    /*!< length of the buffer */
  uint8_t* p_data; /*!< pointer to a buffer */
} phNxpEse_data;

/**
 * \ingroup spi_libese
 * \brief Ese Channel mode
 *
 */
typedef enum {
  ESE_MODE_NORMAL = 0, /*!< All wired transaction other OSU */
  ESE_MODE_OSU         /*!< Jcop Os update mode */
} phNxpEse_initMode;

/**
 * \ingroup spi_libese
 * \brief Ese library init parameters to be set while calling phNxpEse_init
 *
 */
typedef struct phNxpEse_initParams {
  phNxpEse_initMode initMode; /*!< Ese communication mode */
} phNxpEse_initParams;

/*!
 * \brief SEAccess kit MW Android version
 */
#define NXP_ANDROID_VER (8U)

/*!
 * \brief SEAccess kit MW Major version
 */
#define ESELIB_MW_VERSION_MAJ (0x3)

/*!
 * \brief SEAccess kit MW Minor version
 */
#define ESELIB_MW_VERSION_MIN (0x00)

/******************************************************************************
 * \ingroup spi_libese
 *
 * \brief  It Initializes protocol stack instance variables
 *
 * \retval This function return ESESTATUS_SUCCES (0) in case of success
 *         In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_init(phNxpEse_initParams initParams);

/******************************************************************************
 * \ingroup spi_libese
 *
 * \brief  Check if libese has opened
 *
 * \retval return false if it is close, otherwise true.
 *
 ******************************************************************************/
bool phNxpEse_isOpen();

/******************************************************************************
 * \ingroup spi_libese
 *
 * \brief  This function is used to communicate from nfc-hal to ese-hal
 *
 * \retval This function return ESESTATUS_SUCCES (0) in case of success
 *         In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_spiIoctl(uint64_t ioctlType, void* p_data);

/**
 * \ingroup spi_libese
 * \brief This function is called by Jni during the
 *        initialization of the ESE. It opens the physical connection
 *        with ESE () and initializes the protocol stack
 *
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *
 */
ESESTATUS phNxpEse_open(phNxpEse_initParams initParams);

/**
 * \ingroup spi_libese
 * \brief It opens the physical connectio with ESE () and creates required
 *        client thread for operation. This will get priority access to ESE
 *        for timeout period.
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *
 */
ESESTATUS phNxpEse_openPrioSession(phNxpEse_initParams initParams);

/**
 * \ingroup spi_libese
 * \brief This function prepares the C-APDU, send to ESE and then receives the
 *response from ESE,
 *         decode it and returns data.
 *
 * \param[in]       phNxpEse_data: Command to ESE
 * \param[out]     phNxpEse_data: Response from ESE (Returned data to be freed
 *after copying)
 *
 * \retval ESESTATUS_SUCCESS On Success ESESTATUS_SUCCESS else proper error code
 *
 */

ESESTATUS phNxpEse_Transceive(phNxpEse_data* pCmd, phNxpEse_data* pRsp);

/******************************************************************************
 * \ingroup spi_libese
 *
 * \brief  This function is called by Jni/phNxpEse_close during the
 *         de-initialization of the ESE. It de-initializes protocol stack
 *instance variables
 *
 * \retval This function return ESESTATUS_SUCCES (0) in case of success
 *         In case of failure returns other failure value.
 *
 ******************************************************************************/
ESESTATUS phNxpEse_deInit(void);

/**
 * \ingroup spi_libese
 * \brief This function close the ESE interface and free all resources.
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */

ESESTATUS phNxpEse_close(void);

/**
 * \ingroup spi_libese
 * \brief This function reset the ESE interface and free all
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_reset(void);

/**
 * \ingroup spi_libese
 * \brief This function reset the ESE
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_resetJcopUpdate(void);

/**
 * \ingroup spi_libese
 * \brief This function reset the P73 through ISO RST pin
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_chipReset(void);

/**
 * \ingroup spi_libese
 * \brief This function is used to set IFSC size
 *
 * \param[in]       uint16_t IFSC_Size
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_setIfsc(uint16_t IFSC_Size);

/**
 * \ingroup spi_libese
 * \brief This function sends the S-frame to indicate END_OF_APDU
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_EndOfApdu(void);

/**
 * \ingroup spi_libese
 * \brief This function  suspends execution of the calling thread for
 *           (at least) usec microseconds
 *
 * \param[in]       void
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_Sleep(uint32_t usec);

/**
 * \ingroup spi_libese
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
void* phNxpEse_memset(void* buff, int val, size_t len);

/**
 * \ingroup spi_libese
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
void* phNxpEse_memcpy(void* dest, const void* src, size_t len);

/**
 * \ingroup spi_libese
 * \brief This function  suspends allocate memory
 *
 * \param[in]       uint32_t size
 *
 * \retval allocated memory.
 *
 */
void* phNxpEse_memalloc(uint32_t size);

/**
 * \ingroup spi_libese
 * \brief This is utility function for runtime heap memory allocation
 *
 * \param[in]    len                 - number of bytes to be allocated
 *
 * \retval   void
 *
 */
void* phNxpEse_calloc(size_t dataType, size_t size);

/**
 * \ingroup spi_libese
 * \brief This is utility function for freeeing heap memory allocated
 *
 * \param[in]    ptr                 - Address pointer to previous allocation
 *
 * \retval   void
 *
 */
void phNxpEse_free(void* ptr);

/**
+ * \ingroup spi_libese
+ * \brief This function perfroms disbale/enable power control
+ *
+ * \param[in]       void
+ *
+ * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
+ *
+*/
ESESTATUS phNxpEse_DisablePwrCntrl(void);

/**
 * \ingroup spi_libese
 * \brief This function is used to get the ESE timer status
 *
 * \param[in]       phNxpEse_data timer_buffer
 *
 * \retval ESESTATUS_SUCCESS Always return ESESTATUS_SUCCESS (0).
 *
 */
ESESTATUS phNxpEse_GetEseStatus(phNxpEse_data* timer_buffer);
/** @} */
#endif /* _PHNXPSPILIB_API_H_ */
