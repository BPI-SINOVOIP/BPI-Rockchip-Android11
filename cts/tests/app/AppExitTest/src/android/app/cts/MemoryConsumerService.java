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

package android.app.cts;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;
import android.util.Pair;

import java.io.FileDescriptor;
import java.nio.DirectByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

public class MemoryConsumerService extends Service {
    private static final String TAG = MemoryConsumerService.class.getSimpleName();

    private static final int ACTION_RUN_ONCE = IBinder.FIRST_CALL_TRANSACTION;
    private static final String EXTRA_BUNDLE = "bundle";
    private static final String EXTRA_TEST_FUNC = "test_func";

    private IBinder mTestFunc;

    private IBinder mBinder = new Binder() {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            switch (code) {
                case ACTION_RUN_ONCE:
                    reply.writeBoolean(fillUpMemoryAndCheck(data.readLong(), mTestFunc));
                    return true;
                default:
                    return false;
            }
        }
    };

    static class TestFuncInterface extends Binder {
        static final int ACTION_TEST = IBinder.FIRST_CALL_TRANSACTION;

        private final BooleanSupplier mTestFunc;

        TestFuncInterface(final BooleanSupplier testFunc) {
            mTestFunc = testFunc;
        }

        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            switch (code) {
                case ACTION_TEST:
                    reply.writeBoolean(mTestFunc.getAsBoolean());
                    return true;
                default:
                    return false;
            }
        }
    }

    private boolean fillUpMemoryAndCheck(final long totalMb, final IBinder testFunc) {
        final int pageSize = 4096;
        final int oneMb = 1024 * 1024;

        // Create an empty fd -1
        FileDescriptor fd = new FileDescriptor();

        // Okay now start a loop to allocate 1MB each time and check if our process is gone.
        for (long i = 0; i < totalMb; i++) {
            long addr = 0L;
            try {
                addr = Os.mmap(0, oneMb, OsConstants.PROT_WRITE,
                        OsConstants.MAP_PRIVATE | OsConstants.MAP_ANONYMOUS, fd, 0);
            } catch (ErrnoException e) {
                Log.d(TAG, "mmap returns " + e.errno);
                if (e.errno == OsConstants.ENOMEM) {
                    // Running out of address space?
                    return false;
                }
            }
            if (addr == 0) {
                return false;
            }

            // We don't have direct access to Memory.pokeByte() though
            DirectByteBuffer buf = new DirectByteBuffer(oneMb, addr, fd, null, false);

            // Dirt the buffer
            for (int j = 0; j < oneMb; j += pageSize) {
                buf.put(j, (byte) 0xf);
            }

            // Test to see if we could stop
            Parcel data = Parcel.obtain();
            Parcel reply = Parcel.obtain();
            try {
                testFunc.transact(TestFuncInterface.ACTION_TEST, data, reply, 0);
                if (reply.readBoolean()) {
                    break;
                }
            } catch (RemoteException e) {
                // Ignore
            } finally {
                data.recycle();
                reply.recycle();
            }
        }
        return true;
    }

    @Override
    public IBinder onBind(Intent intent) {
        final Bundle bundle = intent.getBundleExtra(EXTRA_BUNDLE);
        mTestFunc = bundle.getBinder(EXTRA_TEST_FUNC);
        return mBinder;
    }

    static Pair<IBinder, ServiceConnection> bindToService(final Context context,
            final TestFuncInterface testFunc, String instanceName) throws Exception {
        final Intent intent = new Intent();
        intent.setClass(context, MemoryConsumerService.class);
        final Bundle bundle = new Bundle();
        bundle.putBinder(EXTRA_TEST_FUNC, testFunc);
        intent.putExtra(EXTRA_BUNDLE, bundle);
        final String keyIBinder = "ibinder";
        final Bundle holder = new Bundle();
        final CountDownLatch latch = new CountDownLatch(1);
        final ServiceConnection conn = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                holder.putBinder(keyIBinder, service);
                latch.countDown();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
            }
        };
        context.bindIsolatedService(intent, Context.BIND_AUTO_CREATE,
                instanceName, AsyncTask.THREAD_POOL_EXECUTOR, conn);
        latch.await(10_000, TimeUnit.MILLISECONDS);
        return new Pair<>(holder.getBinder(keyIBinder), conn);
    }

    static boolean runOnce(final IBinder binder, final long totalMb) {
        final Parcel data = Parcel.obtain();
        final Parcel reply = Parcel.obtain();

        try {
            data.writeLong(totalMb);
            binder.transact(ACTION_RUN_ONCE, data, reply, 0);
            return reply.readBoolean();
        } catch (RemoteException e) {
            return false;
        } finally {
            data.recycle();
            reply.recycle();
        }
    }
}
