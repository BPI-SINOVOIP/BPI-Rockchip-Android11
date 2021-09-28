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

import static android.quickaccesswallet.cts.TestUtils.compareIcons;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.graphics.drawable.Icon;
import android.os.Parcel;
import android.service.quickaccesswallet.GetWalletCardsError;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests {@link GetWalletCardsError}
 */
@RunWith(AndroidJUnit4.class)
public class GetWalletCardsErrorTest {

    private Context mContext;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Test
    public void testParcel_toParcel() {
        Icon icon = Icon.createWithResource(mContext, android.R.drawable.ic_dialog_info);
        GetWalletCardsError error = new GetWalletCardsError(icon, "error");

        Parcel p = Parcel.obtain();
        error.writeToParcel(p, 0);
        p.setDataPosition(0);
        GetWalletCardsError newError = GetWalletCardsError.CREATOR.createFromParcel(p);
        compareIcons(mContext, error.getIcon(), newError.getIcon());
        assertThat(error.getMessage()).isEqualTo(newError.getMessage());
    }

    @Test
    public void testParcel_withNullIcon_toParcel() {
        GetWalletCardsError error = new GetWalletCardsError(null, "error");

        Parcel p = Parcel.obtain();
        error.writeToParcel(p, 0);
        p.setDataPosition(0);
        GetWalletCardsError newError = GetWalletCardsError.CREATOR.createFromParcel(p);
        compareIcons(mContext, error.getIcon(), newError.getIcon());
        assertThat(error.getMessage()).isEqualTo(newError.getMessage());
    }

    @Test
    public void testParcel_withNullIconAndMessage_toParcel() {
        GetWalletCardsError error = new GetWalletCardsError(null, null);

        Parcel p = Parcel.obtain();
        error.writeToParcel(p, 0);
        p.setDataPosition(0);
        GetWalletCardsError newError = GetWalletCardsError.CREATOR.createFromParcel(p);
        compareIcons(mContext, error.getIcon(), newError.getIcon());
        assertThat(error.getMessage()).isEqualTo(newError.getMessage());
    }
}
