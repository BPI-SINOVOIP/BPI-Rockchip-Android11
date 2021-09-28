/*
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

package android.binder.cts;

import android.os.IBinder;
import android.os.Parcel;

import androidx.test.InstrumentationRegistry;

import com.android.gtestrunner.GtestRunner;
import com.android.gtestrunner.TargetLibrary;

import org.junit.runner.RunWith;

/**
 * This test runs gtests for testing libbinder_ndk directly and it also runs client tests for native code.
 */
@RunWith(GtestRunner.class)
@TargetLibrary("binder_ndk_test")
public class NdkBinderTest {
    static IBinder getLocalNativeService() {
        return new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), NativeService.Local.class)
        .get().asBinder();
    }
    static IBinder getLocalJavaService() {
        return new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), JavaService.Local.class)
        .get().asBinder();
    }
    static IBinder getRemoteNativeService() {
        return new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), NativeService.Remote.class)
        .get().asBinder();
    }
    static IBinder getRemoteJavaService() {
        return new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), JavaService.Remote.class)
        .get().asBinder();
    }
    static IBinder getRemoteOldNativeService() {
        return new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), NativeService.RemoteOld.class)
        .get().asBinder();
    }
    static Parcel getEmptyParcel() {
        return Parcel.obtain();
    }
}
