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

package com.android.systemui.plugin.globalactions.wallet;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.app.Application;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.service.quickaccesswallet.GetWalletCardsError;
import android.service.quickaccesswallet.GetWalletCardsRequest;
import android.service.quickaccesswallet.GetWalletCardsResponse;
import android.service.quickaccesswallet.QuickAccessWalletClient;
import android.service.quickaccesswallet.QuickAccessWalletClient.WalletServiceEventListener;
import android.service.quickaccesswallet.QuickAccessWalletService;
import android.service.quickaccesswallet.SelectWalletCardRequest;
import android.service.quickaccesswallet.WalletCard;
import android.service.quickaccesswallet.WalletServiceEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import com.android.systemui.plugins.GlobalActionsPanelPlugin;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;


@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
public class WalletPanelViewControllerTest {

    private static final int MAX_CARDS = 10;
    private static final CharSequence SHORTCUT_SHORT_LABEL = "View all";
    private static final CharSequence SHORTCUT_LONG_LABEL = "Add a payment method";
    private static final CharSequence SERVICE_LABEL = "Wallet app";
    private final Application mContext = ApplicationProvider.getApplicationContext();
    private final Drawable mWalletLogo = mContext.getDrawable(android.R.drawable.ic_lock_lock);
    private final Intent mWalletIntent = new Intent(QuickAccessWalletService.ACTION_VIEW_WALLET)
            .setComponent(new ComponentName(mContext.getPackageName(), "WalletActivity"));

