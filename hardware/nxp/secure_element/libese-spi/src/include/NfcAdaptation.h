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
#pragma once
#include <pthread.h>

#include <android/hardware/nfc/1.0/types.h>
#include <phEseStatus.h>
#include <utils/RefBase.h>
#include <vendor/nxp/nxpnfc/1.0/INxpNfc.h>
#include "hal_nxpese.h"
using vendor::nxp::nxpnfc::V1_0::INxpNfc;

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

class NfcAdaptation {
 public:
  virtual ~NfcAdaptation();
  void Initialize();
  static NfcAdaptation& GetInstance();
  static ESESTATUS HalIoctl(long data_len, void* p_data);
  ese_nxp_IoctlInOutData_t* mCurrentIoctlData;

 private:
  NfcAdaptation();
  static NfcAdaptation* mpInstance;
  static ThreadMutex sLock;
  static ThreadMutex sIoctlLock;
  ThreadCondVar mCondVar;
  static ThreadCondVar mHalIoctlEvent;
  static android::sp<INxpNfc> mHalNxpNfc;
};
