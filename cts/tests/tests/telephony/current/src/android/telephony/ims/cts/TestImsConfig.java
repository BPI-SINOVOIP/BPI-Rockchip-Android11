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

package android.telephony.ims.cts;

import android.telephony.ims.stub.ImsConfigImplBase;

import java.util.HashMap;

public class TestImsConfig extends ImsConfigImplBase {

    private HashMap<Integer, Integer> mIntHashMap = new HashMap<>();
    private HashMap<Integer, String> mStringHashMap = new HashMap<>();

    @Override
    public int setConfig(int item, int value) {
        mIntHashMap.put(item, value);
        return ImsConfigImplBase.CONFIG_RESULT_SUCCESS;
    }

    @Override
    public int setConfig(int item, String value) {
        mStringHashMap.put(item, value);
        return ImsConfigImplBase.CONFIG_RESULT_SUCCESS;
    }

    @Override
    public int getConfigInt(int item) {
        Integer result = mIntHashMap.get(item);
        return result != null ? result : ImsConfigImplBase.CONFIG_RESULT_UNKNOWN;
    }

    @Override
    public String getConfigString(int item) {
        return mStringHashMap.get(item);
    }

    @Override
    public void notifyRcsAutoConfigurationReceived(byte[] content, boolean isCompressed) {
        int item = isCompressed ? ImsUtils.ITEM_COMPRESSED : ImsUtils.ITEM_NON_COMPRESSED;
        setConfig(item, new String(content));
    }
}
