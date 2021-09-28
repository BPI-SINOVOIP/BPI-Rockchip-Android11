/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <radio_config_hidl_hal_utils.h>

#define ASSERT_OK(ret) ASSERT_TRUE(ret.isOk())

/*
 * Test IRadioConfig.getSimSlotsStatus()
 */
TEST_P(RadioConfigHidlTest, getSimSlotsStatus) {
    const int serial = GetRandomSerialNumber();
    Return<void> res = radioConfig->getSimSlotsStatus(serial);
    ASSERT_OK(res);
    ALOGI("getIccSlotsStatus, rspInfo.error = %s\n",
          toString(radioConfigRsp->rspInfo.error).c_str());
}
