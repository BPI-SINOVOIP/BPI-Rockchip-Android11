/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.app.cts;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.stubs.ActivityTestsBase;
import android.app.stubs.IsolatedService;
import android.app.stubs.LaunchpadActivity;
import android.app.stubs.LocalDeniedService;
import android.app.stubs.LocalForegroundService;
import android.app.stubs.LocalGrantedService;
import android.app.stubs.LocalService;
import android.app.stubs.LocalStoppedService;
import android.app.stubs.NullService;
import android.app.stubs.R;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.UserHandle;
import android.service.notification.StatusBarNotification;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;
import android.util.SparseArray;

import androidx.test.filters.FlakyTest;
import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.IBinderParcelable;
import com.android.compatibility.common.util.SystemUtil;
import com.android.server.am.nano.ActivityManagerServiceDumpProcessesProto;
import com.android.server.am.nano.ProcessRecordProto;

import com.google.protobuf.nano.InvalidProtocolBufferNanoException;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

public class ServiceTest extends ActivityTestsBase {
    private static final String TAG = "ServiceTest";
    private static final String NOTIFICATION_CHANNEL_ID = TAG;
    private static final int STATE_START_1 = 0;
    private static final int STATE_START_2 = 1;
    private static final int STATE_START_3 = 2;
    private static final int STATE_UNBIND = 3;
    private static final int STATE_DESTROY = 4;
    private static final int STATE_REBIND = 5;
    private static final int STATE_UNBIND_ONLY = 6;
    private static final int STATE_STOP_SELF_SUCCESS_UNBIND = 6;
    private static final int DELAY = 5000;
    private static final
        String EXIST_CONN_TO_RECEIVE_SERVICE = "existing connection to receive service";
    private static final String EXIST_CONN_TO_LOSE_SERVICE = "existing connection to lose service";
    private static final String EXTERNAL_SERVICE_PACKAGE = "com.android.app2";
    private static final String EXTERNAL_SERVICE_COMPONENT =
            EXTERNAL_SERVICE_PACKAGE + "/android.app.stubs.LocalService";
    private static final String APP_ZYGOTE_PROCESS_NAME = "android.app.stubs_zygote";
    private int mExpectedServiceState;
    private Context mContext;
    private Intent mLocalService;
    private Intent mLocalDeniedService;
    private Intent mLocalForegroundService;
    private Intent mLocalGrantedService;
    private Intent mLocalService_ApplicationHasPermission;
    private Intent mLocalService_ApplicationDoesNotHavePermission;
    private Intent mIsolatedService;
    private Intent mExternalService;
    private Executor mContextMainExecutor;
    private HandlerThread mBackgroundThread;
    private Executor mBackgroundThreadExecutor;

    private IBinder mStateReceiver;

