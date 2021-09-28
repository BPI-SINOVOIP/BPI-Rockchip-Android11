/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.seccomp.cts.app;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.AssetManager;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CpuFeatures;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.InputStream;
import java.util.Iterator;

/**
 * Device-side tests for CtsSeccompHostTestCases
 */
@RunWith(AndroidJUnit4.class)
public class SeccompDeviceTest {
    static {
        System.loadLibrary("ctsseccomp_jni");
    }

    static final String TAG = "SeccompDeviceTest";

    protected Context mContext;

    // This is flagged whenever we have obtained the test result from the isolated
    // service; it allows us to block the test until the results are in.
    private final ConditionVariable mResultCondition = new ConditionVariable();

    private boolean mAppZygoteResult;
    private Messenger mMessenger;
    private HandlerThread mHandlerThread;

    // The service start can take a long time, because seccomp denials will
    // cause process crashes and dumps, which we waitpid() for sequentially.
    private static final int SERVICE_START_TIMEOUT_MS = 120000;

    private JSONObject mAllowedSyscallMap;
    private JSONObject mBlockedSyscallMap;

    @Before
    public void initializeSyscallMap() throws IOException, JSONException {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        AssetManager manager = mContext.getAssets();
        try (InputStream is = manager.open("syscalls_allowed.json")) {
            mAllowedSyscallMap = new JSONObject(readInputStreamFully(is));
        }
        try (InputStream is = manager.open("syscalls_blocked.json")) {
            mBlockedSyscallMap = new JSONObject(readInputStreamFully(is));
        }
        mHandlerThread = new HandlerThread("HandlerThread");
        mHandlerThread.start();
        Looper looper = mHandlerThread.getLooper();
        mMessenger = new Messenger(new IncomingHandler(looper));
    }

    @Test
    public void testCTSSyscallAllowed() throws JSONException {
        JSONObject map = mAllowedSyscallMap.getJSONObject(getCurrentArch());
        Iterator<String> iter = map.keys();
        while (iter.hasNext()) {
            String syscallName = iter.next();
            testAllowed(map.getInt(syscallName));
        }
    }

    @Test
    public void testCTSSyscallBlocked() throws JSONException {
        JSONObject map = mBlockedSyscallMap.getJSONObject(getCurrentArch());
        Iterator<String> iter = map.keys();
        while (iter.hasNext()) {
            String syscallName = iter.next();
            testBlocked(map.getInt(syscallName));
        }
    }

    class IncomingHandler extends Handler {
        IncomingHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case IsolatedService.MSG_SECCOMP_RESULT:
                    mAppZygoteResult = (msg.arg1 == 1);
                    mResultCondition.open();
                default:
                    super.handleMessage(msg);
            }
        }
	}

    class IsolatedConnection implements ServiceConnection {
        private final ConditionVariable mConnectedCondition = new ConditionVariable();
        private Messenger mService;

        public void onServiceConnected(ComponentName name, IBinder binder) {
            mService = new Messenger(binder);
            mConnectedCondition.open();
        }

        public void onServiceDisconnected(ComponentName name) {
            mConnectedCondition.close();
        }

        public boolean getTestResult() {
            boolean connected = mConnectedCondition.block(SERVICE_START_TIMEOUT_MS);
            if (!connected) {
               Log.e(TAG, "Failed to wait for IsolatedService to bind.");
               return false;
            }
            Message msg = Message.obtain(null, IsolatedService.MSG_GET_SECCOMP_RESULT);
            msg.replyTo = mMessenger;
            try {
                mService.send(msg);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to send message to IsolatedService to get test result.", e);
                return false;
            }

            // Wait for result and return it
            mResultCondition.block();

            return mAppZygoteResult;
        }
    }

    private IsolatedConnection bindService(Class<?> cls) {
        Intent intent = new Intent();
        intent.setClass(mContext, cls);
        IsolatedConnection conn = new IsolatedConnection();
        mContext.bindService(intent, conn, Context.BIND_AUTO_CREATE);

        return conn;
    }

    @Test
    public void testAppZygoteSyscalls() {
        // Isolated services that spawn from the application Zygote are allowed
        // to preload code in a security context that is allowed to use
        // setresuid() / setresgid() in a limited range; this test enforces
        // we allow calls within the range, and reject those outside them.
        // This is done from the ZygotePreload class (which runs in the app zygote
        // context); here we just wait for the service to come up, and ask it
        // whether the tests were executed successfully. We have to ask the service
        // because that is the only process that we can talk to that shares the
        // same address space as the ZygotePreload class, which holds the test
        // result.
        IsolatedConnection conn = bindService(IsolatedService.class);
        boolean testResult = conn.getTestResult();
        Assert.assertTrue("seccomp tests in application zygote failed; see logs.", testResult);
    }

    private static String getCurrentArch() {
        if (CpuFeatures.isArm64Cpu()) {
            return "arm64";
        } else if (CpuFeatures.isArmCpu()) {
            return "arm";
        } else if (CpuFeatures.isX86_64Cpu()) {
            return "x86_64";
        } else if (CpuFeatures.isX86Cpu()) {
            return "x86";
        } else if (CpuFeatures.isMips64Cpu()) {
            return "mips64";
        } else if (CpuFeatures.isMipsCpu()) {
            return "mips";
        } else {
            Assert.fail("Unsupported OS");
            return null;
        }
    }

    private String readInputStreamFully(InputStream is) throws IOException {
        StringBuilder sb = new StringBuilder();
        byte[] buffer = new byte[4096];
        while (is.available() > 0) {
            int size = is.read(buffer);
            if (size < 0) {
                break;
            }
            sb.append(new String(buffer, 0, size));
        }
        return sb.toString();
    }

    private void testBlocked(int nr) {
        Assert.assertTrue("Syscall " + nr + " not blocked", testSyscallBlocked(nr));
    }

    private void testAllowed(int nr) {
        Assert.assertFalse("Syscall " + nr + " blocked", testSyscallBlocked(nr));
    }

    private static final native boolean testSyscallBlocked(int nr);
    protected static final native boolean testSetresuidBlocked(int ruid, int euid, int suid);
    protected static final native boolean testSetresgidBlocked(int rgid, int egid, int sgid);
}
