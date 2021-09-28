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

/*
 * ESE Status Values - Function Return Codes
 */

#ifndef PHESESTATUS_H
#define PHESESTATUS_H

/* Internally required by PHESESTVAL. */
#define PHESESTSHL8 (8U)
/* Required by PHESESTVAL. */
#define PHESESTBLOWER ((ESESTATUS)(0x00FFU))

/*
 *  ESE Status Composition Macro
 *
 *  This is the macro which must be used to compose status values.
 *
 *  phEseCompID Component ID, as defined in phEseCompId.h .
 *  phEseStatus Status values, as defined in phEseStatus.h .
 *
 *  The macro is not required for the ESESTATUS_SUCCESS value.
 *  This is the only return value to be used directly.
 *  For all other values it shall be used in assignment and conditional
 *statements, e.g.:
 *     ESESTATUS status = PHESESTVAL(phEseCompID, phEseStatus); ...
 *     if (status == PHESESTVAL(phEseCompID, phEseStatus)) ...
 */
#define PHESESTVAL(phEseCompID, phEseStatus)               \
  (((phEseStatus) == (ESESTATUS_SUCCESS))                  \
       ? (ESESTATUS_SUCCESS)                               \
       : ((((ESESTATUS)(phEseStatus)) & (PHESESTBLOWER)) | \
          (((uint16_t)(phEseCompID)) << (PHESESTSHL8))))

/*
 * PHESESTATUS
 * Get grp_retval from Status Code
 */
#define PHESESTATUS(phEseStatus) ((phEseStatus)&0x00FFU)

typedef enum {
  ESESTATUS_SUCCESS = (0x0000),

  ESESTATUS_FAILED = (0x0001),

  ESESTATUS_IOCTL_FAILED = -1,

  ESESTATUS_INVALID_BUFFER = (0x0002),

  ESESTATUS_BUFFER_TOO_SMALL = (0x0003),

  ESESTATUS_INVALID_CLA = (0x0004),

  ESESTATUS_INVALID_CPDU_TYPE = (0x0005),

  ESESTATUS_INVALID_LE_TYPE = (0x0007),

  ESESTATUS_INVALID_DEVICE = (0x0006),

  ESESTATUS_MORE_FRAME = (0x0008),

  ESESTATUS_LAST_FRAME = (0x0009),

  ESESTATUS_CRC_ERROR = (0x000A),

  ESESTATUS_SOF_ERROR = (0x000B),

  ESESTATUS_INSUFFICIENT_RESOURCES = (0x000C),

  ESESTATUS_PENDING = (0x000D),

  ESESTATUS_BOARD_COMMUNICATION_ERROR = (0x000F),

  ESESTATUS_INVALID_STATE = (0x0011),

  ESESTATUS_NOT_INITIALISED = (0x0031),

  ESESTATUS_ALREADY_INITIALISED = (0x0032),

  ESESTATUS_FEATURE_NOT_SUPPORTED = (0x0033),

  ESESTATUS_PARITY_ERROR = (0x0034),

  ESESTATUS_ALREADY_REGISTERED = (0x0035),

  ESESTATUS_CHAINED_FRAME = (0x0036),

  ESESTATUS_SINGLE_FRAME = (0x0037),

  ESESTATUS_DESELECTED = (0x0038),

  ESESTATUS_RELEASED = (0x0039),

  ESESTATUS_NOT_ALLOWED = (0x003A),

  ESESTATUS_OTHER_ERROR = (0x003C),

  ESESTATUS_DWNLD_BUSY = (0x006E),

  ESESTATUS_BUSY = (0x006F),

  ESESTATUS_INVALID_REMOTE_DEVICE = (0x001D),

  ESESTATUS_READ_FAILED = (0x0014),

  ESESTATUS_WRITE_FAILED = (0x0015),

  ESESTATUS_NO_NDEF_SUPPORT = (0x0016),

  ESESTATUS_RESET_SEQ_COUNTER_FRAME_RESEND = (0x001A),

  ESESTATUS_INVALID_RECEIVE_LENGTH = (0x001B),

  ESESTATUS_INVALID_FORMAT = (0x001C),

  ESESTATUS_INSUFFICIENT_STORAGE = (0x001F),

  ESESTATUS_FRAME_RESEND = (0x0023),

  ESESTATUS_WRITE_TIMEOUT = (0x0024),

  ESESTATUS_RESPONSE_TIMEOUT = (0x0025),

  ESESTATUS_FRAME_RESEND_R_FRAME = (0x0026),

  ESESTATUS_SEND_NEXT_FRAME = (0x0027),

  ESESTATUS_REVOCERY_STARTED = (0x0028),

  ESESTATUS_SEND_R_FRAME = (0x0029),

  ESESTATUS_FRAME_RESEND_RNAK = (0x0030),

  ESESTATUS_FRAME_SEND_R_FRAME = (0x003B),

  ESESTATUS_UNKNOWN_ERROR = (0x00FE),

  ESESTATUS_INVALID_PARAMETER = (0x00FF),

  ESESTATUS_CMD_ABORTED = (0x0002),

  ESESTATUS_NO_TARGET_FOUND = (0x000A),

  ESESTATUS_NO_DEVICE_CONNECTED = (0x000B),

  ESESTATUS_RESYNCH_REQ = (0x000E),

  ESESTATUS_RESYNCH_RES = (0x0010),

  ESESTATUS_IFS_REQ = (0x001E),

  ESESTATUS_IFS_RES = (0x0017),

  ESESTATUS_ABORT_REQ = (0x00F0),

  ESESTATUS_ABORT_RES = (0x00F2),

  ESESTATUS_WTX_REQ = (0x00F5),

  ESESTATUS_WTX_RES = (0x00F6),

  ESESTATUS_RESET_REQ = (0x00F7),

  ESESTATUS_RESET_RES = (0x00F8),

  ESESTATUS_END_APDU_REQ = (0x00F9),

  ESESTATUS_END_APDU_RES = (0x00FA),

  ESESTATUS_SHUTDOWN = (0x0091),

  ESESTATUS_TARGET_LOST = (0x0092),

  ESESTATUS_REJECTED = (0x0093),

  ESESTATUS_TARGET_NOT_CONNECTED = (0x0094),

  ESESTATUS_INVALID_HANDLE = (0x0095),

  ESESTATUS_ABORTED = (0x0096),

  ESESTATUS_COMMAND_NOT_SUPPORTED = (0x0097),

  ESESTATUS_NON_NDEF_COMPLIANT = (0x0098),

  ESESTATUS_NOT_ENOUGH_MEMORY = (0x001F),

  ESESTATUS_INCOMING_CONNECTION = (0x0045),

  ESESTATUS_CONNECTION_SUCCESS = (0x0046),

  ESESTATUS_CONNECTION_FAILED = (0x0047),
} ESESTATUS;
#endif /* PHESESTATUS_H */
