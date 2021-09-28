/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static android.view.View.FIND_VIEWS_WITH_TEXT;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.app.SharedElementCallback;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetHostView;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.appwidget.cts.activity.EmptyActivity;
import android.appwidget.cts.activity.TransitionActivity;
import android.appwidget.cts.service.MyAppWidgetService;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.platform.test.annotations.AppModeFull;
import android.util.ArrayMap;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.ListView;
import android.widget.RemoteViews;
import android.widget.RemoteViewsService;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Predicate;

/**
 * Test AppWidgets transitions.
 */
@LargeTest
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class WidgetTransitionTest extends AppWidgetTestCase {

    private static final String CLICK_ACTION = WidgetTransitionTest.class.getSimpleName();

    @Rule
    public ActivityTestRule<EmptyActivity> mActivityRule =
            new ActivityTestRule<>(EmptyActivity.class);

    private boolean mHasAppWidgets;

    private EmptyActivity mActivity;

    private AppWidgetHost mAppWidgetHost;

    private MyHostView mAppWidgetHostView;
    private int mAppWidgetId;

    @Before
    public void setup() throws Throwable {
        mHasAppWidgets = hasAppWidgets();
        if (!mHasAppWidgets) {
            return;
        }
        // We want to bind widgets - run a shell command to grant bind permission to our
        // package.
        grantBindAppWidgetPermission();

        mActivity = mActivityRule.getActivity();
        mActivityRule.runOnUiThread(this::bindNewWidget);
    }

    @After
    public void teardown() throws Exception {
        if (!mHasAppWidgets) {
            return;
        }
        mAppWidgetHost.deleteHost();
        revokeBindAppWidgetPermission();
    }

    private void bindNewWidget() {
        mAppWidgetHost = new AppWidgetHost(mActivity, 0) {
            @Override
            protected AppWidgetHostView onCreateView(Context context, int appWidgetId,
                    AppWidgetProviderInfo appWidget) {
                return new MyHostView(context);
            }
        };
        mAppWidgetHost.deleteHost();
        mAppWidgetHost.startListening();

        // Allocate a widget id to bind
        mAppWidgetId = mAppWidgetHost.allocateAppWidgetId();

        // Bind the app widget
        final AppWidgetProviderInfo providerInfo = getProviderInfo(getFirstWidgetComponent());
        boolean isBinding = getAppWidgetManager().bindAppWidgetIdIfAllowed(mAppWidgetId,
                providerInfo.getProfile(), providerInfo.provider, null);
        assertTrue(isBinding);

        // Create host view
        mAppWidgetHostView = (MyHostView) mAppWidgetHost
                .createView(mActivity, mAppWidgetId, providerInfo);
        mActivity.setContentView(mAppWidgetHostView);
    }

    @Test
    public void testWidget_sendBroadcast() throws Throwable {
        if (!mHasAppWidgets) {
            return;
        }
        // Push update
        RemoteViews views = getViewsForResponse(RemoteViews.RemoteResponse.fromPendingIntent(
                PendingIntent.getBroadcast(mActivity, 0,
                        new Intent(CLICK_ACTION), PendingIntent.FLAG_UPDATE_CURRENT)));
        getAppWidgetManager().updateAppWidget(new int[] {mAppWidgetId}, views);

        // Await until update
        CountDownLatch updateLatch = new CountDownLatch(1);
        mAppWidgetHostView.mCommands.put(
                (v) -> v.findViewById(R.id.hello) != null, updateLatch::countDown);
        updateLatch.await();

        // Perform click
        verifyClickBroadcast(mAppWidgetHostView);
    }

    @Test
    public void testWidget_startActivity() throws Throwable {
        if (!mHasAppWidgets) {
            return;
        }
        // Push update
        RemoteViews views = getViewsForResponse(RemoteViews.RemoteResponse.fromPendingIntent(
                PendingIntent.getActivity(mActivity, 0,
                        new Intent(mActivity, TransitionActivity.class),
                        PendingIntent.FLAG_UPDATE_CURRENT)));
        getAppWidgetManager().updateAppWidget(new int[] {mAppWidgetId}, views);

        // Await until update
        CountDownLatch updateLatch = new CountDownLatch(1);
        mAppWidgetHostView.mCommands.put(
                (v) -> v.findViewById(R.id.hello) != null, updateLatch::countDown);
        updateLatch.await();

        // Perform click
        BlockingBroadcastReceiver receiver = new BlockingBroadcastReceiver()
                .register(TransitionActivity.BROADCAST_ACTION);
        CountDownLatch sharedElementCallbackLatch = new CountDownLatch(1);
        mActivity.setExitSharedElementCallback(new SharedElementCallback() {
            @Override
            public void onSharedElementsArrived(
                    List<String> sharedElementNames,
                    List<View> sharedElements,
                    final OnSharedElementsReadyListener listener) {
                sharedElementCallbackLatch.countDown();
                mActivity.getWindow().getDecorView().postDelayed(
                        listener::onSharedElementsReady, 60);
            }
        });
        mActivityRule.runOnUiThread(
                () -> mAppWidgetHostView.findViewById(R.id.hello).performClick());

        receiver.await();
        assertTrue(sharedElementCallbackLatch.await(1000, TimeUnit.MILLISECONDS));
        String[] elements = receiver.result
                .getStringArrayListExtra(TransitionActivity.EXTRA_ELEMENTS).toArray(new String[0]);
        assertArrayEquals(new String[] {"green_square"}, elements);
    }

    @Test
    public void testCollection_sendBroadcast() throws Throwable {
        if (!mHasAppWidgets) {
            return;
        }

        // Configure the app widget service behavior
        RemoteViewsService.RemoteViewsFactory factory =
                mock(RemoteViewsService.RemoteViewsFactory.class);
        when(factory.getCount()).thenReturn(3);
        doAnswer(invocation -> {
            final int position = (Integer) invocation.getArguments()[0];
            RemoteViews remoteViews = getViewsForResponse(RemoteViews.RemoteResponse
                    .fromFillInIntent(new Intent().putExtra("item_id", position)));
            remoteViews.setTextViewText(R.id.hello, "Text " + position);
            return remoteViews;
        }).when(factory).getViewAt(any(int.class));
        when(factory.getViewTypeCount()).thenReturn(1);
        MyAppWidgetService.setFactory(factory);

        // Push update
        RemoteViews views = new RemoteViews(mActivity.getPackageName(),
                R.layout.remoteviews_adapter);
        Intent listIntent = new Intent(mActivity, MyAppWidgetService.class)
                .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, mAppWidgetId);
        listIntent.setData(Uri.parse(listIntent.toUri(Intent.URI_INTENT_SCHEME)));
        views.setRemoteAdapter(R.id.remoteViews_list, listIntent);
        views.setViewVisibility(R.id.remoteViews_stack, View.GONE);
        views.setViewVisibility(R.id.remoteViews_list, View.VISIBLE);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mActivity, 0,
                new Intent(CLICK_ACTION), PendingIntent.FLAG_UPDATE_CURRENT);
        views.setPendingIntentTemplate(R.id.remoteViews_list, pendingIntent);

        // Await until update
        getAppWidgetManager().updateAppWidget(new int[] {mAppWidgetId}, views);
        CountDownLatch updateLatch = new CountDownLatch(1);
        mActivityRule.runOnUiThread(() -> mAppWidgetHostView.getViewTreeObserver()
                .addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        mAppWidgetHostView.post(this::verifyChildrenAdded);
                    }

                    private void verifyChildrenAdded() {
                        ListView listView = mAppWidgetHostView.findViewById(R.id.remoteViews_list);
                        if (listView == null || listView.getChildCount() != 3) {
                            return;
                        }
                        if (hasText("Text 0", listView.getChildAt(0))
                                && hasText("Text 1", listView.getChildAt(1))
                                && hasText("Text 2", listView.getChildAt(2))) {
                            updateLatch.countDown();
                        }
                    }

                    private boolean hasText(String text, View parent) {
                        ArrayList<View> out = new ArrayList<>();
                        parent.findViewsWithText(out, text, FIND_VIEWS_WITH_TEXT);
                        return !out.isEmpty();
                    }
                }));
        updateLatch.await();

        // Perform click on various elements
        ListView listView = mAppWidgetHostView.findViewById(R.id.remoteViews_list);
        verifyClickBroadcast(listView.getChildAt(0));
        verifyClickBroadcast(listView.getChildAt(1));
        verifyClickBroadcast(listView.getChildAt(2));
    }

    private RemoteViews getViewsForResponse(RemoteViews.RemoteResponse response) {
        RemoteViews views = new RemoteViews(mActivity.getPackageName(),
                R.layout.activity_transition);
        response.addSharedElement(R.id.greenSquare, "green_square");
        views.setOnClickResponse(R.id.hello, response);
        return views;
    }

    private void verifyClickBroadcast(View parent) throws Throwable {
        BlockingBroadcastReceiver receiver = new BlockingBroadcastReceiver().register(CLICK_ACTION);
        mActivityRule.runOnUiThread(() -> parent.findViewById(R.id.hello).performClick());

        receiver.await();
        assertNotNull(receiver.result);

        assertEquals(getSourceBounds(parent.findViewById(R.id.hello)),
                receiver.result.getSourceBounds());
        assertEquals(getSourceBounds(parent.findViewById(R.id.greenSquare)),
                receiver.result.getBundleExtra(RemoteViews.EXTRA_SHARED_ELEMENT_BOUNDS)
                        .getParcelable("green_square"));
    }

    private static Rect getSourceBounds(View v) {
        final int[] pos = new int[2];
        v.getLocationOnScreen(pos);
        return new Rect(pos[0], pos[1], pos[0] + v.getWidth(), pos[1] + v.getHeight());
    }

    /**
     * Host view which supports waiting for a update to happen.
     */
    private static class MyHostView extends AppWidgetHostView {

        final ArrayMap<Predicate<MyHostView>, Runnable> mCommands = new ArrayMap<>();

        MyHostView(Context context) {
            super(context);
        }

        @Override
        public void updateAppWidget(RemoteViews remoteViews) {
            super.updateAppWidget(remoteViews);

            for (int i = mCommands.size() - 1; i >= 0; i--) {
                if (mCommands.keyAt(i).test(this)) {
                    Runnable action = mCommands.valueAt(i);
                    mCommands.removeAt(i);
                    action.run();
                }
            }
        }
    }
}
