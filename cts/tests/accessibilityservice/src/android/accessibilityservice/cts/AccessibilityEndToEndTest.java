/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.accessibilityservice.cts;

import static android.accessibility.cts.common.InstrumentedAccessibilityService.enableService;
import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.filterForEventType;
import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.filterForEventTypeWithResource;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.findWindowByTitle;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.getActivityTitle;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.accessibilityservice.cts.utils.RunOnMainUtils.getOnMain;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_HIDE_TOOLTIP;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SHOW_TOOLTIP;

import static org.hamcrest.Matchers.in;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.hamcrest.core.IsNull.notNullValue;
import static org.hamcrest.core.IsNull.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.ShellCommandBuilder;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.cts.activities.AccessibilityEndToEndActivity;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Instrumentation;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.app.UiAutomation;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.Region;
import android.os.Process;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsMouseUtil;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.Iterator;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class performs end-to-end testing of the accessibility feature by
 * creating an {@link Activity} and poking around so {@link AccessibilityEvent}s
 * are generated and their correct dispatch verified.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityEndToEndTest {

    private static final String LOG_TAG = "AccessibilityEndToEndTest";

    private static final String GRANT_BIND_APP_WIDGET_PERMISSION_COMMAND =
            "appwidget grantbind --package android.accessibilityservice.cts --user ";

    private static final String REVOKE_BIND_APP_WIDGET_PERMISSION_COMMAND =
            "appwidget revokebind --package android.accessibilityservice.cts --user ";

    private static final String APP_WIDGET_PROVIDER_PACKAGE = "foo.bar.baz";

    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private AccessibilityEndToEndActivity mActivity;

    private ActivityTestRule<AccessibilityEndToEndActivity> mActivityRule =
            new ActivityTestRule<>(AccessibilityEndToEndActivity.class, false, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mDumpOnFailureRule);

    @BeforeClass
    public static void oneTimeSetup() throws Exception {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation();
    }

    @AfterClass
    public static void postTestTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        mActivity = launchActivityAndWaitForItToBeOnscreen(
                sInstrumentation, sUiAutomation, mActivityRule);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeViewSelectedAccessibilityEvent() throws Throwable {
        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_VIEW_SELECTED);
        expected.setClassName(ListView.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(mActivity.getString(R.string.second_list_item));
        expected.setItemCount(2);
        expected.setCurrentItemIndex(1);
        expected.setEnabled(true);
        expected.setScrollable(false);
        expected.setFromIndex(0);
        expected.setToIndex(1);

        final ListView listView = (ListView) mActivity.findViewById(R.id.listview);

        AccessibilityEvent awaitedEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        listView.setSelection(1);
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return equalsAccessiblityEvent(event, expected);
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeViewClickedAccessibilityEvent() throws Throwable {
        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_VIEW_CLICKED);
        expected.setClassName(Button.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(mActivity.getString(R.string.button_title));
        expected.setEnabled(true);

        final Button button = (Button) mActivity.findViewById(R.id.button);

        AccessibilityEvent awaitedEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        button.performClick();
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return equalsAccessiblityEvent(event, expected);
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeViewLongClickedAccessibilityEvent() throws Throwable {
        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_VIEW_LONG_CLICKED);
        expected.setClassName(Button.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(mActivity.getString(R.string.button_title));
        expected.setEnabled(true);

        final Button button = (Button) mActivity.findViewById(R.id.button);

        AccessibilityEvent awaitedEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        button.performLongClick();
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return equalsAccessiblityEvent(event, expected);
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeViewFocusedAccessibilityEvent() throws Throwable {
        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        expected.setClassName(Button.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(mActivity.getString(R.string.button_title));
        expected.setItemCount(5);
        expected.setCurrentItemIndex(3);
        expected.setEnabled(true);

        final Button button = (Button) mActivity.findViewById(R.id.buttonWithTooltip);

        AccessibilityEvent awaitedEvent =
            sUiAutomation.executeAndWaitForEvent(
                    () -> mActivity.runOnUiThread(() -> button.requestFocus()),
                    (event) -> equalsAccessiblityEvent(event, expected),
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeViewTextChangedAccessibilityEvent() throws Throwable {
        // focus the edit text
        final EditText editText = (EditText) mActivity.findViewById(R.id.edittext);

        AccessibilityEvent awaitedFocusEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        editText.requestFocus();
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return event.getEventType() == AccessibilityEvent.TYPE_VIEW_FOCUSED;
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected focuss event.", awaitedFocusEvent);

        final String beforeText = mActivity.getString(R.string.text_input_blah);
        final String newText = mActivity.getString(R.string.text_input_blah_blah);
        final String afterText = beforeText.substring(0, 3) + newText;

        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
        expected.setClassName(EditText.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(afterText);
        expected.setBeforeText(beforeText);
        expected.setFromIndex(3);
        expected.setAddedCount(9);
        expected.setRemovedCount(1);
        expected.setEnabled(true);

        AccessibilityEvent awaitedTextChangeEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        editText.getEditableText().replace(3, 4, newText);
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return equalsAccessiblityEvent(event, expected);
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedTextChangeEvent);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTypeWindowStateChangedAccessibilityEvent() throws Throwable {
        // create and populate the expected event
        final AccessibilityEvent expected = AccessibilityEvent.obtain();
        expected.setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        expected.setClassName(AlertDialog.class.getName());
        expected.setPackageName(mActivity.getPackageName());
        expected.getText().add(mActivity.getString(R.string.alert_title));
        expected.getText().add(mActivity.getString(R.string.alert_message));
        expected.setEnabled(true);

        AccessibilityEvent awaitedEvent =
            sUiAutomation.executeAndWaitForEvent(
                new Runnable() {
            @Override
            public void run() {
                // trigger the event
                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        (new AlertDialog.Builder(mActivity).setTitle(R.string.alert_title)
                                .setMessage(R.string.alert_message)).create().show();
                    }
                });
            }},
            new UiAutomation.AccessibilityEventFilter() {
                // check the received event
                @Override
                public boolean accept(AccessibilityEvent event) {
                    return equalsAccessiblityEvent(event, expected);
                }
            },
                    DEFAULT_TIMEOUT_MS);
        assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
    }

    @MediumTest
    @AppModeFull
    @SuppressWarnings("deprecation")
    @Presubmit
    @Test
    public void testTypeNotificationStateChangedAccessibilityEvent() throws Throwable {
        // No notification UI on televisions.
        if ((mActivity.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_TELEVISION) {
            Log.i(LOG_TAG, "Skipping: testTypeNotificationStateChangedAccessibilityEvent" +
                    " - No notification UI on televisions.");
            return;
        }
        PackageManager pm = sInstrumentation.getTargetContext().getPackageManager();
        if (pm.hasSystemFeature(pm.FEATURE_WATCH)) {
            Log.i(LOG_TAG, "Skipping: testTypeNotificationStateChangedAccessibilityEvent" +
                    " - Watches have different notification system.");
            return;
        }
        if (pm.hasSystemFeature(pm.FEATURE_AUTOMOTIVE)) {
            Log.i(LOG_TAG, "Skipping: testTypeNotificationStateChangedAccessibilityEvent" +
                    " - Automotive handle notifications differently.");
            return;
        }

        String message = mActivity.getString(R.string.notification_message);

        final NotificationManager notificationManager =
                (NotificationManager) mActivity.getSystemService(Service.NOTIFICATION_SERVICE);
        final NotificationChannel channel =
                new NotificationChannel("id", "name", NotificationManager.IMPORTANCE_DEFAULT);
        try {
            // create the notification to send
            channel.enableVibration(true);
            channel.enableLights(true);
            channel.setBypassDnd(true);
            notificationManager.createNotificationChannel(channel);
            NotificationChannel created =
                    notificationManager.getNotificationChannel(channel.getId());
            final int notificationId = 1;
            final Notification notification =
                    new Notification.Builder(mActivity, channel.getId())
                            .setSmallIcon(android.R.drawable.stat_notify_call_mute)
                            .setContentIntent(PendingIntent.getActivity(mActivity, 0,
                                    new Intent(),
                                    PendingIntent.FLAG_CANCEL_CURRENT))
                            .setTicker(message)
                            .setContentTitle("")
                            .setContentText("")
                            .setPriority(Notification.PRIORITY_MAX)
                            // Mark the notification as "interruptive" by specifying a vibration
                            // pattern. This ensures it's announced properly on watch-type devices.
                            .setVibrate(new long[]{})
                            .build();

            // create and populate the expected event
            final AccessibilityEvent expected = AccessibilityEvent.obtain();
            expected.setEventType(AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED);
            expected.setClassName(Notification.class.getName());
            expected.setPackageName(mActivity.getPackageName());
            expected.getText().add(message);
            expected.setParcelableData(notification);

            AccessibilityEvent awaitedEvent =
                    sUiAutomation.executeAndWaitForEvent(
                            new Runnable() {
                                @Override
                                public void run() {
                                    // trigger the event
                                    mActivity.runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            // trigger the event
                                            notificationManager
                                                    .notify(notificationId, notification);
                                            mActivity.finish();
                                        }
                                    });
                                }
                            },
                            new UiAutomation.AccessibilityEventFilter() {
                                // check the received event
                                @Override
                                public boolean accept(AccessibilityEvent event) {
                                    return equalsAccessiblityEvent(event, expected);
                                }
                            },
                            DEFAULT_TIMEOUT_MS);
            assertNotNull("Did not receive expected event: " + expected, awaitedEvent);
        } finally {
            notificationManager.deleteNotificationChannel(channel.getId());
        }
    }

    @MediumTest
    @Test
    public void testInterrupt_notifiesService() {
        sInstrumentation
                .getUiAutomation(UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        InstrumentedAccessibilityService service =
                enableService(InstrumentedAccessibilityService.class);

        try {
            assertFalse(service.wasOnInterruptCalled());

            mActivity.runOnUiThread(() -> {
                AccessibilityManager accessibilityManager = (AccessibilityManager) mActivity
                        .getSystemService(Service.ACCESSIBILITY_SERVICE);
                accessibilityManager.interrupt();
            });

            Object waitObject = service.getInterruptWaitObject();
            synchronized (waitObject) {
                if (!service.wasOnInterruptCalled()) {
                    try {
                        waitObject.wait(DEFAULT_TIMEOUT_MS);
                    } catch (InterruptedException e) {
                        // Do nothing
                    }
                }
            }
            assertTrue(service.wasOnInterruptCalled());
        } finally {
            service.disableSelfAndRemove();
        }
    }

    @MediumTest
    @Test
    public void testPackageNameCannotBeFaked() throws Exception {
        mActivity.runOnUiThread(() -> {
            // Set the activity to report fake package for events and nodes
            mActivity.setReportedPackageName("foo.bar.baz");

            // Make sure node package cannot be faked
            AccessibilityNodeInfo root = sUiAutomation
                    .getRootInActiveWindow();
            assertPackageName(root, mActivity.getPackageName());
        });

        // Make sure event package cannot be faked
        try {
            sUiAutomation.executeAndWaitForEvent(() ->
                sInstrumentation.runOnMainSync(() ->
                    mActivity.findViewById(R.id.button).requestFocus())
                , (AccessibilityEvent event) ->
                    event.getEventType() == AccessibilityEvent.TYPE_VIEW_FOCUSED
                            && event.getPackageName().equals(mActivity.getPackageName())
                , DEFAULT_TIMEOUT_MS);
        } catch (TimeoutException e) {
            fail("Events from fake package should be fixed to use the correct package");
        }
    }

    @AppModeFull
    @MediumTest
    @Presubmit
    @Test
    public void testPackageNameCannotBeFakedAppWidget() throws Exception {
        if (!hasAppWidgets()) {
            return;
        }

        sInstrumentation.runOnMainSync(() -> {
            // Set the activity to report fake package for events and nodes
            mActivity.setReportedPackageName(APP_WIDGET_PROVIDER_PACKAGE);

            // Make sure we cannot report nodes as if from the widget package
            AccessibilityNodeInfo root = sUiAutomation
                    .getRootInActiveWindow();
            assertPackageName(root, mActivity.getPackageName());
        });

        // Make sure we cannot send events as if from the widget package
        try {
            sUiAutomation.executeAndWaitForEvent(() ->
                sInstrumentation.runOnMainSync(() ->
                    mActivity.findViewById(R.id.button).requestFocus())
                , (AccessibilityEvent event) ->
                    event.getEventType() == AccessibilityEvent.TYPE_VIEW_FOCUSED
                            && event.getPackageName().equals(mActivity.getPackageName())
                , DEFAULT_TIMEOUT_MS);
        } catch (TimeoutException e) {
            fail("Should not be able to send events from a widget package if no widget hosted");
        }

        // Create a host and start listening.
        final AppWidgetHost host = new AppWidgetHost(sInstrumentation.getTargetContext(), 0);
        host.deleteHost();
        host.startListening();

        // Well, app do not have this permission unless explicitly granted
        // by the user. Now we will pretend for the user and grant it.
        grantBindAppWidgetPermission();

        // Allocate an app widget id to bind.
        final int appWidgetId = host.allocateAppWidgetId();
        try {
            // Grab a provider we defined to be bound.
            final AppWidgetProviderInfo provider = getAppWidgetProviderInfo();

            // Bind the widget.
            final boolean widgetBound = getAppWidgetManager().bindAppWidgetIdIfAllowed(
                    appWidgetId, provider.getProfile(), provider.provider, null);
            assertTrue(widgetBound);

            // Make sure the app can use the package of a widget it hosts
            sInstrumentation.runOnMainSync(() -> {
                // Make sure we can report nodes as if from the widget package
                AccessibilityNodeInfo root = sUiAutomation
                        .getRootInActiveWindow();
                assertPackageName(root, APP_WIDGET_PROVIDER_PACKAGE);
            });

            // Make sure we can send events as if from the widget package
            try {
                sUiAutomation.executeAndWaitForEvent(() ->
                    sInstrumentation.runOnMainSync(() ->
                        mActivity.findViewById(R.id.button).performClick())
                    , (AccessibilityEvent event) ->
                            event.getEventType() == AccessibilityEvent.TYPE_VIEW_CLICKED
                                    && event.getPackageName().equals(APP_WIDGET_PROVIDER_PACKAGE)
                    , DEFAULT_TIMEOUT_MS);
            } catch (TimeoutException e) {
                fail("Should be able to send events from a widget package if widget hosted");
            }
        } finally {
            // Clean up.
            host.deleteAppWidgetId(appWidgetId);
            host.deleteHost();
            revokeBindAppWidgetPermission();
        }
    }

    @MediumTest
    @Presubmit
    @Test
    public void testViewHeadingReportedToAccessibility() throws Exception {
        final EditText editText = (EditText) getOnMain(sInstrumentation, () -> {
            return mActivity.findViewById(R.id.edittext);
        });
        // Make sure the edittext was populated properly from xml
        final boolean editTextIsHeading = getOnMain(sInstrumentation, () -> {
            return editText.isAccessibilityHeading();
        });
        assertTrue("isAccessibilityHeading not populated properly from xml", editTextIsHeading);

        final AccessibilityNodeInfo editTextNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/edittext")
                .get(0);
        assertTrue("isAccessibilityHeading not reported to accessibility",
                editTextNode.isHeading());

        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(() ->
                        editText.setAccessibilityHeading(false)),
                filterForEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED),
                DEFAULT_TIMEOUT_MS);
        editTextNode.refresh();
        assertFalse("isAccessibilityHeading not reported to accessibility after update",
                editTextNode.isHeading());
    }

    @MediumTest
    @Presubmit
    @Test
    public void testTooltipTextReportedToAccessibility() {
        final AccessibilityNodeInfo buttonNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/buttonWithTooltip")
                .get(0);
        assertEquals("Tooltip text not reported to accessibility",
                sInstrumentation.getContext().getString(R.string.button_tooltip),
                buttonNode.getTooltipText());
    }

    @MediumTest
    @Test
    public void testTooltipTextActionsReportedToAccessibility() throws Exception {
        final AccessibilityNodeInfo buttonNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/buttonWithTooltip")
                .get(0);
        assertFalse(hasTooltipShowing(R.id.buttonWithTooltip));
        assertThat(ACTION_SHOW_TOOLTIP, in(buttonNode.getActionList()));
        assertThat(ACTION_HIDE_TOOLTIP, not(in(buttonNode.getActionList())));
        sUiAutomation.executeAndWaitForEvent(() -> buttonNode.performAction(
                ACTION_SHOW_TOOLTIP.getId()),
                filterForEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED),
                DEFAULT_TIMEOUT_MS);

        // The button should now be showing the tooltip, so it should have the option to hide it.
        buttonNode.refresh();
        assertThat(ACTION_HIDE_TOOLTIP, in(buttonNode.getActionList()));
        assertThat(ACTION_SHOW_TOOLTIP, not(in(buttonNode.getActionList())));
        assertTrue(hasTooltipShowing(R.id.buttonWithTooltip));
    }

    @MediumTest
    @Test
    public void testTraversalBeforeReportedToAccessibility() throws Exception {
        final AccessibilityNodeInfo buttonNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/buttonWithTooltip")
                .get(0);
        final AccessibilityNodeInfo beforeNode = buttonNode.getTraversalBefore();
        assertThat(beforeNode, notNullValue());
        assertThat(beforeNode.getViewIdResourceName(),
                equalTo("android.accessibilityservice.cts:id/edittext"));

        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(
                () -> mActivity.findViewById(R.id.buttonWithTooltip)
                        .setAccessibilityTraversalBefore(View.NO_ID)),
                filterForEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED),
                DEFAULT_TIMEOUT_MS);

        buttonNode.refresh();
        assertThat(buttonNode.getTraversalBefore(), nullValue());
    }

    @MediumTest
    @Test
    public void testTraversalAfterReportedToAccessibility() throws Exception {
        final AccessibilityNodeInfo editNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/edittext")
                .get(0);
        final AccessibilityNodeInfo afterNode = editNode.getTraversalAfter();
        assertThat(afterNode, notNullValue());
        assertThat(afterNode.getViewIdResourceName(),
                equalTo("android.accessibilityservice.cts:id/buttonWithTooltip"));

        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(
                () -> mActivity.findViewById(R.id.edittext)
                        .setAccessibilityTraversalAfter(View.NO_ID)),
                filterForEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED),
                DEFAULT_TIMEOUT_MS);

        editNode.refresh();
        assertThat(editNode.getTraversalAfter(), nullValue());
    }

    @MediumTest
    @Test
    public void testLabelForReportedToAccessibility() throws Exception {
        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(() -> mActivity
                .findViewById(R.id.edittext).setLabelFor(R.id.buttonWithTooltip)),
                filterForEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED),
                DEFAULT_TIMEOUT_MS);
        // TODO: b/78022650: This code should move above the executeAndWait event. It's here because
        // the a11y cache doesn't get notified when labelFor changes, so the node with the
        // labledBy isn't updated.
        final AccessibilityNodeInfo editNode = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/edittext")
                .get(0);
        editNode.refresh();
        final AccessibilityNodeInfo labelForNode = editNode.getLabelFor();
        assertThat(labelForNode, notNullValue());
        // Labeled node should indicate that it is labeled by the other one
        assertThat(labelForNode.getLabeledBy(), equalTo(editNode));
    }

    @MediumTest
    @Test
    public void testA11yActionTriggerMotionEventActionOutside() throws Exception {
        final View.OnTouchListener listener = mock(View.OnTouchListener.class);
        final AccessibilityNodeInfo button = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/button")
                .get(0);
        final String title = sInstrumentation.getContext().getString(R.string.alert_title);

        // Add a dialog that is watching outside touch
        sUiAutomation.executeAndWaitForEvent(
                () -> sInstrumentation.runOnMainSync(() -> {
                            final AlertDialog dialog = new AlertDialog.Builder(mActivity)
                                    .setTitle(R.string.alert_title)
                                    .setMessage(R.string.alert_message)
                                    .create();
                            final Window window = dialog.getWindow();
                            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                                    | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH);
                            window.getDecorView().setOnTouchListener(listener);
                            window.setTitle(title);
                            dialog.show();
                    }),
                (event) -> {
                    // Ensure the dialog is shown over the activity
                    final AccessibilityWindowInfo dialog = findWindowByTitle(
                            sUiAutomation, title);
                    final AccessibilityWindowInfo activity = findWindowByTitle(
                            sUiAutomation, getActivityTitle(sInstrumentation, mActivity));
                    return (dialog != null && activity != null)
                            && (dialog.getLayer() > activity.getLayer());
                }, DEFAULT_TIMEOUT_MS);

        // Perform an action and wait for an event
        sUiAutomation.executeAndWaitForEvent(
                () -> button.performAction(AccessibilityNodeInfo.ACTION_CLICK),
                filterForEventType(AccessibilityEvent.TYPE_VIEW_CLICKED), DEFAULT_TIMEOUT_MS);

        // Make sure the MotionEvent.ACTION_OUTSIDE is received.
        verify(listener, timeout(DEFAULT_TIMEOUT_MS).atLeastOnce()).onTouch(any(View.class),
                argThat(event -> event.getActionMasked() == MotionEvent.ACTION_OUTSIDE));
    }

    @MediumTest
    @Test
    public void testTouchDelegateInfoReportedToAccessibility() {
        final Button button = getOnMain(sInstrumentation, () -> mActivity.findViewById(
                R.id.button));
        final View parent = (View) button.getParent();
        final Rect rect = new Rect();
        button.getHitRect(rect);
        parent.setTouchDelegate(new TouchDelegate(rect, button));

        final AccessibilityNodeInfo nodeInfo = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByViewId(
                        "android.accessibilityservice.cts:id/buttonLayout")
                .get(0);
        AccessibilityNodeInfo.TouchDelegateInfo targetMapInfo =
                nodeInfo.getTouchDelegateInfo();
        assertNotNull("Did not receive TouchDelegate target map", targetMapInfo);
        assertEquals("Incorrect target map size", 1, targetMapInfo.getRegionCount());
        assertEquals("Incorrect target map region", new Region(rect),
                targetMapInfo.getRegionAt(0));
        final AccessibilityNodeInfo node = targetMapInfo.getTargetForRegion(
                targetMapInfo.getRegionAt(0));
        assertEquals("Incorrect target map view",
                "android.accessibilityservice.cts:id/button",
                node.getViewIdResourceName());
        node.recycle();
    }

    @MediumTest
    @Test
    public void testTouchDelegateWithEbtBetweenView_ReHoverDelegate_FocusTargetAgain()
            throws Throwable {
        mActivity.waitForEnterAnimationComplete();

        final Resources resources = sInstrumentation.getTargetContext().getResources();
        final String buttonResourceName = resources.getResourceName(R.id.button);
        final Button button = mActivity.findViewById(R.id.button);
        final int[] buttonLocation = new int[2];
        button.getLocationOnScreen(buttonLocation);
        final int buttonX = button.getWidth() / 2;
        final int buttonY = button.getHeight() / 2;
        final int hoverY = buttonLocation[1] + buttonY;
        final Button buttonWithTooltip = mActivity.findViewById(R.id.buttonWithTooltip);
        final int[] buttonWithTooltipLocation = new int[2];
        buttonWithTooltip.getLocationOnScreen(buttonWithTooltipLocation);
        final int touchableSize = 48;
        final int hoverRight = buttonWithTooltipLocation[0] + touchableSize / 2;
        final int hoverLeft = buttonLocation[0] + button.getWidth() + touchableSize / 2;
        final int hoverMiddle = (hoverLeft + hoverRight) / 2;
        final View.OnHoverListener listener = CtsMouseUtil.installHoverListener(button, false);
        enableTouchExploration(sInstrumentation, true);

        try {
            // common downTime for touch explorer injected events
            final long downTime = SystemClock.uptimeMillis();
            // hover through delegate, parent, 2nd view, parent and delegate again
            sUiAutomation.executeAndWaitForEvent(
                    () -> injectHoverEvent(downTime, false, hoverLeft, hoverY),
                    filterForEventTypeWithResource(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER,
                            buttonResourceName), DEFAULT_TIMEOUT_MS);
            assertTrue(button.isHovered());
            sUiAutomation.executeAndWaitForEvent(
                    () -> {
                        injectHoverEvent(downTime, true, hoverMiddle, hoverY);
                        injectHoverEvent(downTime, true, hoverRight, hoverY);
                        injectHoverEvent(downTime, true, hoverMiddle, hoverY);
                        injectHoverEvent(downTime, true, hoverLeft, hoverY);
                    },
                    filterForEventTypeWithResource(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER,
                            buttonResourceName), DEFAULT_TIMEOUT_MS);
            // delegate target has a11y focus again
            assertTrue(button.isHovered());

            CtsMouseUtil.clearHoverListener(button);
            View.OnHoverListener verifier = inOrder(listener).verify(listener);
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_ENTER, buttonX, buttonY));
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, buttonX, buttonY));
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, hoverMiddle, buttonY));
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_EXIT, buttonX, buttonY));
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_ENTER, buttonX, buttonY));
            verifier.onHover(eq(button),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, buttonX, buttonY));
        } catch (TimeoutException e) {
            fail("Accessibility events should be received as expected " + e.getMessage());
        } finally {
            enableTouchExploration(sInstrumentation, false);
        }
    }

    @MediumTest
    @Test
    public void testTouchDelegateCoverParentWithEbt_HoverChildAndBack_FocusTargetAgain()
            throws Throwable {
        mActivity.waitForEnterAnimationComplete();

        final int touchableSize = 48;
        final Resources resources = sInstrumentation.getTargetContext().getResources();
        final String targetResourceName = resources.getResourceName(R.id.buttonDelegated);
        final View textView = mActivity.findViewById(R.id.delegateText);
        final Button target = mActivity.findViewById(R.id.buttonDelegated);
        int[] location = new int[2];
        textView.getLocationOnScreen(location);
        final int textX = location[0] + touchableSize/2;
        final int textY = location[1] + textView.getHeight() / 2;
        final int delegateX = location[0] - touchableSize/2;
        final int targetX = target.getWidth() / 2;
        final int targetY = target.getHeight() / 2;
        final View.OnHoverListener listener = CtsMouseUtil.installHoverListener(target, false);
        enableTouchExploration(sInstrumentation, true);

        try {
            final long downTime = SystemClock.uptimeMillis();
            // Like switch bar, it has a text view, a button and a delegate covers parent layout.
            // hover the delegate, text and delegate again.
            sUiAutomation.executeAndWaitForEvent(
                    () -> injectHoverEvent(downTime, false, delegateX, textY),
                    filterForEventTypeWithResource(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER,
                           targetResourceName), DEFAULT_TIMEOUT_MS);
            assertTrue(target.isHovered());
            sUiAutomation.executeAndWaitForEvent(
                    () -> injectHoverEvent(downTime, true, textX, textY),
                    filterForEventTypeWithResource(AccessibilityEvent.TYPE_VIEW_HOVER_EXIT,
                           targetResourceName), DEFAULT_TIMEOUT_MS);
            sUiAutomation.executeAndWaitForEvent(
                    () -> injectHoverEvent(downTime, true, delegateX, textY),
                    filterForEventTypeWithResource(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER,
                           targetResourceName), DEFAULT_TIMEOUT_MS);
            assertTrue(target.isHovered());

            CtsMouseUtil.clearHoverListener(target);
            View.OnHoverListener verifier = inOrder(listener).verify(listener);
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_ENTER, targetX, targetY));
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, targetX, targetY));
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, textX, textY));
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_EXIT, targetX, targetY));
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_ENTER, targetX, targetY));
            verifier.onHover(eq(target),
                    matchHover(MotionEvent.ACTION_HOVER_MOVE, targetX, targetY));
        } catch (TimeoutException e) {
            fail("Accessibility events should be received as expected " + e.getMessage());
        } finally {
            enableTouchExploration(sInstrumentation, false);
        }
    }

    private static void assertPackageName(AccessibilityNodeInfo node, String packageName) {
        if (node == null) {
            return;
        }
        assertEquals(packageName, node.getPackageName());
        final int childCount = node.getChildCount();
        for (int i = 0; i < childCount; i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child != null) {
                assertPackageName(child, packageName);
            }
        }
    }

    private static void enableTouchExploration(Instrumentation instrumentation, boolean enabled)
            throws InterruptedException {
        final int TIMEOUT_FOR_SERVICE_ENABLE = 10000; // millis; 10s
        final Object waitObject = new Object();
        final AtomicBoolean atomicBoolean = new AtomicBoolean(!enabled);
        AccessibilityManager.TouchExplorationStateChangeListener serviceListener = (boolean b) -> {
            synchronized (waitObject) {
                atomicBoolean.set(b);
                waitObject.notifyAll();
            }
        };
        final AccessibilityManager manager =
                (AccessibilityManager) instrumentation.getContext().getSystemService(
                        Service.ACCESSIBILITY_SERVICE);
        manager.addTouchExplorationStateChangeListener(serviceListener);

        final UiAutomation uiAutomation = instrumentation.getUiAutomation();
        final AccessibilityServiceInfo info = uiAutomation.getServiceInfo();
        assert info != null;
        if (enabled) {
            info.flags |= AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
        } else {
            info.flags &= ~AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
        }
        uiAutomation.setServiceInfo(info);

        final long timeoutTime = System.currentTimeMillis() + TIMEOUT_FOR_SERVICE_ENABLE;
        synchronized (waitObject) {
            while ((enabled != atomicBoolean.get()) && (System.currentTimeMillis() < timeoutTime)) {
                waitObject.wait(timeoutTime - System.currentTimeMillis());
            }
        }
        if (enabled) {
            assertTrue("Touch exploration state listener not called when services enabled",
                    atomicBoolean.get());
            assertTrue("Timed out enabling accessibility",
                    manager.isEnabled() && manager.isTouchExplorationEnabled());
        } else {
            assertFalse("Touch exploration state listener not called when services disabled",
                    atomicBoolean.get());
            assertFalse("Timed out disabling accessibility",
                    manager.isEnabled() && manager.isTouchExplorationEnabled());
        }
        manager.removeTouchExplorationStateChangeListener(serviceListener);
    }

    private static MotionEvent matchHover(int action, int x, int y) {
        return argThat(new CtsMouseUtil.PositionMatcher(action, x, y));
    }

    private static void injectHoverEvent(long downTime, boolean isFirstHoverEvent,
            int xOnScreen, int yOnScreen) {
        final long eventTime = isFirstHoverEvent ? SystemClock.uptimeMillis() : downTime;
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_HOVER_MOVE,
                xOnScreen, yOnScreen, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        sUiAutomation.injectInputEvent(event, true);
        event.recycle();
    }

    private AppWidgetProviderInfo getAppWidgetProviderInfo() {
        final ComponentName componentName = new ComponentName(
                "foo.bar.baz", "foo.bar.baz.MyAppWidgetProvider");
        final List<AppWidgetProviderInfo> providers = getAppWidgetManager().getInstalledProviders();
        final int providerCount = providers.size();
        for (int i = 0; i < providerCount; i++) {
            final AppWidgetProviderInfo provider = providers.get(i);
            if (componentName.equals(provider.provider)
                    && Process.myUserHandle().equals(provider.getProfile())) {
                return provider;
            }
        }
        return null;
    }

    private void grantBindAppWidgetPermission() throws Exception {
        ShellCommandBuilder.execShellCommand(sUiAutomation,
                GRANT_BIND_APP_WIDGET_PERMISSION_COMMAND + getCurrentUser());
    }

    private void revokeBindAppWidgetPermission() throws Exception {
        ShellCommandBuilder.execShellCommand(sUiAutomation,
                REVOKE_BIND_APP_WIDGET_PERMISSION_COMMAND + getCurrentUser());
    }

    private AppWidgetManager getAppWidgetManager() {
        return (AppWidgetManager) sInstrumentation.getTargetContext()
                .getSystemService(Context.APPWIDGET_SERVICE);
    }

    private boolean hasAppWidgets() {
        return sInstrumentation.getTargetContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_APP_WIDGETS);
    }

    /**
     * Compares all properties of the <code>first</code> and the
     * <code>second</code>.
     */
    private boolean equalsAccessiblityEvent(AccessibilityEvent first, AccessibilityEvent second) {
         return first.getEventType() == second.getEventType()
            && first.isChecked() == second.isChecked()
            && first.getCurrentItemIndex() == second.getCurrentItemIndex()
            && first.isEnabled() == second.isEnabled()
            && first.getFromIndex() == second.getFromIndex()
            && first.getItemCount() == second.getItemCount()
            && first.isPassword() == second.isPassword()
            && first.getRemovedCount() == second.getRemovedCount()
            && first.isScrollable()== second.isScrollable()
            && first.getToIndex() == second.getToIndex()
            && first.getRecordCount() == second.getRecordCount()
            && first.getScrollX() == second.getScrollX()
            && first.getScrollY() == second.getScrollY()
            && first.getAddedCount() == second.getAddedCount()
            && TextUtils.equals(first.getBeforeText(), second.getBeforeText())
            && TextUtils.equals(first.getClassName(), second.getClassName())
            && TextUtils.equals(first.getContentDescription(), second.getContentDescription())
            && equalsNotificationAsParcelableData(first, second)
            && equalsText(first, second);
    }

    /**
     * Compares the {@link android.os.Parcelable} data of the
     * <code>first</code> and <code>second</code>.
     */
    private boolean equalsNotificationAsParcelableData(AccessibilityEvent first,
            AccessibilityEvent second) {
        Notification firstNotification = (Notification) first.getParcelableData();
        Notification secondNotification = (Notification) second.getParcelableData();
        if (firstNotification == null) {
            return (secondNotification == null);
        } else if (secondNotification == null) {
            return false;
        }
        return TextUtils.equals(firstNotification.tickerText, secondNotification.tickerText);
    }

    /**
     * Compares the text of the <code>first</code> and <code>second</code> text.
     */
    private boolean equalsText(AccessibilityEvent first, AccessibilityEvent second) {
        List<CharSequence> firstText = first.getText();
        List<CharSequence> secondText = second.getText();
        if (firstText.size() != secondText.size()) {
            return false;
        }
        Iterator<CharSequence> firstIterator = firstText.iterator();
        Iterator<CharSequence> secondIterator = secondText.iterator();
        for (int i = 0; i < firstText.size(); i++) {
            if (!firstIterator.next().toString().equals(secondIterator.next().toString())) {
                return false;
            }
        }
        return true;
    }

    private boolean hasTooltipShowing(int id) {
        return getOnMain(sInstrumentation, () -> {
            final View viewWithTooltip = mActivity.findViewById(id);
            if (viewWithTooltip == null) {
                return false;
            }
            final View tooltipView = viewWithTooltip.getTooltipView();
            return (tooltipView != null) && (tooltipView.getParent() != null);
        });
    }

    private static int getCurrentUser() {
        return android.os.Process.myUserHandle().getIdentifier();
    }
}
