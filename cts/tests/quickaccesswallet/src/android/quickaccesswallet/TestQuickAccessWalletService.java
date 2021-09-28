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

package android.quickaccesswallet;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.service.quickaccesswallet.GetWalletCardsCallback;
import android.service.quickaccesswallet.GetWalletCardsError;
import android.service.quickaccesswallet.GetWalletCardsRequest;
import android.service.quickaccesswallet.GetWalletCardsResponse;
import android.service.quickaccesswallet.QuickAccessWalletService;
import android.service.quickaccesswallet.SelectWalletCardRequest;
import android.service.quickaccesswallet.WalletCard;
import android.service.quickaccesswallet.WalletServiceEvent;
import android.util.Log;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TestQuickAccessWalletService extends QuickAccessWalletService {

    private static final String TAG = "TestQAWalletSvc";
    private static GetWalletCardsResponse sWalletCardsResponse;
    private static GetWalletCardsError sWalletCardsError;
    private static List<SelectWalletCardRequest> sSelectWalletCardRequests = new ArrayList<>();
    private static boolean sWalletDismissed;
    private static WeakReference<TestQuickAccessWalletService> sServiceRef =
            new WeakReference<>(null);
    private static CountDownLatch sRequestCountDownLatch = new CountDownLatch(0);
    private static CountDownLatch sBindCounter = new CountDownLatch(0);
    private static CountDownLatch sUnbindCounter = new CountDownLatch(0);

    public static void resetStaticFields() {
        sWalletCardsResponse = null;
        sWalletCardsError = null;
        sSelectWalletCardRequests = new ArrayList<>();
        sWalletDismissed = false;
        sRequestCountDownLatch = new CountDownLatch(0);
        sBindCounter = new CountDownLatch(0);
        sUnbindCounter = new CountDownLatch(0);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        sServiceRef = new WeakReference<>(this);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        sUnbindCounter.countDown();
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        sServiceRef.clear();
        super.onDestroy();
    }

    @Override
    public void onWalletCardsRequested(
            GetWalletCardsRequest request,
            GetWalletCardsCallback callback) {
        Log.i(TAG, "onWalletCardsRequested");
        GetWalletCardsError error = sWalletCardsError;
        if (error != null) {
            callback.onFailure(error);
            return;
        }
        GetWalletCardsResponse response = sWalletCardsResponse;
        if (response == null) {
            Bitmap bitmap = Bitmap.createBitmap(
                    request.getCardWidthPx(), request.getCardHeightPx(), Bitmap.Config.ARGB_8888);
            Icon cardImage = Icon.createWithBitmap(bitmap.copy(Bitmap.Config.HARDWARE, false));
            Intent intent = new Intent(this, QuickAccessWalletActivity.class);
            PendingIntent pendingIntent =
                    PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE);
            WalletCard walletCard = new WalletCard.Builder("card1", cardImage, "Card 1",
                    pendingIntent).build();
            List<WalletCard> walletCards = Collections.singletonList(walletCard);
            response = new GetWalletCardsResponse(walletCards, 0);
        }
        callback.onSuccess(response);
    }

    @Override
    public void onWalletCardSelected(SelectWalletCardRequest request) {
        Log.i(TAG, "onWalletCardSelected");
        sSelectWalletCardRequests.add(request);
        sRequestCountDownLatch.countDown();
    }

    @Override
    public void onWalletDismissed() {
        Log.i(TAG, "onWalletDismissed");
        sWalletDismissed = true;
        sRequestCountDownLatch.countDown();
    }

    public static void sendEvent(WalletServiceEvent event) {
        TestQuickAccessWalletService service = sServiceRef.get();
        if (service != null) {
            service.sendWalletServiceEvent(event);
        }
    }

    public static List<SelectWalletCardRequest> getSelectRequests() {
        return new ArrayList<>(sSelectWalletCardRequests);
    }

    public static void setWalletCardsResponse(GetWalletCardsResponse response) {
        sWalletCardsResponse = response;
    }

    public static void setWalletCardsError(GetWalletCardsError error) {
        sWalletCardsError = error;
    }

    public static boolean isWalletDismissed() {
        return sWalletDismissed;
    }

    public static void setExpectedRequestCount(int countdown) {
        sRequestCountDownLatch = new CountDownLatch(countdown);
    }

    public static void awaitRequests(long timeout, TimeUnit timeUnit) throws InterruptedException {
        sRequestCountDownLatch.await(timeout, timeUnit);
    }

    public static void setExpectedBindCount(int count) {
        sBindCounter = new CountDownLatch(count);
    }

    public static void awaitBinding(long timeout, TimeUnit unit) throws InterruptedException {
        sBindCounter.await(timeout, unit);
    }

    public static void setExpectedUnbindCount(int count) {
        sUnbindCounter = new CountDownLatch(count);
    }

    public static void awaitUnbinding(long timeout, TimeUnit unit) throws InterruptedException {
        sUnbindCounter.await(timeout, unit);
    }
}
