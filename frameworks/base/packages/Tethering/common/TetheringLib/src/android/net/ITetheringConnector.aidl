/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.net;

import android.net.IIntResultListener;
import android.net.ITetheringEventCallback;
import android.net.TetheringRequestParcel;
import android.os.ResultReceiver;

/** @hide */
oneway interface ITetheringConnector {
    void tether(String iface, String callerPkg, IIntResultListener receiver);

    void untether(String iface, String callerPkg, IIntResultListener receiver);

    void setUsbTethering(boolean enable, String callerPkg, IIntResultListener receiver);

    void startTethering(in TetheringRequestParcel request, String callerPkg,
            IIntResultListener receiver);

    void stopTethering(int type, String callerPkg, IIntResultListener receiver);

    void requestLatestTetheringEntitlementResult(int type, in ResultReceiver receiver,
            boolean showEntitlementUi, String callerPkg);

    void registerTetheringEventCallback(ITetheringEventCallback callback, String callerPkg);

    void unregisterTetheringEventCallback(ITetheringEventCallback callback, String callerPkg);

    void isTetheringSupported(String callerPkg, IIntResultListener receiver);

    void stopAllTethering(String callerPkg, IIntResultListener receiver);
}
