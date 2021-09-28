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

package android.os.storage.cts;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.Parcel;
import android.os.Process;
import android.os.storage.CrateInfo;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class CrateInfoTest {
    @Rule
    public TestName mTestName = new TestName();

    @Test
    public void getLabel_shouldMatchTheConstructor() {
        CrateInfo crateInfo = new CrateInfo(mTestName.getMethodName());

        assertThat(crateInfo.getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void newCrateInfo_withNormalLabel_shouldBeSuccess() {
        CrateInfo crateInfo = new CrateInfo(mTestName.getMethodName());

        assertThat(crateInfo.getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void newCrateInfo_withNormalLabelAndExpiration_shouldBeSuccess() {
        CrateInfo crateInfo = new CrateInfo(mTestName.getMethodName(), 0);

        assertThat(crateInfo.getLabel()).isEqualTo(mTestName.getMethodName());
    }

    @Test
    public void newCrateInfo_withNormalLabelAndMaxExpiration_shouldBeSuccess() {
        CrateInfo crateInfo = new CrateInfo(mTestName.getMethodName(), Long.MAX_VALUE);

        assertThat(crateInfo.getExpirationMillis()).isEqualTo(Long.MAX_VALUE);
    }

    @Test
    public void newCrateInfo_nullLabel_throwIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;
        try {
            new CrateInfo(null);
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void newCrateInfo_emptyString_throwIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;
        try {
            new CrateInfo("");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void newCrateInfo_withNegativeExpiration_throwIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;
        try {
            new CrateInfo(mTestName.getMethodName(), -1);
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void readFromParcel_shouldMatchFromWriteToParcel() {
        Parcel parcel = Parcel.obtain();
        CrateInfo crateInfo = new CrateInfo(mTestName.getMethodName(), Long.MAX_VALUE);
        crateInfo.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);

        CrateInfo readFromCrateInfo = CrateInfo.CREATOR.createFromParcel(parcel);
        parcel.recycle();

        assertThat(readFromCrateInfo.getLabel()).isEqualTo(mTestName.getMethodName());
        assertThat(readFromCrateInfo.getExpirationMillis()).isEqualTo(Long.MAX_VALUE);
    }

    @Test
    public void copyFrom_validatedCrate_shouldReturnNonNull() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        CrateInfo crateInfo = CrateInfo.copyFrom(Process.myUid(), context.getOpPackageName(),
                mTestName.getMethodName());

        assertThat(crateInfo).isNotNull();
    }

    @Test
    public void copyFrom_invalidCrate_shouldReturnNull() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        CrateInfo crateInfo = CrateInfo.copyFrom(Process.myUid(), context.getOpPackageName(),
                null);

        assertThat(crateInfo).isNull();
    }

    @Test
    public void copyFrom_invalidPackageName_shouldReturnNull() {
        CrateInfo crateInfo = CrateInfo.copyFrom(Process.myUid(), null,
                mTestName.getMethodName());

        assertThat(crateInfo).isNull();
    }
}
