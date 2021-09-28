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

package android.appsecurity.cts;

import static org.junit.Assert.fail;

import com.android.server.wm.KeyguardServiceDelegateProto;
import com.android.server.wm.WindowManagerServiceDumpProto;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;

import com.google.protobuf.MessageLite;
import com.google.protobuf.Parser;

class LockScreenInspector {
    private static final String DUMPSYS_WINDOW = "dumpsys window --proto";

    private Boolean mKeyguardShowingAndNotOccluded;
    private final ITestDevice mDevice;

    private LockScreenInspector(ITestDevice device) {
        mDevice = device;
    }

    static LockScreenInspector newInstance(ITestDevice device) {
        LockScreenInspector inspector = new LockScreenInspector(device);
        inspector.checkCurrentState();
        return inspector;
    }

    boolean isDisplayedAndNotOccluded() {
        return mKeyguardShowingAndNotOccluded;
    }

    private void checkCurrentState() {
        int retriesLeft = 3;
        boolean retry = false;

        do {
            if (retry) {
                CLog.w("***Retrying dump****");
                try {
                    Thread.sleep(500);
                } catch (InterruptedException ignored) {
                }
            }
            try {
                WindowManagerServiceDumpProto dumpProto = getDump(
                        WindowManagerServiceDumpProto.parser(), DUMPSYS_WINDOW);
                KeyguardServiceDelegateProto keyguardProto =
                        dumpProto.getPolicy().getKeyguardDelegate();
                mKeyguardShowingAndNotOccluded = keyguardProto.getShowing()
                        && !keyguardProto.getOccluded();
                retry = false;
            } catch (Exception e) {
                CLog.w(e);
                retry = true;
            }
        } while (retry && retriesLeft-- > 0);

        if (retry) {
            fail("Could not obtain lockscreen state");
        }
    }

    private <T extends MessageLite> T getDump(Parser<T> parser, String command) throws Exception {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        mDevice.executeShellCommand(command, receiver);
        return parser.parseFrom(receiver.getOutput());
    }
}
