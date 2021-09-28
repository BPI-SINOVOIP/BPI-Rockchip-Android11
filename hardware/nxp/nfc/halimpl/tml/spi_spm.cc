/*
 * Copyright (C) 2012-2014 NXP Semiconductors
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <log/log.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "spi_spm.h"
#include "phNxpLog.h"
#include "phTmlNfc_i2c.h"

/*******************************************************************************
**
** Function         phPalEse_spi_ioctl
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
int phPalEse_spi_ioctl(phPalEse_ControlCode_t eControlCode,void *pDevHandle, long level)
{
    int ret;
    NXPLOG_TML_D("phPalEse_spi_ioctl(), ioctl %x , level %lx", eControlCode, level);

  if (NULL == pDevHandle) {
    return -1;
  }
  switch (eControlCode) {
    case phPalEse_e_ChipRst:
        if(level == 1 || level == 0)
        ret = ioctl((intptr_t)pDevHandle, P61_SET_SPI_PWR, level);
        else
        ret=0;
        break;

    case phPalEse_e_GetSPMStatus:
        ret = ioctl((intptr_t)pDevHandle, P61_GET_PWR_STATUS, level);
        break;

    case phPalEse_e_SetPowerScheme:
         ret=0;
        break;
   case phPalEse_e_GetEseAccess:
         ret=0;
        break;
#if(NXP_ESE_JCOP_DWNLD_PROTECTION == TRUE)
    case phPalEse_e_SetJcopDwnldState:
        ret=0;
        break;
#endif
    case phPalEse_e_DisablePwrCntrl:
        ret = ioctl((intptr_t)pDevHandle, P61_SET_SPI_PWR, 1);
        break;
    default:
        ret=-1;
        break;
    }
    return ret;
}
