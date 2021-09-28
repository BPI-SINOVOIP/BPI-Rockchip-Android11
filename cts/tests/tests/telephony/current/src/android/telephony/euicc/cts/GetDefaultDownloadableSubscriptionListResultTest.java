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

package android.telephony.euicc.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Parcel;
import android.os.Parcelable;
import android.service.euicc.EuiccService;
import android.service.euicc.GetDefaultDownloadableSubscriptionListResult;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class GetDefaultDownloadableSubscriptionListResultTest {

    private GetDefaultDownloadableSubscriptionListResult mDefaultSubListResult;

    @Before
    public void setUp() {
        mDefaultSubListResult =
                new GetDefaultDownloadableSubscriptionListResult(
                        EuiccService.RESULT_RESOLVABLE_ERRORS, null /*subscriptions*/);
    }

    @Test
    public void testGetResult() {
        assertNotNull(mDefaultSubListResult);
        assertEquals(EuiccService.RESULT_RESOLVABLE_ERRORS, mDefaultSubListResult.getResult());
    }

    @Test
    public void testGetDownloadableSubscriptions() {
        assertNotNull(mDefaultSubListResult);
        assertNull(mDefaultSubListResult.getDownloadableSubscriptions());
    }

    @Test
    public void testDescribeContents() {
        assertNotNull(mDefaultSubListResult);
        int bitmask = mDefaultSubListResult.describeContents();
        assertTrue(bitmask == 0 || bitmask == Parcelable.CONTENTS_FILE_DESCRIPTOR);
    }

    @Test
    public void testWriteToParcel() {
        assertNotNull(mDefaultSubListResult);

        Parcel parcel = Parcel.obtain();
        assertTrue(parcel != null);
        mDefaultSubListResult.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);
        GetDefaultDownloadableSubscriptionListResult fromParcel =
                GetDefaultDownloadableSubscriptionListResult.CREATOR.createFromParcel(parcel);

        assertEquals(EuiccService.RESULT_RESOLVABLE_ERRORS, fromParcel.getResult());
        assertNull(fromParcel.getDownloadableSubscriptions());
    }
}
