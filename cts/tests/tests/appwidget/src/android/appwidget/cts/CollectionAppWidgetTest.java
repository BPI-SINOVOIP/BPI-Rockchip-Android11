/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.appwidget.cts;

import static android.appwidget.cts.provider.CollectionAppWidgetProvider.BROADCAST_ACTION;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Instrumentation;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetHostView;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.appwidget.cts.activity.EmptyActivity;
import android.appwidget.cts.provider.CollectionAppWidgetProvider;
import android.appwidget.cts.service.MyAppWidgetService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.view.View;
import android.widget.AbsListView;
import android.widget.ListView;
import android.widget.RemoteViews;
import android.widget.RemoteViewsService;
import android.widget.StackView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test AppWidgets which host collection widgets.
 */
@LargeTest
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class CollectionAppWidgetTest extends AppWidgetTestCase {
    public static final String[] COUNTRY_LIST = new String[] {
        "Argentina", "Australia", "Belize", "Botswana", "Brazil", "Cameroon", "China", "Cyprus",
        "Denmark", "Djibouti", "Ethiopia", "Fiji", "Finland", "France", "Gabon", "Germany",
        "Ghana", "Haiti", "Honduras", "Iceland", "India", "Indonesia", "Ireland", "Italy",
        "Japan", "Kiribati", "Laos", "Lesotho", "Liberia", "Malaysia", "Mongolia", "Myanmar",
        "Nauru", "Norway", "Oman", "Pakistan", "Philippines", "Portugal", "Romania", "Russia",
        "Rwanda", "Singapore", "Slovakia", "Slovenia", "Somalia", "Swaziland", "Togo", "Tuvalu",
        "Uganda", "Ukraine", "United States", "Vanuatu", "Venezuela", "Zimbabwe"
    };

    private static final long TEST_TIMEOUT_MS = 5000;

    @Rule
    public ActivityTestRule<EmptyActivity> mActivityRule =
            new ActivityTestRule<>(EmptyActivity.class);

    private Instrumentation mInstrumentation;

    private Context mContext;

    private boolean mHasAppWidgets;

    private AppWidgetHostView mAppWidgetHostView;

    private int mAppWidgetId;

    private StackView mStackView;

    private ListView mListView;

    private AppWidgetHost mAppWidgetHost;

    @Before
    public void setup() throws Throwable {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();

        mHasAppWidgets = hasAppWidgets();
        if (!mHasAppWidgets) {
            return;
        }

        // We want to bind widgets - run a shell command to grant bind permission to our
        // package.
        grantBindAppWidgetPermission();

        mAppWidgetHost = new AppWidgetHost(mContext, 0);

        mAppWidgetHost.deleteHost();
        mAppWidgetHost.startListening();

        // Configure the app widget provider behavior
        final CountDownLatch providerCountDownLatch = new CountDownLatch(2);
        CollectionAppWidgetProvider.configure(providerCountDownLatch, null, null);

        // Grab the provider to be bound
        final AppWidgetProviderInfo providerInfo = getAppWidgetProviderInfo();

        // Allocate a widget id to bind
        mAppWidgetId = mAppWidgetHost.allocateAppWidgetId();

        // Bind the app widget
        boolean isBinding = getAppWidgetManager().bindAppWidgetIdIfAllowed(mAppWidgetId,
                providerInfo.getProfile(), providerInfo.provider, null);
        assertTrue(isBinding);

        // Wait for onEnabled and onUpdate calls on our provider
        try {
            assertTrue(providerCountDownLatch.await(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ie) {
            fail(ie.getMessage());
        }

        // Configure the app widget service behavior
        final CountDownLatch factoryCountDownLatch = new CountDownLatch(2);
        RemoteViewsService.RemoteViewsFactory factory =
                mock(RemoteViewsService.RemoteViewsFactory.class);
        when(factory.getCount()).thenReturn(COUNTRY_LIST.length);
        doAnswer(new Answer<RemoteViews>() {
            @Override
            public RemoteViews answer(InvocationOnMock invocation) throws Throwable {
                final int position = (Integer) invocation.getArguments()[0];
                RemoteViews remoteViews = new RemoteViews(mContext.getPackageName(),
                        R.layout.remoteviews_adapter_item);
                remoteViews.setTextViewText(R.id.item, COUNTRY_LIST[position]);

                // Set a fill-intent which will be used to fill-in the pending intent template
                // which is set on the collection view in CollectionAppWidgetProvider.
                Bundle extras = new Bundle();
                extras.putString(BlockingBroadcastReceiver.KEY_PARAM, COUNTRY_LIST[position]);
                Intent fillInIntent = new Intent();
                fillInIntent.putExtras(extras);
                remoteViews.setOnClickFillInIntent(R.id.item, fillInIntent);

                if (position == 0) {
                    factoryCountDownLatch.countDown();
                }
                return remoteViews;
            }
        }).when(factory).getViewAt(any(int.class));
        when(factory.getViewTypeCount()).thenReturn(1);
        MyAppWidgetService.setFactory(factory);

        mActivityRule.runOnUiThread(
                () -> mAppWidgetHostView = mAppWidgetHost.createView(
                        mContext, mAppWidgetId, providerInfo));

        // Wait our factory to be called to create the first item
        try {
            assertTrue(factoryCountDownLatch.await(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ie) {
            fail(ie.getMessage());
        }

        // Add our host view to the activity behind this test. This is similar to how launchers
        // add widgets to the on-screen UI.
        mActivityRule.runOnUiThread(() -> {
            EmptyActivity activity = mActivityRule.getActivity();
            activity.setContentView(mAppWidgetHostView);
        });
    }

    @After
    public void teardown() throws Exception {
        if (!mHasAppWidgets) {
            return;
        }
        mAppWidgetHost.deleteHost();
        revokeBindAppWidgetPermission();
    }

    private AppWidgetProviderInfo getAppWidgetProviderInfo() {
        ComponentName firstComponentName = new ComponentName(mContext.getPackageName(),
                CollectionAppWidgetProvider.class.getName());

        return getProviderInfo(firstComponentName);
    }

    @Test
    public void testInitialState() {
        if (!mHasAppWidgets) {
            return;
        }

        assertNotNull(mAppWidgetHostView);
        mStackView = (StackView) mAppWidgetHostView.findViewById(R.id.remoteViews_stack);
        assertNotNull(mStackView);

        assertEquals(COUNTRY_LIST.length, mStackView.getCount());
        assertEquals(0, mStackView.getDisplayedChild());
        assertEquals(R.id.remoteViews_empty, mStackView.getEmptyView().getId());
    }

    private void verifySetDisplayedChild(int displayedChildIndex) {
        final CountDownLatch updateLatch = new CountDownLatch(1);
        CollectionAppWidgetProvider.configure(updateLatch, null, null);

        // Create the intent to update the widget. Note that we're passing the value
        // for displayed child index in the intent
        Intent intent = new Intent(mContext, CollectionAppWidgetProvider.class);
        intent.setAction(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        intent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, new  int[] { mAppWidgetId });
        intent.putExtra(CollectionAppWidgetProvider.KEY_DISPLAYED_CHILD_INDEX, displayedChildIndex);
        mContext.sendBroadcast(intent);

        // Wait until the update request has been processed
        try {
            assertTrue(updateLatch.await(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ie) {
            fail(ie.getMessage());
        }
        // And wait until the underlying StackView has been updated to switch to the requested
        // child
        PollingCheck.waitFor(TEST_TIMEOUT_MS,
                () -> mStackView.getDisplayedChild() == displayedChildIndex);
    }

    @Test
    public void testSetDisplayedChild() {
        if (!mHasAppWidgets) {
            return;
        }

        mStackView = (StackView) mAppWidgetHostView.findViewById(R.id.remoteViews_stack);

        verifySetDisplayedChild(4);
        verifySetDisplayedChild(2);
        verifySetDisplayedChild(6);
    }

    private void verifyShowCommand(String intentShowKey, int expectedDisplayedChild) {
        final CountDownLatch updateLatch = new CountDownLatch(1);
        CollectionAppWidgetProvider.configure(updateLatch, null, null);

        // Create the intent to update the widget. Note that we're passing the "indication"
        // which one of showNext / showPrevious APIs to execute in the intent that we're
        // creating.
        Intent intent = new Intent(mContext, CollectionAppWidgetProvider.class);
        intent.setAction(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        intent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, new  int[] { mAppWidgetId });
        intent.putExtra(intentShowKey, true);
        mContext.sendBroadcast(intent);

        // Wait until the update request has been processed
        try {
            assertTrue(updateLatch.await(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ie) {
            fail(ie.getMessage());
        }
        // And wait until the underlying StackView has been updated to switch to the expected
        // child
        PollingCheck.waitFor(TEST_TIMEOUT_MS,
                () -> mStackView.getDisplayedChild() == expectedDisplayedChild);
    }

    @Test
    public void testShowNextPrevious() {
        if (!mHasAppWidgets) {
            return;
        }

        mStackView = (StackView) mAppWidgetHostView.findViewById(R.id.remoteViews_stack);

        // Two forward
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, 1);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, 2);
        // Four back (looping to the end of the adapter data)
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_PREVIOUS, 1);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_PREVIOUS, 0);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_PREVIOUS, COUNTRY_LIST.length - 1);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_PREVIOUS, COUNTRY_LIST.length - 2);
        // And three forward (looping to the start of the adapter data)
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, COUNTRY_LIST.length - 1);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, 0);
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, 1);
    }

    private void verifyItemClickIntents(int indexToClick) throws Throwable {
        BlockingBroadcastReceiver receiver = new BlockingBroadcastReceiver();
        mActivityRule.runOnUiThread(() -> receiver.register(BROADCAST_ACTION));

        mStackView = (StackView) mAppWidgetHostView.findViewById(R.id.remoteViews_stack);
        PollingCheck.waitFor(() -> mStackView.getCurrentView() != null);
        final View initialView = mStackView.getCurrentView();
        mActivityRule.runOnUiThread(
                () -> mStackView.performItemClick(initialView, indexToClick, 0L));
        assertEquals(COUNTRY_LIST[indexToClick],
                receiver.getParam(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    public void testSetOnClickPendingIntent() throws Throwable {
        if (!mHasAppWidgets) {
            return;
        }

        verifyItemClickIntents(0);

        // Switch to another child
        verifySetDisplayedChild(2);
        verifyItemClickIntents(2);

        // And one more
        verifyShowCommand(CollectionAppWidgetProvider.KEY_SHOW_NEXT, 3);
        verifyItemClickIntents(3);
    }

    private class ListScrollListener implements AbsListView.OnScrollListener {
        private CountDownLatch mLatchToNotify;

        private int mTargetPosition;

        public ListScrollListener(CountDownLatch latchToNotify, int targetPosition) {
            mLatchToNotify = latchToNotify;
            mTargetPosition = targetPosition;
        }

        @Override
        public void onScrollStateChanged(AbsListView view, int scrollState) {
        }

        @Override
        public void onScroll(AbsListView view, int firstVisibleItem, int visibleItemCount,
                int totalItemCount) {
            if ((mTargetPosition >= firstVisibleItem) &&
                    (mTargetPosition <= (firstVisibleItem + visibleItemCount))) {
                mLatchToNotify.countDown();
            }
        }
    }

    @Test
    public void testSetScrollPosition() {
        if (!mHasAppWidgets) {
            return;
        }

        mListView = (ListView) mAppWidgetHostView.findViewById(R.id.remoteViews_list);

        final CountDownLatch updateLatch = new CountDownLatch(1);
        final AtomicBoolean scrollToPositionIsComplete = new AtomicBoolean(false);
        // We're configuring our provider with three parameters:
        // 1. The CountDownLatch to be notified when the provider has been enabled
        // 2. The gating condition that waits until ListView has populated its content
        //    so that we can proceed to call setScrollPosition on it
        // 3. The gating condition that waits until the setScrollPosition has completed
        //    its processing / scrolling so that we can proceed to call
        //    setRelativeScrollPosition on it
        CollectionAppWidgetProvider.configure(updateLatch, () -> mListView.getChildCount() > 0,
                scrollToPositionIsComplete::get);

        final int positionToScrollTo = COUNTRY_LIST.length - 10;
        final int scrollByAmount = COUNTRY_LIST.length / 2;
        final int offsetScrollTarget = positionToScrollTo - scrollByAmount;

        // Register the first scroll listener on our ListView. The listener will notify our latch
        // when the "target" item comes into view. If that never happens, the latch will
        // time out and fail the test.
        final CountDownLatch scrollToPositionLatch = new CountDownLatch(1);
        mListView.setOnScrollListener(
                new ListScrollListener(scrollToPositionLatch, positionToScrollTo));

        // Create the intent to update the widget. Note that we're passing the "indication"
        // to switch to our ListView in the intent that we're creating.
        Intent intent = new Intent(mContext, CollectionAppWidgetProvider.class);
        intent.setAction(AppWidgetManager.ACTION_APPWIDGET_UPDATE);
        intent.putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, new  int[] { mAppWidgetId });
        intent.putExtra(CollectionAppWidgetProvider.KEY_SWITCH_TO_LIST, true);
        intent.putExtra(CollectionAppWidgetProvider.KEY_SCROLL_POSITION, positionToScrollTo);
        intent.putExtra(CollectionAppWidgetProvider.KEY_SCROLL_OFFSET, -scrollByAmount);
        mContext.sendBroadcast(intent);

        // Wait until the update request has been processed
        try {
            assertTrue(updateLatch.await(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException ie) {
            fail(ie.getMessage());
        }
        // And wait until the underlying ListView has been updated to be visible
        PollingCheck.waitFor(TEST_TIMEOUT_MS, () -> mListView.getVisibility() == View.VISIBLE);

        // Wait until our ListView has at least one visible child view. At that point we know
        // that not only the host view is on screen, but also that the list view has completed
        // its layout pass after having asked its adapter to populate the list content.
        PollingCheck.waitFor(TEST_TIMEOUT_MS, () -> mListView.getChildCount() > 0);

        // If we're on a really big display, we might be in a situation where the position
        // we're going to scroll to is already visible. In that case the logic in the rest
        // of this test will never fire off a listener callback and then fail the test.
        final int lastVisiblePosition = mListView.getLastVisiblePosition();
        if (positionToScrollTo <= lastVisiblePosition) {
            return;
        }

        boolean result = false;
        try {
            result = scrollToPositionLatch.await(20, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            // ignore
        }
        assertTrue("Timed out while waiting for the target view to be scrolled into view", result);

        if ((offsetScrollTarget < 0) ||
                (offsetScrollTarget >= mListView.getFirstVisiblePosition())) {
            // We can't scroll up because the target is either already visible or negative
            return;
        }

        // Now register another scroll listener on our ListView. The listener will notify our latch
        // when our new "target" item comes into view. If that never happens, the latch will
        // time out and fail the test.
        final CountDownLatch scrollByOffsetLatch = new CountDownLatch(1);
        mListView.setOnScrollListener(
                new ListScrollListener(scrollByOffsetLatch, offsetScrollTarget));

        // Update our atomic boolean to "kick off" the widget provider request to call
        // setRelativeScrollPosition on our RemoteViews
        scrollToPositionIsComplete.set(true);
        try {
            result = scrollByOffsetLatch.await(20, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            // ignore
        }
        assertTrue("Timed out while waiting for the target view to be scrolled into view", result);
    }
}
