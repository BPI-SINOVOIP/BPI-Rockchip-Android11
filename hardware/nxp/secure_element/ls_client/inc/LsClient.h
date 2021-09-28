/*******************************************************************************
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

#ifndef LSCLIENT_H_
#define LSCLIENT_H_

#include <string>

typedef enum {
  LSCSTATUS_SUCCESS = (0x0000),
  LSCSTATUS_FAILED = (0x0003),
  LSCSTATUS_SELF_UPDATE_DONE = (0x0005),
  LSCSTATUS_HASH_SLOT_EMPTY = (0x0006),
  LSCSTATUS_HASH_SLOT_INVALID = (0x0007)
} LSCSTATUS;

/*******************************************************************************
**
** Function:        LSC_onCompletedCallback
**
** Description:     callback function when Loader Service Scripts thread is done
**
*******************************************************************************/
typedef void (*LSC_onCompletedCallback)(bool result, std::string reason,
                                        void* args);

/*******************************************************************************
 **
 ** Function:        LSC_doDownload
 **
 ** Description:     Start LS download process
 **
 ** Returns:         SUCCESS if ok
 **
 *******************************************************************************/
LSCSTATUS LSC_doDownload(LSC_onCompletedCallback callback, void* arg);

#endif /* LSCLIENT_H_ */
