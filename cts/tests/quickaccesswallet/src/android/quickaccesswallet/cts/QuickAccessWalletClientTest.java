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
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.provider.Settings;
import android.quickaccesswallet.NoPermissionQuickAccessWalletService;
import android.quickaccesswallet.QuickAccessWalletActivity;
import android.quickaccesswallet.QuickAccessWalletSettingsActivity;
import android.quickaccesswallet.TestHostApduService;
import android.quickaccesswallet.TestQuickAccessWalletService;
import android.service.quickaccesswallet.GetWalletCardsError;
import android.service.quickaccesswallet.GetWalletCardsRequest;
import android.service.quickaccesswallet.GetWalletCardsResponse;
import android.service.quickaccesswallet.QuickAccessWalletClient;
import android.service.quickaccesswallet.QuickAccessWalletClient.WalletServiceEventListener;
import android.service.quickaccesswallet.QuickAccessWalletService;
import android.service.quickaccesswallet.SelectWalletCardRequest;
import android.service.quickaccesswallet.WalletCard;
import android.service.quickaccesswallet.WalletServiceEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.SettingsUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;


/**
 * Tests parceling of the {@link WalletCard}
 */
@RunWith(AndroidJUnit4.class)
public class QuickAccessWalletClientTest {

    private static final GetWalletCardsRequest GET_WALLET_CARDS_REQUEST =
            new GetWalletCardsRequest(700, 440, 64, 5);

    private static final String SETTING_DISABLED = "0";
    private static final String SETTING_ENABLED = "1";
    private Context mContext;
    private String mDefaultPaymentApp;

    @Before
    public void setUp() throws Exception {
        // Save current default payment app
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mDefaultPaymentApp = SettingsUtils.get(Settings.Secure.NFC_PAYMENT_DEFAULT_COMPONENT);
        ComponentName component =
                ComponentName.createRelative(mContext, TestHostApduService.class.getName());
        SettingsUtils.syncSet(mContext, Settings.Secure.NFC_PAYMENT_DEFAULT_COMPONENT,
                component.flattenToString());
        TestQuickAccessWalletService.resetStaticFields();
    }

    @After
    public void tearDown() {
        // Restore saved default payment app
        ContentResolver cr = mContext.getContentResolver();
        SettingsUtils.syncSet(mContext, Settings.Secure.NFC_PAYMENT_DEFAULT_COMPONENT,
                mDefaultPaymentApp);

        // Return all services to default state
        setServiceState(TestQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);
        setServiceState(NoPermissionQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT);
        TestQuickAccessWalletService.resetStaticFields();
    }

    @Test
    public void testIsWalletServiceAvailable_returnsFalseIfNoServiceAvailable() {
        setServiceState(TestQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);

        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        assertThat(client.isWalletServiceAvailable()).isFalse();
    }

    @Test
    public void testIsWalletFeatureAvailableWhenDeviceLocked_checksSecureSettings() {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        String showCardsAndPasses = SettingsUtils.getSecureSetting(
                Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT);

        try {
            SettingsUtils.syncSet(mContext, Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT,
                    SETTING_ENABLED);
            assertThat(client.isWalletFeatureAvailableWhenDeviceLocked()).isTrue();

            SettingsUtils.syncSet(mContext, Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT,
                    SETTING_DISABLED);
            assertThat(client.isWalletFeatureAvailableWhenDeviceLocked()).isFalse();
        } finally {
            // return setting to original value
            SettingsUtils.syncSet(mContext, Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT,
                    showCardsAndPasses);
        }
    }

