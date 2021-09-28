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

#define LOG_TAG "SpiAddaptation"
#include "NfcAdaptation.h"
#include <android/hardware/nfc/1.0/types.h>
#include <hwbinder/ProcessState.h>
#include <log/log.h>
#include <pthread.h>

using android::sp;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::hidl_vec;
using vendor::nxp::nxpnfc::V1_0::INxpNfc;

sp<INxpNfc> NfcAdaptation::mHalNxpNfc = nullptr;
ThreadMutex NfcAdaptation::sIoctlLock;
NfcAdaptation* NfcAdaptation::mpInstance = NULL;
ThreadMutex NfcAdaptation::sLock;

int omapi_status;
extern bool ese_debug_enabled;
void NfcAdaptation::Initialize() {
  const char* func = "NfcAdaptation::Initialize";
  ALOGD_IF(ese_debug_enabled, "%s", func);
  if (mHalNxpNfc != nullptr) return;
  mHalNxpNfc = INxpNfc::tryGetService();
  LOG_FATAL_IF(mHalNxpNfc == nullptr, "Failed to retrieve the NXP NFC HAL!");
  if (mHalNxpNfc != nullptr) {
    ALOGD_IF(ese_debug_enabled, "%s: INxpNfc::getService() returned %p (%s)",
             func, mHalNxpNfc.get(),
             (mHalNxpNfc->isRemote() ? "remote" : "local"));
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit", func);
}
/*******************************************************************************
**
** Function:    NfcAdaptation::GetInstance()
**
** Description: access class singleton
**
** Returns:     pointer to the singleton object
**
*******************************************************************************/
NfcAdaptation& NfcAdaptation::GetInstance() {
  AutoThreadMutex a(sLock);

  if (!mpInstance) mpInstance = new NfcAdaptation;
  return *mpInstance;
}
/*******************************************************************************
**
** Function:    ThreadMutex::ThreadMutex()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::ThreadMutex() {
  pthread_mutexattr_t mutexAttr;

  pthread_mutexattr_init(&mutexAttr);
  pthread_mutex_init(&mMutex, &mutexAttr);
  pthread_mutexattr_destroy(&mutexAttr);
}
/*******************************************************************************
**
** Function:    ThreadMutex::~ThreadMutex()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::~ThreadMutex() { pthread_mutex_destroy(&mMutex); }

/*******************************************************************************
**
** Function:    AutoThreadMutex::AutoThreadMutex()
**
** Description: class constructor, automatically lock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::AutoThreadMutex(ThreadMutex& m) : mm(m) { mm.lock(); }

/*******************************************************************************
**
** Function:    AutoThreadMutex::~AutoThreadMutex()
**
** Description: class destructor, automatically unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::~AutoThreadMutex() { mm.unlock(); }

/*******************************************************************************
**
** Function:    ThreadMutex::lock()
**
** Description: lock kthe mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::lock() { pthread_mutex_lock(&mMutex); }

/*******************************************************************************
**
** Function:    ThreadMutex::unblock()
**
** Description: unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::unlock() { pthread_mutex_unlock(&mMutex); }

/*******************************************************************************
**
** Function:    NfcAdaptation::NfcAdaptation()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::NfcAdaptation() { mCurrentIoctlData = NULL; }

/*******************************************************************************
**
** Function:    NfcAdaptation::~NfcAdaptation()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::~NfcAdaptation() { mpInstance = NULL; }

/*******************************************************************************
**
** Function:    IoctlCallback
**
** Description: Callback from HAL stub for IOCTL api invoked.
**              Output data for IOCTL is sent as argument
**
** Returns:     None.
**
*******************************************************************************/
void IoctlCallback(::android::hardware::nfc::V1_0::NfcData outputData) {
  const char* func = "IoctlCallback";
  ese_nxp_ExtnOutputData_t* pOutData =
      (ese_nxp_ExtnOutputData_t*)&outputData[0];
  ALOGD_IF(ese_debug_enabled, "%s Ioctl Type=%lu", func,
           (unsigned long)pOutData->ioctlType);
  NfcAdaptation* pAdaptation = (NfcAdaptation*)pOutData->context;
  /*Output Data from stub->Proxy is copied back to output data
   * This data will be sent back to libnfc*/
  memcpy(&pAdaptation->mCurrentIoctlData->out, &outputData[0],
         sizeof(ese_nxp_ExtnOutputData_t));
  ALOGD_IF(ese_debug_enabled, "%s Ioctl Type value[0]:0x%x and value[3] 0x%x",
           func, pOutData->data.nxpRsp.p_rsp[0],
           pOutData->data.nxpRsp.p_rsp[3]);
  omapi_status = pOutData->data.nxpRsp.p_rsp[3];
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalIoctl
**
** Description: Calls ioctl to the Nfc driver.
**              If called with a arg value of 0x01 than wired access requested,
**              status of the requst would be updated to p_data.
**              If called with a arg value of 0x00 than wired access will be
**              released, status of the requst would be updated to p_data.
**              If called with a arg value of 0x02 than current p61 state would
*be
**              updated to p_data.
**
** Returns:     -1 or 0.
**
*******************************************************************************/
ESESTATUS NfcAdaptation::HalIoctl(long arg, void* p_data) {
  const char* func = "NfcAdaptation::HalIoctl";
  ::android::hardware::nfc::V1_0::NfcData data;
  ESESTATUS result = ESESTATUS_FAILED;
  AutoThreadMutex a(sIoctlLock);
  ese_nxp_IoctlInOutData_t* pInpOutData = (ese_nxp_IoctlInOutData_t*)p_data;
  ALOGD_IF(ese_debug_enabled, "%s arg=%ld", func, arg);
  pInpOutData->inp.context = &NfcAdaptation::GetInstance();
  NfcAdaptation::GetInstance().mCurrentIoctlData = pInpOutData;
  data.setToExternal((uint8_t*)pInpOutData, sizeof(ese_nxp_IoctlInOutData_t));
  if (mHalNxpNfc != nullptr) {
    mHalNxpNfc->ioctl(arg, data, IoctlCallback);
  }
  ALOGD_IF(ese_debug_enabled, "%s Ioctl Completed for Type=%lu", func,
           (unsigned long)pInpOutData->out.ioctlType);
  result = (ESESTATUS)(pInpOutData->out.result);
  return result;
}

/*******************************************************************************
**
** Function:    ThreadCondVar::ThreadCondVar()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::ThreadCondVar() {
  pthread_condattr_t CondAttr;

  pthread_condattr_init(&CondAttr);
  pthread_cond_init(&mCondVar, &CondAttr);

  pthread_condattr_destroy(&CondAttr);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::~ThreadCondVar()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::~ThreadCondVar() { pthread_cond_destroy(&mCondVar); }
