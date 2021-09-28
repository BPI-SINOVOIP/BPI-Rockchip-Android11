/*
 * Copyright (C) 2015 The Android Open Source Project
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
package android.car.apitest;

import android.car.content.pm.AppBlockingPackageInfo;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.Signature;
import android.os.Parcel;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

@SmallTest
public class AppBlockingPackageInfoTest {
    private static final String TAG = AppBlockingPackageInfoTest.class.getSimpleName();

    private final Context mContext = InstrumentationRegistry.getInstrumentation()
            .getTargetContext();

    @Test
    public void testParcellingSystemInfo() throws Exception {
        AppBlockingPackageInfo carServiceInfo = createInfoCarService(mContext);
        Parcel dest = Parcel.obtain();
        carServiceInfo.writeToParcel(dest, 0);
        dest.setDataPosition(0);
        AppBlockingPackageInfo carServiceInfoRead = new AppBlockingPackageInfo(dest);
        Log.i(TAG, "expected:" + carServiceInfo + ",read:" + carServiceInfoRead);
        assertThat(carServiceInfoRead).isEqualTo(carServiceInfo);
    }

    @Test
    public void testParcellingNonSystemInfo() throws Exception {
        AppBlockingPackageInfo selfInfo = createInfoSelf(mContext);
        Parcel dest = Parcel.obtain();
        selfInfo.writeToParcel(dest, 0);
        dest.setDataPosition(0);
        AppBlockingPackageInfo selfInfoRead = new AppBlockingPackageInfo(dest);
        Log.i(TAG, "expected:" + selfInfo + ",read:" + selfInfoRead);
        assertThat(selfInfoRead).isEqualTo(selfInfo);
    }

    public static AppBlockingPackageInfo createInfoCarService(Context context) {
        final String packageName = "android.car";
        return new AppBlockingPackageInfo(packageName, 0, 0, AppBlockingPackageInfo.FLAG_SYSTEM_APP,
                null, null);
    }

    public static final AppBlockingPackageInfo createInfoSelf(Context context) {
        final String packageName = "android.car.apitest";
        PackageManager pm = context.getPackageManager();
        Signature[] signatures;
        try {
            signatures = pm.getPackageInfo(packageName, PackageManager.GET_SIGNATURES).signatures;
        } catch (NameNotFoundException e) {
            return null;
        }
        String[] activties = new String[] { "Hello", "World" };
        return new AppBlockingPackageInfo(packageName, 0, 100, 0, signatures, activties);
    }
}