    @Mock
    QuickAccessWalletClient mWalletClient;
    @Mock
    GlobalActionsPanelPlugin.Callbacks mPluginCallbacks;
    @Captor
    ArgumentCaptor<GetWalletCardsRequest> mRequestCaptor;
    @Captor
    ArgumentCaptor<QuickAccessWalletClient.OnWalletCardsRetrievedCallback> mCallbackCaptor;
    @Captor
    ArgumentCaptor<SelectWalletCardRequest> mSelectCardRequestCaptor;
    @Captor
    ArgumentCaptor<WalletServiceEventListener> mListenerCaptor;
    private WalletPanelViewController mViewController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mViewController = new WalletPanelViewController(
                mContext, mContext, mWalletClient, mPluginCallbacks, false);
        doAnswer(invocation -> {
            PendingIntent pendingIntent = invocation.getArgument(0);
            pendingIntent.send();
            return null;
        }).when(mPluginCallbacks).startPendingIntentDismissingKeyguard(any());
        when(mWalletClient.getLogo()).thenReturn(mWalletLogo);
        when(mWalletClient.getShortcutLongLabel()).thenReturn(SHORTCUT_LONG_LABEL);
        when(mWalletClient.getShortcutShortLabel()).thenReturn(SHORTCUT_SHORT_LABEL);
        when(mWalletClient.getServiceLabel()).thenReturn(SERVICE_LABEL);
        when(mWalletClient.createWalletIntent()).thenReturn(mWalletIntent);
        ShadowLog.stream = System.out;
    }

    /**
     * getPanelContent may be called multiple times even when shown once to user. It should always
     * return the same view and not re-request card.
     */
    @Test
    public void getPanelContent_returnsSameView() {
        View view1 = mViewController.getPanelContent();
        View view2 = mViewController.getPanelContent();

        assertThat(view1).isSameAs(view2);
        verify(mWalletClient, never()).getWalletCards(any(), any(), any());
    }

    @Test
    public void init_showsEmptyStateViewIfCardsNotReturnedLastTime() {
        WalletView walletView = (WalletView) mViewController.getPanelContent();
        GetWalletCardsResponse response = new GetWalletCardsResponse(Collections.emptyList(), 0);
        mViewController.onWalletCardsRetrieved(response);
        assertThat(walletView.getEmptyStateView().getVisibility()).isEqualTo(VISIBLE);
        assertThat(walletView.getCardCarouselContainer().getVisibility()).isEqualTo(GONE);
        mViewController.onDismissed();

        // When re-instantiated, should show empty state view
        mViewController = new WalletPanelViewController(
                mContext, mContext, mWalletClient, mPluginCallbacks, false);
        walletView = (WalletView) mViewController.getPanelContent();
        assertThat(walletView.getEmptyStateView().getVisibility()).isEqualTo(VISIBLE);
        assertThat(walletView.getCardCarouselContainer().getVisibility()).isEqualTo(GONE);
    }

    @Test
    public void init_doesNotShowEmptyStateViewIfCardsReturnedLastTime() {
        WalletView walletView = (WalletView) mViewController.getPanelContent();
        List<WalletCard> cards = Collections.singletonList(createWalletCard("c1"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);
        mViewController.onWalletCardsRetrieved(response);
        assertThat(walletView.getEmptyStateView().getVisibility()).isEqualTo(GONE);
        assertThat(walletView.getCardCarouselContainer().getVisibility()).isEqualTo(VISIBLE);
        mViewController.onDismissed();

        // When re-instantiated, should show empty state view
        mViewController = new WalletPanelViewController(
                mContext, mContext, mWalletClient, mPluginCallbacks, false);
        walletView = (WalletView) mViewController.getPanelContent();
        assertThat(walletView.getEmptyStateView().getVisibility()).isEqualTo(GONE);
        assertThat(walletView.getCardCarouselContainer().getVisibility()).isEqualTo(GONE);
    }

    @Test
    public void onDismissed_notifiesClientAndRemotesListener() {
        mViewController.queryWalletCards();
        verify(mWalletClient).addWalletServiceEventListener(mListenerCaptor.capture());
        WalletServiceEventListener serviceEventListener = mListenerCaptor.getValue();

        mViewController.onDismissed();

        verify(mWalletClient).notifyWalletDismissed();
        verify(mWalletClient).removeWalletServiceEventListener(serviceEventListener);
    }

    @Test
    public void onDismissed_doesNotTriggerTwice() {
        mViewController.queryWalletCards();

        mViewController.onDismissed();
        mViewController.onDismissed();

        verify(mWalletClient, times(1)).notifyWalletDismissed();
        verify(mWalletClient, times(1)).removeWalletServiceEventListener(any());
    }

    @Test
    public void onDeviceLockStateChanged_unlocked_queriesCards() {
        mViewController =
                new WalletPanelViewController(mContext, mContext, mWalletClient, mPluginCallbacks,
                        true);
        when(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).thenReturn(false);
        mViewController.queryWalletCards();
        verify(mWalletClient, never()).getWalletCards(any(), any(), any());

        mViewController.onDeviceLockStateChanged(false);

        verify(mWalletClient).getWalletCards(any(), any(), any());
    }

    @Test
    public void onDeviceLockStateChanged_calledTwice_onlyQueriesCardsOnce() {
        mViewController =
                new WalletPanelViewController(mContext, mContext, mWalletClient, mPluginCallbacks,
                        true);
        when(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).thenReturn(false);
        mViewController.queryWalletCards();
        verify(mWalletClient, never()).getWalletCards(any(), any(), any());  // sanity check

        // Sometimes the unlock triggers multiple events
        mViewController.onDeviceLockStateChanged(false);
        mViewController.onDeviceLockStateChanged(false);

        verify(mWalletClient).getWalletCards(any(), any(), any());
    }

    /**
     * There is currently a bug in KeyguardStateController that causes it to report that the device
     * is unlocked, then locked, after biometric unlock. It should not respond to reports that the
     * phone has become locked after it is unlocked.
     */
    @Test
    public void onDeviceLockStateChanged_unlocked_locked_queriesCardsAndDoesNotHide() {
        mViewController =
                new WalletPanelViewController(mContext, mContext, mWalletClient, mPluginCallbacks,
                        true);
        when(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).thenReturn(false);
        WalletView walletView = (WalletView) mViewController.getPanelContent();
        View errorView = walletView.getErrorView();

        mViewController.queryWalletCards();

        verify(mWalletClient, never()).getWalletCards(any(), any(), any());
        assertThat(walletView.getVisibility()).isEqualTo(GONE);

        mViewController.onDeviceLockStateChanged(false);
        mViewController.onDeviceLockStateChanged(true);

        verify(mWalletClient).getWalletCards(any(), any(), any());
        assertThat(walletView.getVisibility()).isEqualTo(VISIBLE);
        assertThat(errorView.getVisibility()).isEqualTo(GONE);
    }

    @Test
    public void queryWalletCards_registersListenerAndRequestsWalletCards() {
        mViewController.queryWalletCards();

        verify(mWalletClient).addWalletServiceEventListener(mViewController);
        verify(mWalletClient).getWalletCards(
                any(), mRequestCaptor.capture(), mCallbackCaptor.capture());
        GetWalletCardsRequest request = mRequestCaptor.getValue();
        assertThat(request.getMaxCards()).isEqualTo(MAX_CARDS);
    }

    @Test
    public void queryWalletCards_onlyRegistersListenerOnce() {
        mViewController.queryWalletCards();
        mViewController.queryWalletCards();

        verify(mWalletClient, times(1)).addWalletServiceEventListener(mViewController);
        verify(mWalletClient, times(2)).getWalletCards(any(), any(), any());
    }

    @Test
    public void queryWalletCards_deviceLocked_cardsAllowedOnLockScreen_queriesCards() {
        mViewController =
                new WalletPanelViewController(mContext, mContext, mWalletClient, mPluginCallbacks,
                        true);
        when(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).thenReturn(true);

        mViewController.queryWalletCards();

        verify(mWalletClient).addWalletServiceEventListener(mViewController);
        verify(mWalletClient).getWalletCards(any(), any(), any());
    }

    @Test
    public void queryWalletCards_deviceLocked_cardsNotAllowedOnLockScreen_doesNotQueryCards() {
        mViewController =
                new WalletPanelViewController(mContext, mContext, mWalletClient, mPluginCallbacks,
                        true);
        when(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).thenReturn(false);

        mViewController.queryWalletCards();

        verify(mWalletClient).addWalletServiceEventListener(mViewController);
        verify(mWalletClient, never()).getWalletCards(any(), any(), any());
    }

    @Test
    public void onWalletCardsRetrieved_showsCards() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        View errorView = walletView.getErrorView();
        RecyclerView carouselView = walletView.getCardCarousel();
        assertThat(errorView.getVisibility()).isEqualTo(View.GONE);
        assertThat(carouselView.getVisibility()).isEqualTo(VISIBLE);
        int itemCount = carouselView.getAdapter().getItemCount();
        assertThat(itemCount).isEqualTo(2);
    }

    @Test
    public void onWalletCardsRetrieved_showsOverflowButton_startWalletActivity() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        View overflowIcon = walletView.getOverflowIcon();
        assertThat(overflowIcon.getVisibility()).isEqualTo(VISIBLE);

        overflowIcon.performClick();

        ListPopupWindow popupWindow = walletView.getOverflowPopup();
        assertThat(popupWindow.isShowing()).isTrue();
        assertThat(popupWindow.getListView().getCount()).isEqualTo(2);
        assertThat(((TextView) popupWindow.getListView().getChildAt(0)).getText()).isEqualTo(
                SHORTCUT_SHORT_LABEL);
        assertThat(((TextView) popupWindow.getListView().getChildAt(1)).getText()).isEqualTo(
                "Settings");

        popupWindow.performItemClick(0);

        Intent nextIntent = shadowOf(mContext).getNextStartedActivity();
        assertThat(nextIntent).isEqualTo(mWalletIntent);
        verify(mPluginCallbacks).startPendingIntentDismissingKeyguard(any());
        verify(mPluginCallbacks).dismissGlobalActionsMenu();
    }

    @Test
    public void onWalletCardsRetrieved_noShortLabel_showsOnlySettingsInOverflowMenu() {
        when(mWalletClient.getShortcutShortLabel()).thenReturn(null);
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        walletView.getOverflowIcon().performClick();
        ListPopupWindow popupWindow = walletView.getOverflowPopup();
        assertThat(popupWindow.isShowing()).isTrue();
        assertThat(popupWindow.getListView().getCount()).isEqualTo(1);
        assertThat(((TextView) popupWindow.getListView().getChildAt(0)).getText()).isEqualTo(
                "Settings");

        popupWindow.performItemClick(0);

        Intent nextIntent = shadowOf(mContext).getNextStartedActivity();
        assertThat(nextIntent.getAction()).isEqualTo(
                "com.android.settings.GLOBAL_ACTIONS_PANEL_SETTINGS");
        assertThat(nextIntent.getPackage()).isEqualTo("com.android.settings");
        verify(mPluginCallbacks).startPendingIntentDismissingKeyguard(any());
        verify(mPluginCallbacks).dismissGlobalActionsMenu();
    }

    @Test
    public void onWalletCardsRetrieved_noWalletIntent_showsOnlySettingsInOverflowMenu() {
        when(mWalletClient.createWalletIntent()).thenReturn(null);
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        ListPopupWindow popupWindow = walletView.getOverflowPopup();
        assertThat(popupWindow.isShowing()).isFalse();

        walletView.getOverflowIcon().performClick();

        assertThat(popupWindow.isShowing()).isTrue();
        assertThat(popupWindow.getListView().getCount()).isEqualTo(1);
        assertThat(((TextView) popupWindow.getListView().getChildAt(0)).getText()).isEqualTo(
                "Settings");
    }

    @Test
    public void onWalletCardsRetrieved_dismissed_doesNotShowCards() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);
        mViewController.onDismissed();

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        WalletCardCarousel carouselView = walletView.getCardCarousel();
        int itemCount = carouselView.getAdapter().getItemCount();
        assertThat(itemCount).isEqualTo(0);
    }

    @Test
    public void onWalletCardRetrievalError_showsErrorMessage() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        CharSequence errorMessage = "Cards unavailable";
        GetWalletCardsError error = new GetWalletCardsError(null, errorMessage);

        mCallbackCaptor.getValue().onWalletCardRetrievalError(error);

        WalletView view = (WalletView) mViewController.getPanelContent();
        TextView errorView = view.getErrorView();
        assertThat(view.getCardCarouselContainer().getVisibility()).isEqualTo(View.GONE);
        assertThat(errorView.getVisibility()).isEqualTo(VISIBLE);
        assertThat(errorView.getText()).isEqualTo(errorMessage);
    }

    @Test
    public void onWalletCardsRetrieved_cardDataEmpty_showsEmptyState() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        GetWalletCardsResponse response = new GetWalletCardsResponse(Collections.emptyList(), 0);

        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        ViewGroup emptyStateView = walletView.getEmptyStateView();
        assertThat(walletView.getCardCarouselContainer().getVisibility()).isEqualTo(View.GONE);
        assertThat(walletView.getErrorView().getVisibility()).isEqualTo(View.GONE);
        assertThat(emptyStateView.getVisibility()).isEqualTo(VISIBLE);
        // empty state view should have icon and text provided by client
        assertThat(emptyStateView.<TextView>requireViewById(R.id.title).getText()).isEqualTo(
                SHORTCUT_LONG_LABEL);
        assertThat(emptyStateView.<ImageView>requireViewById(R.id.icon).getDrawable())
                .isEqualTo(mWalletLogo);
        assertThat(emptyStateView.<ImageView>requireViewById(R.id.icon).getContentDescription())
                .isEqualTo(SERVICE_LABEL);

        // clicking on the button should start the activity
        emptyStateView.performClick();

        verify(mPluginCallbacks).startPendingIntentDismissingKeyguard(any());
        verify(mPluginCallbacks).dismissGlobalActionsMenu();
        Intent nextIntent = shadowOf(mContext).getNextStartedActivity();
        assertThat(nextIntent).isEqualTo(mWalletIntent);
    }

    @Test
    public void onWalletCardsRetrieved_cardDataEmpty_intentIsNull_hidesWallet() {
        when(mWalletClient.createWalletIntent()).thenReturn(null);
        GetWalletCardsResponse response = new GetWalletCardsResponse(Collections.emptyList(), 0);

        mViewController.onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        assertThat(walletView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void onWalletCardsRetrieved_cardDataEmpty_logoIsNull_hidesWallet() {
        when(mWalletClient.getLogo()).thenReturn(null);
        GetWalletCardsResponse response = new GetWalletCardsResponse(Collections.emptyList(), 0);

        mViewController.onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        assertThat(walletView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void onWalletCardsRetrieved_cardDataEmpty_labelIsNull_hidesWallet() {
        when(mWalletClient.getShortcutLongLabel()).thenReturn(null);
        GetWalletCardsResponse response = new GetWalletCardsResponse(Collections.emptyList(), 0);

        mViewController.onWalletCardsRetrieved(response);

        WalletView walletView = (WalletView) mViewController.getPanelContent();
        assertThat(walletView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void onWalletServiceEvent_tapStarted_dismissesGlobalActionsMenu() {
        mViewController.queryWalletCards();
        verify(mWalletClient).addWalletServiceEventListener(mListenerCaptor.capture());
        WalletServiceEvent event =
                new WalletServiceEvent(WalletServiceEvent.TYPE_NFC_PAYMENT_STARTED);

        WalletServiceEventListener listener = mListenerCaptor.getValue();
        listener.onWalletServiceEvent(event);

        verify(mPluginCallbacks).dismissGlobalActionsMenu();
        verify(mWalletClient).removeWalletServiceEventListener(listener);
        verify(mWalletClient).notifyWalletDismissed();
    }

    @Test
    public void onWalletServiceEvent_cardsChanged_requeriesCards() {
        mViewController.queryWalletCards();
        verify(mWalletClient).addWalletServiceEventListener(mListenerCaptor.capture());
        WalletServiceEvent event =
                new WalletServiceEvent(WalletServiceEvent.TYPE_WALLET_CARDS_UPDATED);

        WalletServiceEventListener listener = mListenerCaptor.getValue();
        listener.onWalletServiceEvent(event);

        verify(mPluginCallbacks, never()).dismissGlobalActionsMenu();
        verify(mWalletClient, never()).notifyWalletDismissed();
        verify(mWalletClient, times(2)).getWalletCards(any(), any(), any());
    }

    @Test
    public void onCardSelected_selectsCardWithClient() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);
        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);
        WalletView view = (WalletView) mViewController.getPanelContent();
        WalletCardCarousel carouselView = view.getCardCarousel();

        carouselView.scrollToPosition(1);

        verify(mWalletClient, times(2)).selectWalletCard(mSelectCardRequestCaptor.capture());
        List<SelectWalletCardRequest> selectRequests = mSelectCardRequestCaptor.getAllValues();
        // The first select happens as soon as cards are shown.
        assertThat(selectRequests.get(0).getCardId()).isEqualTo("c1");
        // The second select happens when the user scrolls to that card.
        assertThat(selectRequests.get(1).getCardId()).isEqualTo("c2");
    }

    @Test
    public void onCardClicked_startsIntent() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);
        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);
        WalletView view = (WalletView) mViewController.getPanelContent();
        WalletCardCarousel carouselView = view.getCardCarousel();
        // layout carousel so that child views are added and card scrolling 'works'
        carouselView.measure(0, 0);
        int width = mContext.getResources().getDisplayMetrics().widthPixels;
        int height = mContext.getResources().getDisplayMetrics().heightPixels;
        carouselView.layout(0, 0, width, height / 2);
        carouselView.scrollToPosition(0);

        // Perform click on CardView
        ((ViewGroup) carouselView.getChildAt(0)).getChildAt(0).performClick();

        PendingIntent pendingIntent = cards.get(0).getPendingIntent();
        verify(mPluginCallbacks).startPendingIntentDismissingKeyguard(pendingIntent);
        verify(mPluginCallbacks).dismissGlobalActionsMenu();
        verify(mWalletClient).notifyWalletDismissed();
        verify(mWalletClient).removeWalletServiceEventListener(any());
    }

    @Test
    public void onSingleTapUp_doesNotdismissWallet() {
        mViewController.queryWalletCards();
        verify(mWalletClient).getWalletCards(any(), mRequestCaptor.capture(),
                mCallbackCaptor.capture());
        List<WalletCard> cards = Arrays.asList(createWalletCard("c1"), createWalletCard("c2"));
        GetWalletCardsResponse response = new GetWalletCardsResponse(cards, 0);
        mCallbackCaptor.getValue().onWalletCardsRetrieved(response);
        WalletView view = (WalletView) mViewController.getPanelContent();
        WalletCardCarousel carouselView = view.getCardCarousel();
        // layout carousel so that child views are added and card scrolling 'works'
        carouselView.measure(0, 0);
        int width = mContext.getResources().getDisplayMetrics().widthPixels;
        int height = mContext.getResources().getDisplayMetrics().heightPixels;
        carouselView.layout(0, 0, width, height / 2);

        view.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0));
        view.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));

        verify(mPluginCallbacks, never()).startPendingIntentDismissingKeyguard(any());
        verify(mPluginCallbacks, never()).dismissGlobalActionsMenu();
        verify(mWalletClient, never()).notifyWalletDismissed();
        verify(mWalletClient, never()).removeWalletServiceEventListener(any());
    }

    private WalletCard createWalletCard(String cardId) {
        Icon cardImage = Icon.createWithBitmap(
                Bitmap.createBitmap(70, 44, Bitmap.Config.ARGB_8888));
        Intent intent = new Intent(Intent.ACTION_VIEW)
                .setComponent(new ComponentName("foo.bar.wallet", "foo.bar.wallet.WalletActivity"))
                .putExtra("cardId", cardId);
        PendingIntent pendingIntent =
                PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_IMMUTABLE);
        return new WalletCard.Builder(cardId, cardImage, cardId + " card", pendingIntent).build();
    }
}
