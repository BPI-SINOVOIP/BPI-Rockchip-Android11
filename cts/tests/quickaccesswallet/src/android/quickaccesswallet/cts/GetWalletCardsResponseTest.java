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

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Icon;
import android.os.Parcel;
import android.quickaccesswallet.QuickAccessWalletActivity;
import android.service.quickaccesswallet.GetWalletCardsResponse;
import android.service.quickaccesswallet.WalletCard;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

/**
 * Tests {@link GetWalletCardsResponse}
 */
@RunWith(AndroidJUnit4.class)
public class GetWalletCardsResponseTest {

    private static final int CARD_WIDTH = 70;
    private static final int CARD_HEIGHT = 44;

    private Context mContext;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Test
    public void testParcel_toParcel() {
        Intent intent = new Intent(mContext, QuickAccessWalletActivity.class);
        WalletCard card1 = new WalletCard.Builder(
                "card1",
                Icon.createWithBitmap(createBitmap("1")),
                "content description",
                PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE))
                .build();

        WalletCard card2 = new WalletCard.Builder(
                "card2",
                Icon.createWithBitmap(createBitmap("2")),
                "content description",
                PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE))
                .build();

        GetWalletCardsResponse response =
                new GetWalletCardsResponse(Arrays.asList(card1, card2), 1);
        Parcel p = Parcel.obtain();
        response.writeToParcel(p, 0);
        p.setDataPosition(0);
        GetWalletCardsResponse newResponse = GetWalletCardsResponse.CREATOR.createFromParcel(p);
        assertThat(newResponse.getWalletCards()).hasSize(2);
        assertThat(newResponse.getWalletCards().get(0).getCardId()).isEqualTo("card1");
        TestUtils.compareIcons(mContext,
                response.getWalletCards().get(0).getCardImage(),
                newResponse.getWalletCards().get(0).getCardImage());
        TestUtils.compareIcons(mContext,
                response.getWalletCards().get(1).getCardImage(),
                newResponse.getWalletCards().get(1).getCardImage());
        assertThat(newResponse.getWalletCards().get(1).getCardId()).isEqualTo("card2");
        assertThat(newResponse.getSelectedIndex()).isEqualTo(1);
    }

    private Bitmap createBitmap(String cardNumber) {
        Bitmap bitmap = Bitmap.createBitmap(CARD_WIDTH, CARD_HEIGHT, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.WHITE);
        Paint paint = new Paint();
        paint.setColor(Color.BLACK);
        canvas.drawText(cardNumber, 0, 0, paint);
        return bitmap;
    }
}