    private static class EmptyConnection implements ServiceConnection {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
        }
    }

    private static class NullServiceConnection implements ServiceConnection {
        boolean mNullBinding = false;

        @Override public void onServiceConnected(ComponentName name, IBinder service) {}
        @Override public void onServiceDisconnected(ComponentName name) {}

        @Override
        public void onNullBinding(ComponentName name) {
            synchronized (this) {
                mNullBinding = true;
                this.notifyAll();
            }
        }

        public void waitForNullBinding(final long timeout) {
            long now = SystemClock.uptimeMillis();
            final long end = now + timeout;
            synchronized (this) {
                while (!mNullBinding && (now < end)) {
                    try {
                        this.wait(end - now);
                    } catch (InterruptedException e) {
                    }
                    now = SystemClock.uptimeMillis();
                }
            }
        }

        public boolean nullBindingReceived() {
            synchronized (this) {
                return mNullBinding;
            }
        }
    }

    private class TestConnection implements ServiceConnection {
        private final boolean mExpectDisconnect;
        private final boolean mSetReporter;
        private boolean mMonitor;
        private int mCount;
        private Thread mOnServiceConnectedThread;

        public TestConnection(boolean expectDisconnect, boolean setReporter) {
            mExpectDisconnect = expectDisconnect;
            mSetReporter = setReporter;
            mMonitor = !setReporter;
        }

        void setMonitor(boolean v) {
            mMonitor = v;
        }

        public Thread getOnServiceConnectedThread() {
            return mOnServiceConnectedThread;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mOnServiceConnectedThread = Thread.currentThread();
            if (mSetReporter) {
                Parcel data = Parcel.obtain();
                data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
                data.writeStrongBinder(mStateReceiver);
                try {
                    service.transact(LocalService.SET_REPORTER_CODE, data, null, 0);
                } catch (RemoteException e) {
                    finishBad("DeadObjectException when sending reporting object");
                }
                data.recycle();
            }

            if (mMonitor) {
                mCount++;
                if (mExpectedServiceState == STATE_START_1) {
                    if (mCount == 1) {
                        finishGood();
                    } else {
                        finishBad("onServiceConnected() again on an object when it "
                                + "should have been the first time");
                    }
                } else if (mExpectedServiceState == STATE_START_2) {
                    if (mCount == 2) {
                        finishGood();
                    } else {
                        finishBad("onServiceConnected() the first time on an object "
                                + "when it should have been the second time");
                    }
                } else {
                    finishBad("onServiceConnected() called unexpectedly");
                }
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            if (mMonitor) {
                if (mExpectedServiceState == STATE_DESTROY) {
                    if (mExpectDisconnect) {
                        finishGood();
                    } else {
                        finishBad("onServiceDisconnected() when it shouldn't have been");
                    }
                } else {
                    finishBad("onServiceDisconnected() called unexpectedly");
                }
            }
        }
    }

    private class TestStopSelfConnection extends TestConnection {
        private IBinder mService;

        public TestStopSelfConnection() {
            super(false /* expectDisconnect */, true /* setReporter */);
        }

        private void executeTransact(int code) {
            Parcel data = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(code, data, null /* reply */, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            data.recycle();
        }

        public void stopSelf() {
            executeTransact(LocalService.STOP_SELF_CODE);
        }

        public void stopSelfResult() {
            executeTransact(LocalService.STOP_SELF_RESULT_CODE);
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mService = service;
            super.onServiceConnected(name, service);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            synchronized (this) {
                mService = null;
            }
        }
    }

    final class IsolatedConnection implements ServiceConnection {
        private IBinder mService;
        private int mUid;
        private int mPid;
        private int mPpid;
        private Thread mOnServiceConnectedThread;

        public IsolatedConnection() {
            mUid = mPid = -1;
        }

        public void waitForService(int timeoutMs) {
            final long endTime = System.currentTimeMillis() + timeoutMs;

            boolean timeout = false;
            synchronized (this) {
                while (mService == null) {
                    final long delay = endTime - System.currentTimeMillis();
                    if (delay < 0) {
                        timeout = true;
                        break;
                    }

                    try {
                        wait(delay);
                    } catch (final java.lang.InterruptedException e) {
                        // do nothing
                    }
                }
            }

            if (timeout) {
                throw new RuntimeException("Timed out waiting for connection");
            }
        }

        public int getUid() {
            return mUid;
        }

        public int getPid() {
            return mPid;
        }

        public int getPpid() {
            return mPpid;
        }

        public boolean zygotePreloadCalled() {
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(LocalService.GET_ZYGOTE_PRELOAD_CALLED, data, reply, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            boolean value = reply.readBoolean();
            reply.recycle();
            data.recycle();
            return value;
        }

        public void setValue(int value) {
            Parcel data = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            data.writeInt(value);
            try {
                mService.transact(LocalService.SET_VALUE_CODE, data, null, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            data.recycle();
        }

        public int getValue() {
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(LocalService.GET_VALUE_CODE, data, reply, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            int value = reply.readInt();
            reply.recycle();
            data.recycle();
            return value;
        }

        public int getPidIpc() {
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(LocalService.GET_PID_CODE, data, reply, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            int value = reply.readInt();
            reply.recycle();
            data.recycle();
            return value;
        }

        public int getPpidIpc() {
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(LocalService.GET_PPID_CODE, data, reply, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            int value = reply.readInt();
            reply.recycle();
            data.recycle();
            return value;
        }

        public int getUidIpc() {
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            data.writeInterfaceToken(LocalService.SERVICE_LOCAL);
            try {
                mService.transact(LocalService.GET_UID_CODE, data, reply, 0);
            } catch (RemoteException e) {
                finishBad("DeadObjectException when sending reporting object");
            }
            int value = reply.readInt();
            reply.recycle();
            data.recycle();
            return value;
        }

        public Thread getOnServiceConnectedThread() {
            return mOnServiceConnectedThread;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            synchronized (this) {
                mOnServiceConnectedThread = Thread.currentThread();
                mService = service;
                mUid = getUidIpc();
                mPid = getPidIpc();
                mPpid = getPpidIpc();
                notifyAll();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            synchronized (this) {
                mService = null;
            }
        }
    }

    private byte[] executeShellCommand(String cmd) {
        try {
            ParcelFileDescriptor pfd =
                    InstrumentationRegistry.getInstrumentation().getUiAutomation()
                            .executeShellCommand(cmd);
            byte[] buf = new byte[512];
            int bytesRead;
            FileInputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(pfd);
            ByteArrayOutputStream stdout = new ByteArrayOutputStream();
            while ((bytesRead = fis.read(buf)) != -1) {
                stdout.write(buf, 0, bytesRead);
            }
            fis.close();
            return stdout.toByteArray();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public ActivityManagerServiceDumpProcessesProto getActivityManagerProcesses() {
        byte[] dump = executeShellCommand("dumpsys activity --proto processes");
        try {
            return ActivityManagerServiceDumpProcessesProto.parseFrom(dump);
        } catch (InvalidProtocolBufferNanoException e) {
            throw new RuntimeException("Failed parsing proto", e);
        }
    }

    private void startExpectResult(Intent service) {
        startExpectResult(service, new Bundle());
    }

    private void startExpectResult(Intent service, Bundle bundle) {
        bundle.putParcelable(LocalService.REPORT_OBJ_NAME, new IBinderParcelable(mStateReceiver));

        boolean success = false;
        try {
            mExpectedServiceState = STATE_START_1;
            mContext.startService(new Intent(service).putExtras(bundle));
            waitForResultOrThrow(DELAY, "service to start first time");
            mExpectedServiceState = STATE_START_2;
            mContext.startService(new Intent(service).putExtras(bundle));
            waitForResultOrThrow(DELAY, "service to start second time");
            success = true;
        } finally {
            if (!success) {
                mContext.stopService(service);
            }
        }
        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(service);
        waitForResultOrThrow(DELAY, "service to be destroyed");
    }

    private NotificationManager getNotificationManager() {
        NotificationManager notificationManager =
                (NotificationManager) getContext().getSystemService(Context.NOTIFICATION_SERVICE);
        return notificationManager;
    }

    private void sendNotification(int id, String title) {
        Notification notification = new Notification.Builder(getContext(), NOTIFICATION_CHANNEL_ID)
            .setContentTitle(title)
            .setSmallIcon(R.drawable.black)
            .build();
        getNotificationManager().notify(id, notification);
    }

    private void cancelNotification(int id) {
        getNotificationManager().cancel(id);
    }

    private void assertNotification(int id, String expectedTitle) {
        String packageName = getContext().getPackageName();
        String errorMessage = null;
        for (int i = 1; i<=2; i++) {
            errorMessage = null;
            StatusBarNotification[] sbns = getNotificationManager().getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                if (sbn.getId() == id && sbn.getPackageName().equals(packageName)) {
                    String actualTitle =
                            sbn.getNotification().extras.getString(Notification.EXTRA_TITLE);
                    if (expectedTitle.equals(actualTitle)) {
                        return;
                    }
                    // It's possible the notification hasn't been updated yet, so save the error
                    // message to only fail after retrying.
                    errorMessage = String.format("Wrong title for notification #%d: "
                            + "expected '%s', actual '%s'", id, expectedTitle, actualTitle);
                    Log.w(TAG, errorMessage);
                }
            }
            // Notification might not be rendered yet, wait and try again...
            try {
                Thread.sleep(DELAY);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        if (errorMessage != null) {
            fail(errorMessage);
        }
        fail("No notification with id " + id + " for package " + packageName);
    }

    private void assertNoNotification(int id) {
        String packageName = getContext().getPackageName();
        StatusBarNotification found = null;
        for (int i = 1; i<=2; i++) {
            found = null;
            StatusBarNotification[] sbns = getNotificationManager().getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                if (sbn.getId() == id && sbn.getPackageName().equals(packageName)) {
                    found = sbn;
                    break;
                }
            }
            if (found != null) {
                // Notification might not be canceled yet, wait and try again...
                try {
                    Thread.sleep(DELAY);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }
        assertNull("Found notification with id " + id + " for package " + packageName + ": "
                + found, found);
    }

    /**
     * test the service lifecycle, a service can be used in two ways:
     * 1  It can be started and allowed to run until someone stops it or it stops itself.
     *    In this mode, it's started by calling Context.startService()
     *    and stopped by calling Context.stopService().
     *    It can stop itself by calling Service.stopSelf() or Service.stopSelfResult().
     *    Only one stopService() call is needed to stop the service,
     *    no matter how many times startService() was called.
     * 2  It can be operated programmatically using an interface that it defines and exports.
     *    Clients establish a connection to the Service object
     *    and use that connection to call into the service.
     *    The connection is established by calling Context.bindService(),
     *    and is closed by calling Context.unbindService().
     *    Multiple clients can bind to the same service.
     *    If the service has not already been launched, bindService() can optionally launch it.
     */
    private void bindExpectResult(Intent service) {
        TestConnection conn = new TestConnection(true, false);
        TestConnection conn2 = new TestConnection(false, false);
        boolean success = false;
        try {
            // Expect to see the TestConnection connected.
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(service, conn, 0);
            mContext.startService(service);
            waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);

            // Expect to see the second TestConnection connected.
            mContext.bindService(service, conn2, 0);
            waitForResultOrThrow(DELAY, "new connection to receive service");

            mContext.unbindService(conn2);
            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
                mContext.unbindService(conn2);
                mContext.stopService(service);
            }
        }

        // Expect to see the TestConnection disconnected.
        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(service);
        waitForResultOrThrow(DELAY, EXIST_CONN_TO_LOSE_SERVICE);

        mContext.unbindService(conn);

        conn = new TestConnection(true, true);
        success = false;
        try {
            // Expect to see the TestConnection connected.
            conn.setMonitor(true);
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(service, conn, 0);
            mContext.startService(service);
            waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);

            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
                mContext.stopService(service);
            }
        }

        // Expect to see the service unbind and then destroyed.
        conn.setMonitor(false);
        mExpectedServiceState = STATE_UNBIND;
        mContext.stopService(service);
        waitForResultOrThrow(DELAY, EXIST_CONN_TO_LOSE_SERVICE);

        mContext.unbindService(conn);

        conn = new TestConnection(true, true);
        success = false;
        try {
            // Expect to see the TestConnection connected.
            conn.setMonitor(true);
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(service, conn, 0);
            mContext.startService(service);
            waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);

            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
                mContext.stopService(service);
            }
        }

        // Expect to see the service unbind but not destroyed.
        conn.setMonitor(false);
        mExpectedServiceState = STATE_UNBIND_ONLY;
        mContext.unbindService(conn);
        waitForResultOrThrow(DELAY, "existing connection to unbind service");

        // Expect to see the service rebound.
        mExpectedServiceState = STATE_REBIND;
        mContext.bindService(service, conn, 0);
        waitForResultOrThrow(DELAY, "existing connection to rebind service");

        // Expect to see the service unbind and then destroyed.
        mExpectedServiceState = STATE_UNBIND;
        mContext.stopService(service);
        waitForResultOrThrow(DELAY, EXIST_CONN_TO_LOSE_SERVICE);

        mContext.unbindService(conn);
    }

    /**
     * test automatically create the service as long as the binding exists
     * and disconnect from an application service
     */
    private void bindAutoExpectResult(Intent service) {
        TestConnection conn = new TestConnection(false, true);
        boolean success = false;
        try {
            conn.setMonitor(true);
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(
                    service, conn, Context.BIND_AUTO_CREATE);
            waitForResultOrThrow(DELAY, "connection to start and receive service");
            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
            }
        }
        mExpectedServiceState = STATE_UNBIND;
        mContext.unbindService(conn);
        waitForResultOrThrow(DELAY, "disconnecting from service");
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getContext();
        mLocalService = new Intent(mContext, LocalService.class);
        mExternalService = new Intent();
        mExternalService.setComponent(ComponentName.unflattenFromString(EXTERNAL_SERVICE_COMPONENT));
        mLocalForegroundService = new Intent(mContext, LocalForegroundService.class);
        mLocalDeniedService = new Intent(mContext, LocalDeniedService.class);
        mLocalGrantedService = new Intent(mContext, LocalGrantedService.class);
        mLocalService_ApplicationHasPermission = new Intent(
                LocalService.SERVICE_LOCAL_GRANTED, null /*uri*/, mContext, LocalService.class);
        mLocalService_ApplicationDoesNotHavePermission = new Intent(
                LocalService.SERVICE_LOCAL_DENIED, null /*uri*/, mContext, LocalService.class);
        mIsolatedService = new Intent(mContext, IsolatedService.class);
        mStateReceiver = new MockBinder();
        getNotificationManager().createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
        mContextMainExecutor = mContext.getMainExecutor();
    }

    private void setupBackgroundThread() {
        HandlerThread thread  = new HandlerThread("ServiceTestBackgroundThread");
        thread.start();
        Handler handler = new Handler(thread.getLooper());
        mBackgroundThread = thread;
        mBackgroundThreadExecutor = new Executor() {
            @Override
            public void execute(Runnable runnable) {
                handler.post(runnable);
            }
        };
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        getNotificationManager().deleteNotificationChannel(NOTIFICATION_CHANNEL_ID);
        mContext.stopService(mLocalService);
        mContext.stopService(mLocalForegroundService);
        mContext.stopService(mLocalGrantedService);
        mContext.stopService(mLocalService_ApplicationHasPermission);
        mContext.stopService(mExternalService);
        if (mBackgroundThread != null) {
            mBackgroundThread.quitSafely();
        }
        mBackgroundThread = null;
        mBackgroundThreadExecutor = null;
    }

    private class MockBinder extends Binder {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply,
                int flags) throws RemoteException {
            if (code == LocalService.STARTED_CODE) {
                data.enforceInterface(LocalService.SERVICE_LOCAL);
                int count = data.readInt();
                if (mExpectedServiceState == STATE_START_1) {
                    if (count == 1) {
                        finishGood();
                    } else {
                        finishBad("onStart() again on an object when it "
                                + "should have been the first time");
                    }
                } else if (mExpectedServiceState == STATE_START_2) {
                    if (count == 2) {
                        finishGood();
                    } else {
                        finishBad("onStart() the first time on an object when it "
                                + "should have been the second time");
                    }
                } else if (mExpectedServiceState == STATE_START_3) {
                    if (count == 3) {
                        finishGood();
                    } else {
                        finishBad("onStart() the first time on an object when it "
                                + "should have been the third time");
                    }
                } else {
                    finishBad("onStart() was called when not expected (state="
                            + mExpectedServiceState + ")");
                }
                return true;
            } else if (code == LocalService.DESTROYED_CODE) {
                data.enforceInterface(LocalService.SERVICE_LOCAL);
                if (mExpectedServiceState == STATE_DESTROY) {
                    finishGood();
                } else {
                    finishBad("onDestroy() was called when not expected (state="
                            + mExpectedServiceState + ")");
                }
                return true;
            } else if (code == LocalService.UNBIND_CODE) {
                data.enforceInterface(LocalService.SERVICE_LOCAL);
                if (mExpectedServiceState == STATE_UNBIND) {
                    mExpectedServiceState = STATE_DESTROY;
                } else if (mExpectedServiceState == STATE_UNBIND_ONLY) {
                    finishGood();
                } else {
                    finishBad("onUnbind() was called when not expected (state="
                            + mExpectedServiceState + ")");
                }
                return true;
            } else if (code == LocalService.REBIND_CODE) {
                data.enforceInterface(LocalService.SERVICE_LOCAL);
                if (mExpectedServiceState == STATE_REBIND) {
                    finishGood();
                } else {
                    finishBad("onRebind() was called when not expected (state="
                            + mExpectedServiceState + ")");
                }
                return true;
            } else if (code == LocalService.STOP_SELF_SUCCESS_UNBIND_CODE) {
                data.enforceInterface(LocalService.SERVICE_LOCAL);
                if (mExpectedServiceState == STATE_STOP_SELF_SUCCESS_UNBIND) {
                    finishGood();
                } else {
                    finishBad("onUnbind() was called when not expected (state="
                            + mExpectedServiceState + ")");
                }
                return true;
            } else {
                return super.onTransact(code, data, reply, flags);
            }
        }
    }

    public void testStopSelf() throws Exception {
        TestStopSelfConnection conn = new TestStopSelfConnection();
        boolean success = false;
        final Intent service = new Intent(mContext, LocalStoppedService.class);
        try {
            conn.setMonitor(true);
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(service, conn, 0);
            mContext.startService(service);
            waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);
            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
                mContext.stopService(service);
            }
        }
        // Expect to see the service unbind and then destroyed.
        mExpectedServiceState = STATE_UNBIND;
        conn.stopSelf();
        waitForResultOrThrow(DELAY, EXIST_CONN_TO_LOSE_SERVICE);

        mContext.unbindService(conn);
    }

    public void testStopSelfResult() throws Exception {
        TestStopSelfConnection conn = new TestStopSelfConnection();
        boolean success = false;
        final Intent service = new Intent(mContext, LocalStoppedService.class);
        try {
            conn.setMonitor(true);
            mExpectedServiceState = STATE_START_1;
            mContext.bindService(service, conn, 0);
            mContext.startService(service);
            waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);
            success = true;
        } finally {
            if (!success) {
                mContext.unbindService(conn);
                mContext.stopService(service);
            }
        }
        // Expect to see the service unbind and then destroyed.
        mExpectedServiceState = STATE_STOP_SELF_SUCCESS_UNBIND;
        conn.stopSelfResult();
        waitForResultOrThrow(DELAY, EXIST_CONN_TO_LOSE_SERVICE);

        mContext.unbindService(conn);
    }

    public void testLocalStartClass() throws Exception {
        startExpectResult(mLocalService);
    }

    public void testLocalStartAction() throws Exception {
        startExpectResult(new Intent(
                LocalService.SERVICE_LOCAL, null /*uri*/, mContext, LocalService.class));
    }

    public void testLocalBindClass() throws Exception {
        bindExpectResult(mLocalService);
    }

    public void testBindServiceWithExecutor() throws Exception {
      setupBackgroundThread();

      TestConnection conn = new TestConnection(true, false);
      mExpectedServiceState = STATE_START_1;
      mContext.bindService(
          mLocalService, Context.BIND_AUTO_CREATE, mBackgroundThreadExecutor, conn);
      waitForResultOrThrow(DELAY, EXIST_CONN_TO_RECEIVE_SERVICE);
      assertEquals(mBackgroundThread, conn.getOnServiceConnectedThread());

      mContext.unbindService(conn);
    }

    /* Just the Intent for a foreground service */
    private Intent foregroundServiceIntent(int command) {
        return new Intent(mLocalForegroundService)
                .putExtras(LocalForegroundService.newCommand(mStateReceiver, command));
    }

    private void startForegroundService(int command) {
        mContext.startService(foregroundServiceIntent(command));
    }

    /* Start the service in a way that promises to go into the foreground */
    private void startRequiredForegroundService(int command) {
        mContext.startForegroundService(foregroundServiceIntent(command));
    }

    @MediumTest
    public void testForegroundService_dontRemoveNotificationOnStop() throws Exception {
        boolean success = false;
        try {
            // Start service as foreground - it should show notification #1
            mExpectedServiceState = STATE_START_1;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start first time");
            assertNotification(1, LocalForegroundService.getNotificationTitle(1));

            // Stop foreground without removing notification - it should still show notification #1
            mExpectedServiceState = STATE_START_2;
            startForegroundService(
                    LocalForegroundService.COMMAND_STOP_FOREGROUND_DONT_REMOVE_NOTIFICATION);
            waitForResultOrThrow(DELAY, "service to stop foreground");
            assertNotification(1, LocalForegroundService.getNotificationTitle(1));

            // Sends another notification reusing the same notification id.
            String newTitle = "YODA I AM";
            sendNotification(1, newTitle);
            assertNotification(1, newTitle);

            // Start service as foreground again - it should kill notification #1 and show #2
            mExpectedServiceState = STATE_START_3;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start foreground 2nd time");
            assertNoNotification(1);
            assertNotification(2, LocalForegroundService.getNotificationTitle(2));

            success = true;
        } finally {
            if (!success) {
                mContext.stopService(mLocalForegroundService);
            }
        }
        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(mLocalForegroundService);
        waitForResultOrThrow(DELAY, "service to be destroyed");
        assertNoNotification(1);
        assertNoNotification(2);
    }

    @MediumTest
    public void testForegroundService_removeNotificationOnStop() throws Exception {
        testForegroundServiceRemoveNotificationOnStop(false);
    }

    @MediumTest
    public void testForegroundService_removeNotificationOnStopUsingFlags() throws Exception {
        testForegroundServiceRemoveNotificationOnStop(true);
    }

    private void testForegroundServiceRemoveNotificationOnStop(boolean usingFlags)
            throws Exception {
        boolean success = false;
        try {
            // Start service as foreground - it should show notification #1
            Log.d(TAG, "Expecting first start state...");
            mExpectedServiceState = STATE_START_1;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start first time");
            assertNotification(1, LocalForegroundService.getNotificationTitle(1));

            // Stop foreground removing notification
            Log.d(TAG, "Expecting second start state...");
            mExpectedServiceState = STATE_START_2;
            if (usingFlags) {
                startForegroundService(LocalForegroundService
                        .COMMAND_STOP_FOREGROUND_REMOVE_NOTIFICATION_USING_FLAGS);
            } else {
                startForegroundService(LocalForegroundService
                        .COMMAND_STOP_FOREGROUND_REMOVE_NOTIFICATION);
            }
            waitForResultOrThrow(DELAY, "service to stop foreground");
            assertNoNotification(1);

            // Start service as foreground again - it should show notification #2
            mExpectedServiceState = STATE_START_3;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start as foreground 2nd time");
            assertNotification(2, LocalForegroundService.getNotificationTitle(2));

            success = true;
        } finally {
            if (!success) {
                mContext.stopService(mLocalForegroundService);
            }
        }
        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(mLocalForegroundService);
        waitForResultOrThrow(DELAY, "service to be destroyed");
        assertNoNotification(1);
        assertNoNotification(2);
    }

    @FlakyTest
    public void testRunningServices() throws Exception {
        final int maxReturnedServices = 10;
        final Bundle bundle = new Bundle();
        bundle.putParcelable(LocalService.REPORT_OBJ_NAME, new IBinderParcelable(mStateReceiver));

        boolean success = false;

        ActivityManager am = mContext.getSystemService(ActivityManager.class);

        // Put target app on whitelist so we can start its services.
        SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
                "cmd deviceidle whitelist +" + EXTERNAL_SERVICE_PACKAGE);

        // No services should be reported back at the beginning
        assertEquals(0, am.getRunningServices(maxReturnedServices).size());
        try {
            mExpectedServiceState = STATE_START_1;
            // Start external service.
            mContext.startService(new Intent(mExternalService).putExtras(bundle));
            waitForResultOrThrow(DELAY, "external service to start first time");

            // Ensure we can't see service.
            assertEquals(0, am.getRunningServices(maxReturnedServices).size());

            // Start local service.
            mContext.startService(new Intent(mLocalService).putExtras(bundle));
            waitForResultOrThrow(DELAY, "local service to start first time");
            success = true;

            // Ensure we can see service and it is ours.
            List<ActivityManager.RunningServiceInfo> services = am.getRunningServices(maxReturnedServices);
            assertEquals(1, services.size());
            assertEquals(android.os.Process.myUid(), services.get(0).uid);
        } finally {
            SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
                    "cmd deviceidle whitelist -" + EXTERNAL_SERVICE_PACKAGE);
            if (!success) {
                mContext.stopService(mLocalService);
                mContext.stopService(mExternalService);
            }
        }
        mExpectedServiceState = STATE_DESTROY;

        mContext.stopService(mExternalService);
        waitForResultOrThrow(DELAY, "external service to be destroyed");

        mContext.stopService(mLocalService);
        waitForResultOrThrow(DELAY, "local service to be destroyed");

        // Once our service has stopped, make sure we can't see any services.
        assertEquals(0, am.getRunningServices(maxReturnedServices).size());
    }

    @MediumTest
    public void testForegroundService_detachNotificationOnStop() throws Exception {
        String newTitle = null;
        boolean success = false;
        try {

            // Start service as foreground - it should show notification #1
            mExpectedServiceState = STATE_START_1;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start first time");
            assertNotification(1, LocalForegroundService.getNotificationTitle(1));

            // Detaching notification
            mExpectedServiceState = STATE_START_2;
            startForegroundService(
                    LocalForegroundService.COMMAND_STOP_FOREGROUND_DETACH_NOTIFICATION);
            waitForResultOrThrow(DELAY, "service to stop foreground");
            assertNotification(1, LocalForegroundService.getNotificationTitle(1));

            // Sends another notification reusing the same notification id.
            newTitle = "YODA I AM";
            sendNotification(1, newTitle);
            assertNotification(1, newTitle);

            // Start service as foreground again - it should show notification #2..
            mExpectedServiceState = STATE_START_3;
            startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
            waitForResultOrThrow(DELAY, "service to start as foreground 2nd time");
            assertNotification(2, LocalForegroundService.getNotificationTitle(2));
            //...but keeping notification #1
            assertNotification(1, newTitle);

            success = true;
        } finally {
            if (!success) {
                mContext.stopService(mLocalForegroundService);
            }
        }
        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(mLocalForegroundService);
        waitForResultOrThrow(DELAY, "service to be destroyed");
        if (newTitle == null) {
            assertNoNotification(1);
        } else {
            assertNotification(1, newTitle);
            cancelNotification(1);
            assertNoNotification(1);
        }
        assertNoNotification(2);
    }

    public void testForegroundService_notificationChannelDeletion() throws Exception {
        NotificationManager noMan = mContext.getSystemService(NotificationManager.class);

        // Start service as foreground - it should show notification #1
        mExpectedServiceState = STATE_START_1;
        startForegroundService(LocalForegroundService.COMMAND_START_FOREGROUND);
        waitForResultOrThrow(DELAY, "service to start first time");
        assertNotification(1, LocalForegroundService.getNotificationTitle(1));

        try {
            final String channel = LocalForegroundService.NOTIFICATION_CHANNEL_ID;
            noMan.deleteNotificationChannel(channel);
            fail("Deleting FGS notification channel did not throw");
        } catch (SecurityException se) {
            // Expected outcome, i.e. success case
        } catch (Exception e) {
            fail("Deleting FGS notification threw unexpected failure " + e);
        }

        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(mLocalForegroundService);
        waitForResultOrThrow(DELAY, "service to be destroyed");

    }

    class TestSendCallback implements PendingIntent.OnFinished {
        public volatile int result = -1;

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            Log.i(TAG, "foreground service PendingIntent callback got " + resultCode);
            this.result = resultCode;
        }
    }

    @MediumTest
    public void testForegroundService_pendingIntentForeground() throws Exception {
        boolean success = false;

        PendingIntent pi = PendingIntent.getForegroundService(mContext, 1,
                foregroundServiceIntent(LocalForegroundService.COMMAND_START_FOREGROUND),
                PendingIntent.FLAG_CANCEL_CURRENT);
        TestSendCallback callback = new TestSendCallback();

        try {
            mExpectedServiceState = STATE_START_1;
            pi.send(5038, callback, null);
            waitForResultOrThrow(DELAY, "service to start first time");
            assertTrue(callback.result > -1);

            success = true;
        } finally {
            if (!success) {
                mContext.stopService(mLocalForegroundService);
            }
        }

        mExpectedServiceState = STATE_DESTROY;
        mContext.stopService(mLocalForegroundService);
        waitForResultOrThrow(DELAY, "pendingintent service to be destroyed");
    }

    @MediumTest
    public void testLocalBindAction() throws Exception {
        bindExpectResult(new Intent(
                LocalService.SERVICE_LOCAL, null /*uri*/, mContext, LocalService.class));
    }

    @MediumTest
    public void testLocalBindAutoClass() throws Exception {
        bindAutoExpectResult(mLocalService);
    }

    @MediumTest
    public void testLocalBindAutoAction() throws Exception {
        bindAutoExpectResult(new Intent(
                LocalService.SERVICE_LOCAL, null /*uri*/, mContext, LocalService.class));
    }

    @MediumTest
    public void testLocalStartClassPermissions() throws Exception {
        startExpectResult(mLocalGrantedService);
        startExpectResult(mLocalDeniedService);
    }

    @MediumTest
    public void testLocalStartActionPermissions() throws Exception {
        startExpectResult(mLocalService_ApplicationHasPermission);
        startExpectResult(mLocalService_ApplicationDoesNotHavePermission);
    }

    @MediumTest
    public void testLocalBindClassPermissions() throws Exception {
        bindExpectResult(mLocalGrantedService);
        bindExpectResult(mLocalDeniedService);
    }

    @MediumTest
    public void testLocalBindActionPermissions() throws Exception {
        bindExpectResult(mLocalService_ApplicationHasPermission);
        bindExpectResult(mLocalService_ApplicationDoesNotHavePermission);
    }

    @MediumTest
    public void testLocalBindAutoClassPermissionGranted() throws Exception {
        bindAutoExpectResult(mLocalGrantedService);
    }

    @MediumTest
    public void testLocalBindAutoActionPermissionGranted() throws Exception {
        bindAutoExpectResult(mLocalService_ApplicationHasPermission);
    }

    @MediumTest
    public void testLocalUnbindTwice() throws Exception {
        EmptyConnection conn = new EmptyConnection();
        mContext.bindService(
                mLocalService_ApplicationHasPermission, conn, 0);
        mContext.unbindService(conn);
        try {
            mContext.unbindService(conn);
            fail("No exception thrown on the second unbind");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    @MediumTest
    public void testImplicitIntentFailsOnApiLevel21() throws Exception {
        Intent intent = new Intent(LocalService.SERVICE_LOCAL);
        EmptyConnection conn = new EmptyConnection();
        try {
            mContext.bindService(intent, conn, 0);
            mContext.unbindService(conn);
            fail("Implicit intents should be disallowed for apps targeting API 21+");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    /**
     * Verify that when the requested service's onBind() returns null,
     * the connection's onNullBinding() method is invoked.
     */
    @MediumTest
    public void testNullServiceBinder() throws Exception {
        Intent intent = new Intent(mContext, NullService.class);
        intent.setAction("testNullServiceBinder");
        NullServiceConnection conn1 = new NullServiceConnection();
        NullServiceConnection conn2 = new NullServiceConnection();
        try {
            assertTrue(mContext.bindService(intent, conn1, Context.BIND_AUTO_CREATE));
            conn1.waitForNullBinding(DELAY);
            assertTrue(conn1.nullBindingReceived());

            assertTrue(mContext.bindService(intent, conn2, Context.BIND_AUTO_CREATE));
            conn2.waitForNullBinding(DELAY);
            assertTrue(conn2.nullBindingReceived());
        } finally {
            mContext.unbindService(conn1);
            mContext.unbindService(conn2);
        }
    }

    /**
     * Verify that we can't use bindIsolatedService() on a non-isolated service.
     */
    @MediumTest
    public void testFailBindNonIsolatedService() throws Exception {
        EmptyConnection conn = new EmptyConnection();
        try {
            mContext.bindIsolatedService(mLocalService, 0, "isolated", mContextMainExecutor, conn);
            mContext.unbindService(conn);
            fail("Didn't get IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // This is expected.
        }
    }

    /**
     * Verify that certain characters are prohibited in instanceName.
     */
    public void testFailBindIsoaltedServiceWithInvalidInstanceName() throws Exception {
        String[] badNames = {
            "t\rest",
            "test\n",
            "test-three",
            "test four",
            "escape\u00a9seq",
            "\u0164est",
        };
        for (String instanceName : badNames) {
            EmptyConnection conn = new EmptyConnection();
            try {
                mContext.bindIsolatedService(mIsolatedService, Context.BIND_AUTO_CREATE,
                        instanceName, mContextMainExecutor, conn);
                mContext.unbindService(conn);
                fail("Didn't get IllegalArgumentException: " + instanceName);
            } catch (IllegalArgumentException e) {
                // This is expected.
            }
        }
    }

    /**
     * Verify that bindIsolatedService() correctly makes different instances when given
     * different instance names.
     */
    @MediumTest
    public void testBindIsolatedServiceInstances() throws Exception {
        IsolatedConnection conn1a = null;
        IsolatedConnection conn1b = null;
        IsolatedConnection conn2 = null;
        try {
            conn1a = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn1a);
            conn1b = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn1b);
            conn2 = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "2", mContextMainExecutor, conn2);

            conn1a.waitForService(DELAY);
            conn1b.waitForService(DELAY);
            conn2.waitForService(DELAY);

            if (conn1a.getPid() != conn1b.getPid()) {
                fail("Connections to same service name in different pids");
            }
            if (conn1a.getPid() == conn2.getPid()) {
                fail("Connections to different service names in same pids");
            }

            conn1a.setValue(1);
            assertEquals(1, conn1a.getValue());
            assertEquals(1, conn1b.getValue());

            conn2.setValue(2);
            assertEquals(1, conn1a.getValue());
            assertEquals(1, conn1b.getValue());
            assertEquals(2, conn2.getValue());

            conn1b.setValue(3);
            assertEquals(3, conn1a.getValue());
            assertEquals(3, conn1b.getValue());
            assertEquals(2, conn2.getValue());
        } finally {
            if (conn2 != null) {
                mContext.unbindService(conn2);
            }
            if (conn1b != null) {
                mContext.unbindService(conn1b);
            }
            if (conn1a != null) {
                mContext.unbindService(conn1a);
            }
        }
    }

    public void testBindIsolatedServiceOnBackgroundThread() throws Exception {
        setupBackgroundThread();
        IsolatedConnection conn = new IsolatedConnection();
        mContext.bindIsolatedService(mIsolatedService, Context.BIND_AUTO_CREATE,
            "background_instance", mBackgroundThreadExecutor, conn);
        conn.waitForService(DELAY);
        assertEquals(mBackgroundThread, conn.getOnServiceConnectedThread());
        mContext.unbindService(conn);
    }

    static final int BINDING_WEAK = 0;
    static final int BINDING_STRONG = 1;
    static final int BINDING_ANY = -1;

    final class IsolatedConnectionInfo {
        final int mStrong;
        final String mInstanceName;
        final String mLabel;
        int mGroup;
        int mImportance;
        IsolatedConnection mConnection;

        IsolatedConnectionInfo(int group, int importance, int strong) {
            mGroup = group;
            mImportance = importance;
            mStrong = strong;
            mInstanceName = group + "_" + importance;
            StringBuilder b = new StringBuilder(mInstanceName);
            b.append('_');
            if (strong == BINDING_WEAK) {
                b.append('W');
            } else if (strong == BINDING_STRONG) {
                b.append('S');
            } else {
                b.append(strong);
            }
            mLabel = b.toString();
        }

        void setGroup(int group) {
            mGroup = group;
        }

        void setImportance(int importance) {
            mImportance = importance;
        }

        boolean match(int group, int strong) {
            return (group < 0 || mGroup == group)
                    && (strong == BINDING_ANY || mStrong == strong);
        }

        boolean bind(Context context) {
            if (mConnection != null) {
                return true;
            }
            Log.i("XXXXXXX", "Binding " + mLabel + ": conn=" + mConnection
                    + " context=" + context);
            mConnection = new IsolatedConnection();
            boolean result = context.bindIsolatedService(
                    mIsolatedService,
                    Context.BIND_AUTO_CREATE | Context.BIND_DEBUG_UNBIND
                            | (mStrong == BINDING_STRONG ? 0 : Context.BIND_ALLOW_OOM_MANAGEMENT),
                    mInstanceName, mContextMainExecutor, mConnection);
            if (!result) {
                mConnection = null;
            }
            return result;
        }

        IsolatedConnection getConnection() {
            return mConnection;
        }

        void unbind(Context context) {
            if (mConnection != null) {
                Log.i("XXXXXXX", "Unbinding " + mLabel + ": conn=" + mConnection
                        + " context=" + context);
                context.unbindService(mConnection);
                mConnection = null;
            }
        }
    }

    final class LruOrderItem {
        static final int FLAG_SKIP_UNKNOWN = 1<<0;

        final IsolatedConnectionInfo mInfo;
        final int mUid;
        final int mFlags;

        LruOrderItem(IsolatedConnectionInfo info, int flags) {
            mInfo = info;
            mUid = -1;
            mFlags = flags;
        }

        LruOrderItem(int uid, int flags) {
            mInfo = null;
            mUid = uid;
            mFlags = flags;
        }

        IsolatedConnectionInfo getInfo() {
            return mInfo;
        }

        int getUid() {
            return mInfo != null ? mInfo.getConnection().getUid() : mUid;
        }

        int getUserId() {
            return UserHandle.getUserHandleForUid(getUid()).getIdentifier();
        }

        int getAppId() {
            return UserHandle.getAppId(getUid());
        }

        boolean isEquivalentTo(ProcessRecordProto proc) {
            int procAppId = proc.isolatedAppId != 0 ? proc.isolatedAppId : UserHandle.getAppId(
                    proc.uid);

            // Compare appid and userid separately because UserHandle.getUid is @hide.
            return procAppId == getAppId() && proc.userId == getUserId();
        }

        int getFlags() {
            return mFlags;
        }
    }

    private void doBind(Context context, IsolatedConnectionInfo[] connections, int group,
            int strong) {
        for (IsolatedConnectionInfo ci : connections) {
            if (ci.match(group, strong)) {
                ci.bind(context);
            }
        }
    }

    private void doBind(Context context, IsolatedConnectionInfo[] connections, int[] selected) {
        for (int i : selected) {
            boolean result = connections[i].bind(context);
            if (!result) {
                fail("Unable to bind connection " + connections[i].mLabel);
            }
        }
    }

    private void doWaitForService(IsolatedConnectionInfo[] connections, int group,
            int strong) {
        for (IsolatedConnectionInfo ci : connections) {
            if (ci.match(group, strong)) {
                ci.mConnection.waitForService(DELAY);
            }
        }
    }

    private void doUpdateServiceGroup(Context context, IsolatedConnectionInfo[] connections,
            int group, int strong) {
        for (IsolatedConnectionInfo ci : connections) {
            if (ci.match(group, strong)) {
                context.updateServiceGroup(ci.mConnection, ci.mGroup, ci.mImportance);
            }
        }
    }

    private void doUnbind(Context context, IsolatedConnectionInfo[] connections, int group,
            int strong) {
        for (IsolatedConnectionInfo ci : connections) {
            if (ci.match(group, strong)) {
                ci.unbind(context);
            }
        }
    }

    private void doUnbind(Context context, IsolatedConnectionInfo[] connections, int[] selected) {
        for (int i : selected) {
            connections[i].unbind(context);
        }
    }

    List<ProcessRecordProto> getLruProcesses() {
        ActivityManagerServiceDumpProcessesProto dump = getActivityManagerProcesses();
        SparseArray<ProcessRecordProto> procs = new SparseArray<>();
        ProcessRecordProto[] procsList = dump.procs;
        for (ProcessRecordProto proc : procsList) {
            procs.put(proc.lruIndex, proc);
        }
        ArrayList<ProcessRecordProto> lruProcs = new ArrayList<>();
        for (int i = 0; i < procs.size(); i++) {
            lruProcs.add(procs.valueAt(i));
        }
        return lruProcs;
    }

    String printProc(int i, ProcessRecordProto proc) {
        return "#" + i + ": " + proc.processName
                + " pid=" + proc.pid + " uid=" + proc.uid
                + (proc.isolatedAppId != 0 ? " isolated=" + proc.isolatedAppId : "");
    }

    private void logProc(int i, ProcessRecordProto proc) {
        Log.i("XXXXXXXX", printProc(i, proc));
    }

    private void verifyLruOrder(LruOrderItem[] orderItems) {
        List<ProcessRecordProto> procs = getLruProcesses();
        Log.i("XXXXXXXX", "Processes:");
        int orderI = 0;
        for (int i = procs.size() - 1; i >= 0; i--) {
            ProcessRecordProto proc = procs.get(i);
            logProc(i, proc);
            final LruOrderItem lru = orderItems[orderI];
            Log.i("XXXXXXXX", "Expecting uid: " + lru.getUid());
            if (!lru.isEquivalentTo(proc)) {
                if ((lru.getFlags() & LruOrderItem.FLAG_SKIP_UNKNOWN) != 0) {
                    while (i > 0) {
                        i--;
                        proc = procs.get(i);
                        logProc(i, proc);
                        if (lru.isEquivalentTo(proc)) {
                            break;
                        }
                    }
                }
                if (!lru.isEquivalentTo(proc)) {
                    if ((lru.getFlags() & LruOrderItem.FLAG_SKIP_UNKNOWN) != 0) {
                        fail("Didn't find expected LRU proc uid=" + lru.getUid());
                    }
                    fail("Expected proc uid=" + lru.getUid() + " at found proc "
                            + printProc(i, proc));
                }
            }
            orderI++;
            if (orderI >= orderItems.length) {
                return;
            }
        }
    }

    @MediumTest
    public void testAppZygotePreload() throws Exception {
        IsolatedConnection conn = new IsolatedConnection();
        try {
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn);

            conn.waitForService(DELAY);

            // Verify application preload was done
            assertTrue(conn.zygotePreloadCalled());
        } finally {
            if (conn != null) {
                mContext.unbindService(conn);
            }
        }
    }

    @MediumTest
    public void testAppZygoteServices() throws Exception {
        IsolatedConnection conn1a = null;
        IsolatedConnection conn1b = null;
        IsolatedConnection conn2 = null;
        int appZygotePid;
        try {
            conn1a = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn1a);
            conn1b = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn1b);
            conn2 = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "2", mContextMainExecutor, conn2);

            conn1a.waitForService(DELAY);
            conn1b.waitForService(DELAY);
            conn2.waitForService(DELAY);

            // Get PPID of each service, and verify they're identical
            int ppid1a = conn1a.getPpid();
            int ppid1b = conn1b.getPpid();
            int ppid2 = conn2.getPpid();

            assertEquals(ppid1a, ppid1b);
            assertEquals(ppid1b, ppid2);
            // Find the app zygote process hosting these
            String result = SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
                "ps -p " + Integer.toString(ppid1a) + " -o NAME=");
            result = result.replaceAll("\\s+", "");
            assertEquals(result, APP_ZYGOTE_PROCESS_NAME);
            appZygotePid = ppid1a;
        } finally {
            if (conn2 != null) {
                mContext.unbindService(conn2);
            }
            if (conn1b != null) {
                mContext.unbindService(conn1b);
            }
            if (conn1a != null) {
                mContext.unbindService(conn1a);
            }
        }
        // Sleep for 2 seconds and bind a service again, see it uses the same Zygote
        try {
            conn1a = new IsolatedConnection();
            mContext.bindIsolatedService(
                    mIsolatedService, Context.BIND_AUTO_CREATE, "1", mContextMainExecutor, conn1a);

            conn1a.waitForService(DELAY);

            int ppid1a = conn1a.getPpid();
            assertEquals(appZygotePid, ppid1a);
        } finally {
            if (conn1a != null) {
                mContext.unbindService(conn1a);
            }
        }
        // Sleep for 10 seconds, verify the app_zygote is gone
        Thread.sleep(10000);
        String result = SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
            "ps -p " + Integer.toString(appZygotePid) + " -o NAME=");
        result = result.replaceAll("\\s+", "");
        assertEquals("", result);
    }

    /**
     * Test that the system properly orders processes bound by an activity within the
     * LRU list.
     */
    // TODO(b/131059432): Re-enable the test after that bug is fixed.
    @FlakyTest
    @MediumTest
    public void testActivityServiceBindingLru() throws Exception {
        // Bring up the activity we will hang services off of.
        runLaunchpad(LaunchpadActivity.ACTIVITY_PREPARE);

        final Activity a = getRunningActivity();

        final int CONN_1_1_W = 0;
        final int CONN_1_1_S = 1;
        final int CONN_1_2_W = 2;
        final int CONN_1_2_S = 3;
        final int CONN_2_1_W = 4;
        final int CONN_2_1_S = 5;
        final int CONN_2_2_W = 6;
        final int CONN_2_2_S = 7;
        final int CONN_2_3_W = 8;
        final int CONN_2_3_S = 9;

        // We are going to have both weak and strong references to services, so we can allow
        // some to go down in the LRU list.
        final IsolatedConnectionInfo[] connections = new IsolatedConnectionInfo[] {
                new IsolatedConnectionInfo(1, 1, BINDING_WEAK),
                new IsolatedConnectionInfo(1, 1, BINDING_STRONG),
                new IsolatedConnectionInfo(1, 2, BINDING_WEAK),
                new IsolatedConnectionInfo(1, 2, BINDING_STRONG),
                new IsolatedConnectionInfo(2, 1, BINDING_WEAK),
                new IsolatedConnectionInfo(2, 1, BINDING_STRONG),
                new IsolatedConnectionInfo(2, 2, BINDING_WEAK),
                new IsolatedConnectionInfo(2, 2, BINDING_STRONG),
                new IsolatedConnectionInfo(2, 3, BINDING_WEAK),
                new IsolatedConnectionInfo(2, 3, BINDING_STRONG),
        };

        final int[] REV_GROUP_1_STRONG = new int[] {
                CONN_1_2_S, CONN_1_1_S
        };

        final int[] REV_GROUP_2_STRONG = new int[] {
                CONN_2_3_S, CONN_2_2_S, CONN_2_1_S
        };

        final int[] MIXED_GROUP_3_STRONG = new int[] {
                CONN_2_3_S, CONN_1_1_S, CONN_2_1_S, CONN_2_2_S
        };

        boolean passed = false;

        try {
            // Start the group 1 processes as weak.
            doBind(a, connections, 1, BINDING_WEAK);
            doUpdateServiceGroup(a, connections, 1, BINDING_WEAK);

            // Wait for them to come up.
            doWaitForService(connections, 1, BINDING_WEAK);

            // Now fully bind to the services.
            doBind(a, connections, 1, BINDING_STRONG);
            doWaitForService(connections, 1, BINDING_STRONG);

            verifyLruOrder(new LruOrderItem[] {
                    new LruOrderItem(Process.myUid(), 0),
                    new LruOrderItem(connections[CONN_1_1_W], 0),
                    new LruOrderItem(connections[CONN_1_2_W], 0),
            });

            // Now remove the full binding, leaving only the weak.
            doUnbind(a, connections, 1, BINDING_STRONG);

            // Start the group 2 processes as weak.
            doBind(a, connections, 2, BINDING_WEAK);

            // Wait for them to come up.
            doWaitForService(connections, 2, BINDING_WEAK);

            // Set the group and index.  In this case we do it after we know the process
            // is started, to make sure setting it directly works.
            doUpdateServiceGroup(a, connections, 2, BINDING_WEAK);

            // Now fully bind to group 2
            doBind(a, connections, REV_GROUP_2_STRONG);

            verifyLruOrder(new LruOrderItem[] {
                    new LruOrderItem(Process.myUid(), 0),
                    new LruOrderItem(connections[CONN_2_1_W], 0),
                    new LruOrderItem(connections[CONN_2_2_W], 0),
                    new LruOrderItem(connections[CONN_2_3_W], 0),
                    new LruOrderItem(connections[CONN_1_1_W], LruOrderItem.FLAG_SKIP_UNKNOWN),
                    new LruOrderItem(connections[CONN_1_2_W], 0),
            });

            // Bring group 1 back to the foreground, but in the opposite order.
            doBind(a, connections, REV_GROUP_1_STRONG);

            verifyLruOrder(new LruOrderItem[] {
                    new LruOrderItem(Process.myUid(), 0),
                    new LruOrderItem(connections[CONN_1_1_W], 0),
                    new LruOrderItem(connections[CONN_1_2_W], 0),
                    new LruOrderItem(connections[CONN_2_1_W], LruOrderItem.FLAG_SKIP_UNKNOWN),
                    new LruOrderItem(connections[CONN_2_2_W], 0),
                    new LruOrderItem(connections[CONN_2_3_W], 0),
            });

            // Now remove all full bindings, keeping only weak.
            doUnbind(a, connections, 1, BINDING_STRONG);
            doUnbind(a, connections, 2, BINDING_STRONG);

            // Change the grouping and importance to make sure that gets reflected.
            connections[CONN_1_1_W].setGroup(3);
            connections[CONN_1_1_W].setImportance(1);
            connections[CONN_2_1_W].setGroup(3);
            connections[CONN_2_1_W].setImportance(2);
            connections[CONN_2_2_W].setGroup(3);
            connections[CONN_2_2_W].setImportance(3);
            connections[CONN_2_3_W].setGroup(3);
            connections[CONN_2_3_W].setImportance(4);

            doUpdateServiceGroup(a, connections, 3, BINDING_WEAK);

            // Now bind them back up in an interesting order.
            doBind(a, connections, MIXED_GROUP_3_STRONG);

            verifyLruOrder(new LruOrderItem[] {
                    new LruOrderItem(Process.myUid(), 0),
                    new LruOrderItem(connections[CONN_1_1_W], 0),
                    new LruOrderItem(connections[CONN_2_1_W], 0),
                    new LruOrderItem(connections[CONN_2_2_W], 0),
                    new LruOrderItem(connections[CONN_2_3_W], 0),
                    new LruOrderItem(connections[CONN_1_2_W], LruOrderItem.FLAG_SKIP_UNKNOWN),
            });

            passed = true;

        } finally {
            if (!passed) {
                List<ProcessRecordProto> procs = getLruProcesses();
                Log.i("XXXXXXXX", "Processes:");
                for (int i = procs.size() - 1; i >= 0; i--) {
                    ProcessRecordProto proc = procs.get(i);
                    logProc(i, proc);
                }
            }
            doUnbind(a, connections, -1, BINDING_ANY);
        }
    }
}
