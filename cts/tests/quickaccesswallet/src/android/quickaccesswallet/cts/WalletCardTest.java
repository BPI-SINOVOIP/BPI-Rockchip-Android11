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

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Parcel;
import android.quickaccesswallet.QuickAccessWalletActivity;
import android.service.quickaccesswallet.WalletCard;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests parceling of the {@link WalletCard}
 */
@RunWith(AndroidJUnit4.class)
public class WalletCardTest {

    private Context mContext;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /**
     * This inserts the key value pair and assert they can be retrieved. Then it clears the
     * preferences and asserts they can no longer be retrieved.
     */
    @Test
    public void testParcel_toParcel() {
        Bitmap bitmap = Bitmap.createBitmap(70, 44, Bitmap.Config.ARGB_8888);
        Intent intent = new Intent(mContext, QuickAccessWalletActivity.class);
        WalletCard card = new WalletCard.Builder(
                "cardId",
                Icon.createWithBitmap(bitmap),
                "content description",
                PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE))
                .setCardIcon(Icon.createWithResource(mContext, android.R.drawable.ic_dialog_info))
                .setCardLabel("card label")
                .build();

        Parcel p = Parcel.obtain();
        card.writeToParcel(p, 0);
        p.setDataPosition(0);
        WalletCard newCard = WalletCard.CREATOR.createFromParcel(p);
        assertThat(card.getCardId()).isEqualTo(newCard.getCardId());
        compareIcons(mContext, card.getCardImage(), newCard.getCardImage());
        assertThat(card.getContentDescription()).isEqualTo(newCard.getContentDescription());
        assertThat(card.getPendingIntent()).isEqualTo(newCard.getPendingIntent());
        compareIcons(mContext, card.getCardIcon(), newCard.getCardIcon());
        assertThat(card.getCardLabel()).isEqualTo(newCard.getCardLabel());
    }

    @Test
    public void testParcel_noIconOrLabel_toParcel() {
        Bitmap bitmap = Bitmap.createBitmap(70, 44, Bitmap.Config.ARGB_8888);
        Intent intent = new Intent(mContext, QuickAccessWalletActivity.class);
        WalletCard card = new WalletCard.Builder(
                "cardId",
                Icon.createWithBitmap(bitmap),
                "content description",
                PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE))
                .build();

        Parcel p = Parcel.obtain();
        card.writeToParcel(p, 0);
        p.setDataPosition(0);
        WalletCard newCard = WalletCard.CREATOR.createFromParcel(p);
        assertThat(card.getCardId()).isEqualTo(newCard.getCardId());
        compareIcons(mContext, card.getCardImage(), newCard.getCardImage());
        assertThat(card.getContentDescription()).isEqualTo(newCard.getContentDescription());
        assertThat(card.getPendingIntent()).isEqualTo(newCard.getPendingIntent());
        compareIcons(mContext, card.getCardIcon(), card.getCardIcon());
        assertThat(card.getCardLabel()).isEqualTo(newCard.getCardLabel());
    }
}