    @Test
    public void testGetWalletCards_success() throws Exception {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        assertThat(client.isWalletServiceAvailable()).isTrue();
        TestCallback callback = new TestCallback();

        client.getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mResponse).isNotNull();
        assertThat(callback.mResponse.getWalletCards()).hasSize(1);
        assertThat(callback.mError).isNull();
    }

    @Test
    public void testGetWalletCards_failsIfNoServiceAvailable() throws Exception {
        setServiceState(TestQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        assertThat(client.isWalletServiceAvailable()).isFalse();
        TestCallback callback = new TestCallback();

        client.getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mResponse).isNull();
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_failsIfServiceDoesNotRequirePermission() throws Exception {
        setServiceState(NoPermissionQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
        setServiceState(TestQuickAccessWalletService.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        assertThat(client.isWalletServiceAvailable()).isFalse();
        TestCallback callback = new TestCallback();

        client.getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mResponse).isNull();
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidCard_nullId_fails() throws Exception {
        WalletCard card = new WalletCard.Builder(null, createCardImage(), "Card",
                createPendingIntent()).build();
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(Collections.singletonList(card), 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidCard_nullImage_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", null, "Card",
                createPendingIntent()).build();
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(Collections.singletonList(card), 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidCard_nullContentDesc_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", createCardImage(), null,
                createPendingIntent()).build();
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(Collections.singletonList(card), 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);
        callback.await(3, TimeUnit.SECONDS);

        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidCard_nullPendingIntent_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", createCardImage(), "card", null).build();
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(Collections.singletonList(card), 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidResponse_tooManyCards_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", createCardImage(), "card",
                createPendingIntent()).build();
        List<WalletCard> walletCards = Arrays.asList(card, card, card, card, card, card, card);
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(walletCards, 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_zeroCards_isAllowed() throws Exception {
        List<WalletCard> walletCards = Collections.emptyList();
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(walletCards, 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mResponse).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidResponse_nullCards_fails() throws Exception {
        TestQuickAccessWalletService.setWalletCardsResponse(new GetWalletCardsResponse(null, 0));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidResponse_negativeIndex_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", createCardImage(), "card",
                createPendingIntent()).build();
        List<WalletCard> walletCards = Arrays.asList(card, card, card);
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(walletCards, -1));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);

        callback.await(3, TimeUnit.SECONDS);
        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_invalidResponse_indexOutOfBounds_fails() throws Exception {
        WalletCard card = new WalletCard.Builder("cardId", createCardImage(), "card",
                createPendingIntent()).build();
        List<WalletCard> walletCards = Arrays.asList(card, card, card);
        TestQuickAccessWalletService.setWalletCardsResponse(
                new GetWalletCardsResponse(walletCards, 3));
        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);
        callback.await(3, TimeUnit.SECONDS);

        assertThat(callback.mError).isNotNull();
    }

    @Test
    public void testGetWalletCards_serviceResponseWithError_success() throws Exception {
        GetWalletCardsError error = new GetWalletCardsError(null, "error");
        TestQuickAccessWalletService.setWalletCardsError(error);

        TestCallback callback = new TestCallback();

        QuickAccessWalletClient.create(mContext).getWalletCards(GET_WALLET_CARDS_REQUEST, callback);
        callback.await(3, TimeUnit.SECONDS);

        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError.getMessage()).isEqualTo(error.getMessage());
    }

    @Test
    public void testSelectWalletCard_success() throws Exception {
        TestQuickAccessWalletService.setExpectedRequestCount(1);
        QuickAccessWalletClient.create(mContext).selectWalletCard(
                new SelectWalletCardRequest("card1"));
        TestQuickAccessWalletService.awaitRequests(3, TimeUnit.SECONDS);

        List<SelectWalletCardRequest> selectRequests =
                TestQuickAccessWalletService.getSelectRequests();

        assertThat(selectRequests).hasSize(1);
        assertThat(selectRequests.get(0).getCardId()).isEqualTo("card1");
    }

    @Test
    public void testNotifyWalletDismissed_success() throws Exception {
        assertThat(TestQuickAccessWalletService.isWalletDismissed()).isFalse();
        TestQuickAccessWalletService.setExpectedRequestCount(1);

        QuickAccessWalletClient.create(mContext).notifyWalletDismissed();
        TestQuickAccessWalletService.awaitRequests(3, TimeUnit.SECONDS);

        assertThat(TestQuickAccessWalletService.isWalletDismissed()).isTrue();
    }

    @Test
    public void testAddListener_sendEvent_success() throws Exception {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        TestEventListener listener = new TestEventListener();
        TestQuickAccessWalletService.setExpectedRequestCount(1);
        client.addWalletServiceEventListener(listener);
        TestQuickAccessWalletService.awaitRequests(1, TimeUnit.SECONDS);

        TestQuickAccessWalletService.sendEvent(
                new WalletServiceEvent(WalletServiceEvent.TYPE_NFC_PAYMENT_STARTED));
        listener.await(300, TimeUnit.SECONDS);

        assertThat(listener.mEvents).hasSize(1);
    }

    @Test
    public void testRemoveListener_sendEvent_shouldNotBeDelivered() {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        TestEventListener listener = new TestEventListener();
        client.addWalletServiceEventListener(listener);
        client.removeWalletServiceEventListener(listener);

        TestQuickAccessWalletService.sendEvent(
                new WalletServiceEvent(WalletServiceEvent.TYPE_NFC_PAYMENT_STARTED));
        try {
            listener.await(250, TimeUnit.MILLISECONDS);
        } catch (InterruptedException ignored) {
            // It is expected that this time out
        }

        assertThat(listener.mEvents).hasSize(0);
    }

    @Test
    public void testDisconnect_shouldClearListenersAndDisconnect() throws Exception {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);
        TestQuickAccessWalletService.setExpectedBindCount(1);
        TestQuickAccessWalletService.setExpectedUnbindCount(1);

        client.getWalletCards(GET_WALLET_CARDS_REQUEST, new TestCallback());

        TestQuickAccessWalletService.awaitBinding(3, TimeUnit.SECONDS);
        client.disconnect();
        TestQuickAccessWalletService.awaitUnbinding(3, TimeUnit.SECONDS);
    }

    @Test
    public void testConnect_disconnect_reconnect_shouldWork() throws Exception {
        QuickAccessWalletClient client = QuickAccessWalletClient.create(mContext);

        TestCallback callback = new TestCallback();
        client.getWalletCards(GET_WALLET_CARDS_REQUEST, callback);
        callback.await(3, TimeUnit.SECONDS);

        TestQuickAccessWalletService.setExpectedUnbindCount(1);
        client.disconnect();
        TestQuickAccessWalletService.awaitUnbinding(3, TimeUnit.SECONDS);

        TestCallback callback2 = new TestCallback();
        client.getWalletCards(GET_WALLET_CARDS_REQUEST, callback2);
        callback2.await(3, TimeUnit.SECONDS);

        assertThat(callback.mResponse).isNotNull();
        assertThat(callback2.mResponse).isNotNull();
    }

    @Test
    public void testCreateWalletIntent_parsesXmlAndUsesCorrectIntentAction() {
        Intent walletIntent = QuickAccessWalletClient.create(mContext).createWalletIntent();
        assertThat(walletIntent).isNotNull();
        assertThat(walletIntent.getAction()).isEqualTo(QuickAccessWalletService.ACTION_VIEW_WALLET);
        ComponentName expectedComponent = ComponentName.createRelative(mContext,
                QuickAccessWalletActivity.class.getName());
        assertThat(walletIntent.getComponent()).isEqualTo(expectedComponent);
    }

    @Test
    public void testCreateWalletSettingsIntent_usesSettingsActionToFindAppropriateActivity() {
        Intent settingsIntent =
                QuickAccessWalletClient.create(mContext).createWalletSettingsIntent();
        assertThat(settingsIntent).isNotNull();
        assertThat(settingsIntent.getAction()).isEqualTo(
                QuickAccessWalletService.ACTION_VIEW_WALLET_SETTINGS);
        ComponentName expectedComponent = ComponentName.createRelative(mContext,
                QuickAccessWalletSettingsActivity.class.getName());
        assertThat(settingsIntent.getComponent()).isEqualTo(expectedComponent);
    }

    private void setServiceState(
            Class<? extends QuickAccessWalletService> cls, int state) {
        ComponentName componentName = ComponentName.createRelative(mContext, cls.getName());
        mContext.getPackageManager().setComponentEnabledSetting(
                componentName, state, PackageManager.DONT_KILL_APP);
    }

    private Icon createCardImage() {
        Bitmap bitmap = Bitmap.createBitmap(
                GET_WALLET_CARDS_REQUEST.getCardWidthPx(),
                GET_WALLET_CARDS_REQUEST.getCardHeightPx(),
                Bitmap.Config.ARGB_8888);
        return Icon.createWithBitmap(bitmap.copy(Bitmap.Config.HARDWARE, false));
    }

    private PendingIntent createPendingIntent() {
        Intent intent = new Intent(mContext, QuickAccessWalletActivity.class);
        return PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE);
    }

    private static class TestCallback implements
            QuickAccessWalletClient.OnWalletCardsRetrievedCallback {

        private final CountDownLatch mLatch = new CountDownLatch(1);
        private GetWalletCardsResponse mResponse;
        private GetWalletCardsError mError;

        @Override
        public void onWalletCardsRetrieved(GetWalletCardsResponse response) {
            mResponse = response;
            mLatch.countDown();
        }

        @Override
        public void onWalletCardRetrievalError(GetWalletCardsError error) {
            mError = error;
            mLatch.countDown();
        }

        public void await(int time, TimeUnit unit) throws InterruptedException {
            mLatch.await(time, unit);
        }
    }

    private static class TestEventListener implements WalletServiceEventListener {

        private final List<WalletServiceEvent> mEvents = new ArrayList<>();
        private final CountDownLatch mLatch = new CountDownLatch(1);


        @Override
        public void onWalletServiceEvent(WalletServiceEvent event) {
            mEvents.add(event);
            mLatch.countDown();
        }

        public void await(int time, TimeUnit unit) throws InterruptedException {
            mLatch.await(time, unit);
        }
    }
}
