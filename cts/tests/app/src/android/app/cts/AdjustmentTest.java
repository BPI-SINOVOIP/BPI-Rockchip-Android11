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

package android.app.cts;


import android.os.Bundle;
import android.os.Parcel;
import android.os.UserHandle;
import android.service.notification.Adjustment;
import android.test.AndroidTestCase;
import android.util.Log;

public class AdjustmentTest extends AndroidTestCase {
    private static final String ADJ_PACKAGE = "com.foo.bar";
    private static final String ADJ_KEY = "foo_key";
    private static final String ADJ_EXPLANATION = "I just feel like adjusting this";
    private static final UserHandle ADJ_USER = UserHandle.CURRENT;

    private final Bundle mSignals = new Bundle();

    private Adjustment mAdjustment;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mSignals.putString("foobar", "Hello, world!");
        mSignals.putInt("chirp", 47);
        mAdjustment = new Adjustment(ADJ_PACKAGE, ADJ_KEY, mSignals, ADJ_EXPLANATION, ADJ_USER);
    }

    public void testGetPackage() {
        assertEquals(ADJ_PACKAGE, mAdjustment.getPackage());
    }

    public void testGetKey() {
        assertEquals(ADJ_KEY, mAdjustment.getKey());
    }

    public void testGetExplanation() {
        assertEquals(ADJ_EXPLANATION, mAdjustment.getExplanation());
    }

    public void testGetUser() {
        assertEquals(ADJ_USER, mAdjustment.getUserHandle());
    }

    public void testGetSignals() {
        assertEquals(mSignals, mAdjustment.getSignals());
        assertEquals("Hello, world!", mAdjustment.getSignals().getString("foobar"));
        assertEquals(47, mAdjustment.getSignals().getInt("chirp"));
    }

    public void testDescribeContents() {
        assertEquals(0, mAdjustment.describeContents());
    }

    public void testParcelling() {
        final Parcel outParcel = Parcel.obtain();
        mAdjustment.writeToParcel(outParcel, 0);
        outParcel.setDataPosition(0);
        final Adjustment unparceled = Adjustment.CREATOR.createFromParcel(outParcel);

        assertEquals(mAdjustment.getPackage(), unparceled.getPackage());
        assertEquals(mAdjustment.getKey(), unparceled.getKey());
        assertEquals(mAdjustment.getExplanation(), unparceled.getExplanation());
        assertEquals(mAdjustment.getUserHandle(), unparceled.getUserHandle());

        assertEquals("Hello, world!", unparceled.getSignals().getString("foobar"));
        assertEquals(47, unparceled.getSignals().getInt("chirp"));
    }
}
