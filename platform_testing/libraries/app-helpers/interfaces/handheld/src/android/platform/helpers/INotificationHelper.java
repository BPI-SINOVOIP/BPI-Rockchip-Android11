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

package android.platform.helpers;

import android.app.Notification;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiObject2;

import androidx.annotation.Nullable;

/** An App Helper interface for the Notification. */
public interface INotificationHelper extends IAppHelper {

    String UI_PACKAGE_NAME_SYSUI = "com.android.systemui";
    String UI_PACKAGE_NAME_ANDROID = "android";
    String UI_NOTIFICATION_ID = "status_bar_latest_event_content";
    String NOTIFICATION_TITLE_TEXT = "TEST NOTIFICATION";
    String NOTIFICATION_CONTENT_TEXT = "Test notification content";
    String NOTIFICATION_BIG_TEXT = "lorem ipsum dolor sit amet\n"
            + "lorem ipsum dolor sit amet\n"
            + "lorem ipsum dolor sit amet\n"
            + "lorem ipsum dolor sit amet";
    String NOTIFICATION_CHANNEL_NAME = "Test Channel";
    String EXPAND_BUTTON_ID = "expand_button";
    String APP_ICON_ID = "icon";

    /**
     * Setup expectations: Notification shade opened.
     *
     * <p>Opens a notification from notification shade.
     *
     * @param index The index of the notification to open.
     */
    default void openNotificationbyIndex(int index) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: None
     *
     * <p>Posts a number of notifications to the device. Successive calls to this should post
     * new notifications to those previously posted. Note that this may fail if the helper has
     * surpassed the system-defined limit for per-package notifications.
     *
     * @param count The number of notifications to post.
     */
    default void postNotifications(int count) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: Shade is open
     *
     * <p>Posts a notification using {@link android.app.Notification.BigTextStyle}.
     *
     * @param pkg App to launch, when clicking on notification.
     */
    default UiObject2 postBigTextNotification(@Nullable String pkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: Shade is open
     *
     * <p>Posts a notification using {@link android.app.Notification.BigPictureStyle}.
     *
     * @param pkg App to launch, when clicking on notification.
     */
    default UiObject2 postBigPictureNotification(String pkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: Shade is open
     *
     * <p>Posts a notification using {@link android.app.Notification.MessagingStyle}.
     *
     * @param pkg App to launch, when clicking on notification.
     */
    default UiObject2 postMessagingStyleNotification(String pkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: Shade is open
     *
     * <p>Posts a conversation notification. This notification is associated with a conversation
     * shortcut and in {@link android.app.Notification.MessagingStyle}.
     *
     * @param pkg App to launch, when clicking on notification.
     */
    default UiObject2 postConversationNotification(String pkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: None
     *
     * <p>Posts a number of notifications to the device with a package to launch. Successive calls
     * to this should post new notifications in addition to those previously posted. Note that this
     * may fail if the helper has surpassed the system-defined limit for per-package notifications.
     *
     * @param count The number of notifications to post.
     * @param pkg The application that will be launched by notifications.
     */
    default void postNotifications(int count, String pkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: None
     *
     * <p>Posts a number of notifications to the device with a package to launch. Successive calls
     * to this should post new notifications in addition to those previously posted. Note that this
     * may fail if the helper has surpassed the system-defined limit for per-package notifications.
     *
     * @param count The number of notifications to post.
     * @param pkg The application that will be launched by notifications.
     * @param interrupting If notification should make sounds and be on top section of the shade.
     */
    default void postNotifications(int count, String pkg, boolean interrupting) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: None
     *
     * <p>Posts notification.
     *
     * @param builder Builder for notification to post.
     */
    default void postNotification(Notification.Builder builder) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup Expectations: None
     *
     * <p>Cancel any notifications posted by this helper.
     */
    default void cancelNotifications() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Notification shade opened.
     *
     * <p>Opens the first notification by the specified title and checks if the expected application
     * is in foreground or not
     *
     * @param title The title of the notification to open.
     * @param expectedPkg The foreground application after opening a notification. Won't check the
     *     foreground application if the application is null
     */
    default void openNotificationByTitle(String title, String expectedPkg) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Notification shade opened.
     *
     * <p>Taps the chevron or swipes down on the specified notification and checks if the
     * expanded view contains the expected text.
     *
     * @param notification Notification that should be expanded.
     * @param dragging By swiping down when {@code true}, by tapping the chevron otherwise.
     */
    default void expandNotification(UiObject2 notification, boolean dragging) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Long press on notification to show its hidden menu (a.k.a. guts)
     *
     * @param notification Notification.
     */
    default void showGuts(UiObject2 notification) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Taps the "Done" button on the notification guts.
     *
     * @param notification Notification.
     */
    default void hideGuts(UiObject2 notification) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Notification shade opened.
     *
     * <p>Find the screenshot notification; expand the notification if it's collapsed and click on
     * the "share" button.
     */
    default void shareScreenshotFromNotification(BySelector pageSelector) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the expanding notification screen.
     *
     * <p>Get the UiObject2 of expanding notification screen.
     */
    default UiObject2 getNotificationShadeScrollContainer() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Scroll feeds on Notifications screen
     *
     * <p>Setup expectations: Notification is open with lots of notifications.
     *
     * @param container The container with scrollable elements.
     * @param dir The direction of the fling, must be UP or DOWN.
     */
    default void flingFeed(UiObject2 container, Direction dir) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Scroll feeds on Notifications screen
     *
     * <p>Setup expectations: Notification is open with lots of notifications.
     *
     * @param container The container with scrollable elements.
     * @param dir The direction of the scroll, must be UP or DOWN.
     * @param speed The speed of fling.
     */
    default void flingFeed(UiObject2 container, Direction dir, int speed) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
