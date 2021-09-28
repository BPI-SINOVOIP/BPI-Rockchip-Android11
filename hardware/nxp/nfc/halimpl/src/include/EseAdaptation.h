/******************************************************************************
 *
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
#pragma once
#include <pthread.h>

#include "ese_hal_api.h"
#include "hal_nxpese.h"
#include <utils/RefBase.h>
#include <android/hardware/secure_element/1.0/ISecureElement.h>
#include <android/hardware/secure_element/1.0/ISecureElementHalCallback.h>
#include <android/hardware/secure_element/1.0/types.h>
#include <vendor/nxp/nxpese/1.0/INxpEse.h>
using vendor::nxp::nxpese::V1_0::INxpEse;

class ThreadMutex {
 public:
  ThreadMutex();
  virtual ~ThreadMutex();
  void lock();
  void unlock();
  operator pthread_mutex_t*() { return &mMutex; }

 private:
  pthread_mutex_t mMutex;
};

class ThreadCondVar : public ThreadMutex {
 public:
  ThreadCondVar();
  virtual ~ThreadCondVar();
  void signal();
  void wait();
  operator pthread_cond_t*() { return &mCondVar; }
  operator pthread_mutex_t*() {
    return ThreadMutex::operator pthread_mutex_t*();
  }

 private:
  pthread_cond_t mCondVar;
};

class AutoThreadMutex {
 public:
  AutoThreadMutex(ThreadMutex& m);
  virtual ~AutoThreadMutex();
  operator ThreadMutex&() { return mm; }
  operator pthread_mutex_t*() { return (pthread_mutex_t*)mm; }

 private:
  ThreadMutex& mm;
};

class EseAdaptation {
 public:
  void Initialize();
  void InitializeHalDeviceContext();
  virtual ~EseAdaptation();
  static EseAdaptation& GetInstance();
  static int HalIoctl(long arg, void* p_data);
  tHAL_ESE_ENTRY* GetHalEntryFuncs();
  ese_nxp_IoctlInOutData_t* mCurrentIoctlData;
  tHAL_ESE_ENTRY mSpiHalEntryFuncs;  // function pointers for HAL entry points

 private:
  EseAdaptation();
  void signal();
  static EseAdaptation* mpInstance;
  static ThreadMutex sLock;
  static ThreadMutex sIoctlLock;
  ThreadCondVar mCondVar;
  static tHAL_ESE_CBACK* mHalCallback;
  static tHAL_ESE_DATA_CBACK* mHalDataCallback;
  static ThreadCondVar mHalOpenCompletedEvent;
  static ThreadCondVar mHalCloseCompletedEvent;
  static ThreadCondVar mHalIoctlEvent;
  static android::sp<android::hardware::secure_element::V1_0::ISecureElement>
      mHal;
  static android::sp<vendor::nxp::nxpese::V1_0::INxpEse> mHalNxpEse;
#if (NXP_EXTNS == TRUE)
  pthread_t mThreadId;
  static ThreadCondVar mHalCoreResetCompletedEvent;
  static ThreadCondVar mHalCoreInitCompletedEvent;
  static ThreadCondVar mHalInitCompletedEvent;
#endif
  static uint32_t Thread(uint32_t arg);
  static void HalDeviceContextDataCallback(uint16_t data_len, uint8_t* p_data);

  static void HalOpen(tHAL_ESE_CBACK* p_hal_cback,
                      tHAL_ESE_DATA_CBACK* p_data_cback);
  static void HalClose();
  static void HalWrite(uint16_t data_len, uint8_t* p_data);
  static void HalRead(uint16_t data_len, uint8_t* p_data);
};
tHAL_ESE_ENTRY* getInstance();
