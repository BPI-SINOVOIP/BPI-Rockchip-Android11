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
package android.quickaccesswallet.cts;

import static com.google.common.truth.Truth.assertThat;

import android.os.Parcel;
import android.service.quickaccesswallet.SelectWalletCardRequest;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests {@link SelectWalletCardRequest}
 */
@RunWith(AndroidJUnit4.class)
public class SelectWalletCardRequestTest {

    @Test
    public void testParcel_toParcel() {
        SelectWalletCardRequest request = new SelectWalletCardRequest("card1");

        Parcel p = Parcel.obtain();
        request.writeToParcel(p, 0);
        p.setDataPosition(0);
        SelectWalletCardRequest newRequest = SelectWalletCardRequest.CREATOR.createFromParcel(p);
        assertThat(request.getCardId()).isEqualTo(newRequest.getCardId());
    }
}
