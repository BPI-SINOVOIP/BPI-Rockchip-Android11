/*
 * Copyright (C) 2019 The Android Open Source Project
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
package android.app.cts;

import android.app.Notification;
import android.app.Notification.Action;
import android.app.Notification.WearableExtender;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.os.Parcel;
import android.test.AndroidTestCase;
import android.view.Gravity;
import java.util.ArrayList;
import java.util.List;

public class WearableExtenderTest extends AndroidTestCase {

    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getContext();
    }

    public void testWearableExtender() {
        final String bridgeTag = "bridge_tag";
        final String dismissalId = "dismissal_id";
        final int contentActionIndex = 2;
        final Bitmap background = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, new Intent(), 0);
        Notification page1 = new Notification.Builder(mContext, "test id")
            .setSmallIcon(1)
            .setContentTitle("page1")
            .build();
        Notification page2 = new Notification.Builder(mContext, "test id")
            .setSmallIcon(1)
            .setContentTitle("page2")
            .build();
        List<Notification> pages = new ArrayList<>();
        pages.add(page2);
        final int gravity = Gravity.LEFT;
        final int icon = 3;
        final int height = 4;
        final int size = 5;
        final int timeout = 6;

        WearableExtender extender = new WearableExtender()
                .setStartScrollBottom(true)
                .setContentIntentAvailableOffline(true)
                .setHintContentIntentLaunchesActivity(true)
                .setBridgeTag(bridgeTag)
                .setDismissalId(dismissalId)
                .setContentAction(contentActionIndex)
                // deprecated methods follow
                .setBackground(background)
                .setGravity(gravity)
                .setContentIcon(icon)
                .setContentIconGravity(gravity)
                .setCustomContentHeight(height)
                .setCustomSizePreset(size)
                .setDisplayIntent(pendingIntent)
                .setHintAmbientBigPicture(true)
                .setHintAvoidBackgroundClipping(true)
                .setHintHideIcon(true)
                .setHintScreenTimeout(timeout)
                .setHintShowBackgroundOnly(true)
                .addPage(page1)
                .clearPages()
                .addPages(pages);

        assertTrue(extender.getStartScrollBottom());
        assertTrue(extender.getContentIntentAvailableOffline());
        assertTrue(extender.getHintContentIntentLaunchesActivity());
        assertEquals(bridgeTag, extender.getBridgeTag());
        assertEquals(dismissalId, extender.getDismissalId());
        assertEquals(contentActionIndex, extender.getContentAction());
        // deprecated methods follow
        assertEquals(background, extender.getBackground());
        assertEquals(gravity, extender.getGravity());
        assertEquals(icon, extender.getContentIcon());
        assertEquals(gravity, extender.getContentIconGravity());
        assertEquals(height, extender.getCustomContentHeight());
        assertEquals(size, extender.getCustomSizePreset());
        assertEquals(pendingIntent, extender.getDisplayIntent());
        assertTrue(extender.getHintAmbientBigPicture());
        assertTrue(extender.getHintAvoidBackgroundClipping());
        assertTrue(extender.getHintHideIcon());
        assertEquals(timeout, extender.getHintScreenTimeout());
        assertTrue(extender.getHintShowBackgroundOnly());
        assertEquals(1, extender.getPages().size());
        assertEquals(page2, extender.getPages().get(0));
    }

    public void testWearableExtenderActions() {
        Notification.Action a = newActionBuilder().build();
        Notification.Action b = newActionBuilder().build();
        Notification.Action c = newActionBuilder().build();
        List<Action> actions = new ArrayList<>();
        actions.add(b);
        actions.add(c);

        WearableExtender extender = new WearableExtender()
                .addAction(a)
                .addActions(actions)
                .setContentAction(1);

        assertEquals(3, extender.getActions().size());
        assertEquals(b, extender.getActions().get(extender.getContentAction()));
    }

    public void testWearableExtender_clearActions() {
        WearableExtender extender = new WearableExtender()
                .addAction(newActionBuilder().build());

        extender.clearActions();

        assertEquals(0, extender.getActions().size());
    }

    public void testWearableExtender_clone() {
        final String bridgeTag = "bridge_tag";
        final String dismissalId = "dismissal_id";
        final int contentActionIndex = 2;
        WearableExtender original = new WearableExtender()
                .addAction(newActionBuilder().build())
                .setStartScrollBottom(true)
                .setContentIntentAvailableOffline(true)
                .setHintContentIntentLaunchesActivity(true)
                .setBridgeTag(bridgeTag)
                .setDismissalId(dismissalId)
                .setContentAction(contentActionIndex);

        // WHEN the enter is cloned
        WearableExtender clone = original.clone();
        // and WHEN the original is modified
        original.clearActions();

        assertTrue(clone.getStartScrollBottom());
        assertTrue(clone.getContentIntentAvailableOffline());
        assertTrue(clone.getHintContentIntentLaunchesActivity());
        assertEquals(bridgeTag, clone.getBridgeTag());
        assertEquals(dismissalId, clone.getDismissalId());
        assertEquals(contentActionIndex, clone.getContentAction());
        assertEquals(1, clone.getActions().size());
    }

    public void testWearableExtender_extend() {
        final String title = "test_title";
        final String bridgeTag = "bridge_tag";
        final String dismissalId = "dismissal_id";
        final int contentActionIndex = 2;
        Notification.Builder notifBuilder = new Notification.Builder(mContext, "test id")
                .setSmallIcon(1)
                .setContentTitle(title);
        WearableExtender extender = new WearableExtender()
                .addAction(newActionBuilder().build())
                .setStartScrollBottom(true)
                .setContentIntentAvailableOffline(true)
                .setHintContentIntentLaunchesActivity(true)
                .setBridgeTag(bridgeTag)
                .setDismissalId(dismissalId)
                .setContentAction(contentActionIndex);

        extender.extend(notifBuilder);

        WearableExtender result = new WearableExtender(notifBuilder.build());
        assertTrue(result.getStartScrollBottom());
        assertTrue(result.getContentIntentAvailableOffline());
        assertTrue(result.getHintContentIntentLaunchesActivity());
        assertEquals(bridgeTag, result.getBridgeTag());
        assertEquals(dismissalId, result.getDismissalId());
        assertEquals(contentActionIndex, result.getContentAction());
        assertEquals(1, result.getActions().size());
    }

    public void testWriteToParcel() {
        final String bridgeTag = "bridge_tag";
        final String dismissalId = "dismissal_id";
        final int contentActionIndex = 2;
        Notification.Action action = newActionBuilder().build();
        final Bitmap background = Bitmap.createBitmap(10, 10, Config.ARGB_8888);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, new Intent(), 0);
        Notification page1 = new Notification.Builder(mContext, "test id")
            .setSmallIcon(1)
            .setContentTitle("page1")
            .build();
        Notification page2 = new Notification.Builder(mContext, "test id")
            .setSmallIcon(1)
            .setContentTitle("page2")
            .build();
        List<Notification> pages = new ArrayList<>();
        pages.add(page2);
        final int gravity = Gravity.LEFT;
        final int icon = 3;
        final int height = 4;
        final int size = 5;
        final int timeout = 6;

        Notification notif = new Notification.Builder(mContext, "test id")
                .setSmallIcon(1)
                .setContentTitle("test_title")
                .extend(new Notification.WearableExtender()
                        .setStartScrollBottom(true)
                        .setContentIntentAvailableOffline(true)
                        .setHintContentIntentLaunchesActivity(true)
                        .setBridgeTag(bridgeTag)
                        .setDismissalId(dismissalId)
                        .addAction(action)
                        .setContentAction(contentActionIndex)
                        // deprecated methods follow
                        .setBackground(background)
                        .setGravity(gravity)
                        .setContentIcon(icon)
                        .setContentIconGravity(gravity)
                        .setCustomContentHeight(height)
                        .setCustomSizePreset(size)
                        .setDisplayIntent(pendingIntent)
                        .setHintAmbientBigPicture(true)
                        .setHintAvoidBackgroundClipping(true)
                        .setHintHideIcon(true)
                        .setHintScreenTimeout(timeout)
                        .setHintShowBackgroundOnly(true)
                        .addPage(page1))
                .build();

        Parcel parcel = Parcel.obtain();
        notif.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        Notification result = new Notification(parcel);

        WearableExtender extender = new WearableExtender(result);
        assertTrue(extender.getStartScrollBottom());
        assertTrue(extender.getContentIntentAvailableOffline());
        assertTrue(extender.getHintContentIntentLaunchesActivity());
        assertEquals(bridgeTag, extender.getBridgeTag());
        assertEquals(dismissalId, extender.getDismissalId());
        assertEquals(contentActionIndex, extender.getContentAction());
        assertEquals(1, extender.getActions().size());
        // deprecated methods follow
        assertNotNull(extender.getBackground());
        assertEquals(gravity, extender.getGravity());
        assertEquals(icon, extender.getContentIcon());
        assertEquals(gravity, extender.getContentIconGravity());
        assertEquals(height, extender.getCustomContentHeight());
        assertEquals(size, extender.getCustomSizePreset());
        assertEquals(pendingIntent, extender.getDisplayIntent());
        assertTrue(extender.getHintAmbientBigPicture());
        assertTrue(extender.getHintAvoidBackgroundClipping());
        assertTrue(extender.getHintHideIcon());
        assertEquals(timeout, extender.getHintScreenTimeout());
        assertTrue(extender.getHintShowBackgroundOnly());
        assertEquals(1, extender.getPages().size());
    }

    private static Notification.Action.Builder newActionBuilder() {
        return new Notification.Action.Builder(0, "title", null);
    }

    /** Notification.Action.WearableExtender functions */

    public void testActionWearableExtender_constructor() {
        // should not throw exception
        Action.WearableExtender extender = new Action.WearableExtender();
    }

    public void testActionWearableExtender_constructor_copy() {
        Action.WearableExtender extender = new Action.WearableExtender();

        // set a flag to ensure flags are copied correctly
        extender.setAvailableOffline(false);

        // deprecated setters
        extender.setInProgressLabel("inProgress");
        extender.setConfirmLabel("confirm");
        extender.setCancelLabel("cancel");

        Action action = extender.extend(newActionBuilder()).build();

        Action.WearableExtender copiedExtender = new Action.WearableExtender(action);

        assertEquals("available offline set via flags", false, copiedExtender.isAvailableOffline());

        // deprecated getters
        assertEquals("has correct progress label", "inProgress", copiedExtender.getInProgressLabel());
        assertEquals("has correct confirm label", "confirm", copiedExtender.getConfirmLabel());
        assertEquals("has correct cancel label", "cancel", copiedExtender.getCancelLabel());
    }

    public void testActionWearableExtender_clone() {
        Action.WearableExtender extender = new Action.WearableExtender();

        // set a flag to make sure flags are copied correctly
        extender.setAvailableOffline(false);

        // deprecated setters
        extender.setInProgressLabel("inProgress");
        extender.setConfirmLabel("confirm");
        extender.setCancelLabel("cancel");

        Action.WearableExtender copiedExtender = extender.clone();

        assertEquals("available offline set via flags", false, copiedExtender.isAvailableOffline());

        // deprecated getters
        assertEquals("has correct progress label", "inProgress", copiedExtender.getInProgressLabel());
        assertEquals("has correct confirm label", "confirm", copiedExtender.getConfirmLabel());
        assertEquals("has correct cancel label", "cancel", copiedExtender.getCancelLabel());
    }

    public void testActionWearableExtender_setAvailableOffline() {
        Action.WearableExtender extender = new Action.WearableExtender();

        assertEquals("available offline", true, extender.isAvailableOffline());

        extender.setAvailableOffline(false);

        assertEquals("not available offline", false, extender.isAvailableOffline());
    }

    public void testActionWearableExtender_setHintLaunchesActivity() {
        Action.WearableExtender extender = new Action.WearableExtender();

        assertEquals("action will not launch activity", false, extender.getHintLaunchesActivity());

        extender.setHintLaunchesActivity(true);

        assertEquals("action will launch activity", true, extender.getHintLaunchesActivity());
    }

    public void testActionWearableExtender_setHintDisplayActionInline() {
        Action.WearableExtender extender = new Action.WearableExtender();

        assertEquals("action not displayed inline", false, extender.getHintDisplayActionInline());

        extender.setHintDisplayActionInline(true);

        assertEquals("action will display inline", true, extender.getHintDisplayActionInline());
    }

    // deprecated
    public void testActionWearableExtender_setInProgressLabel() {
        Action.WearableExtender extender = new Action.WearableExtender();

        extender.setInProgressLabel("progressing");

        assertEquals("has progress label", "progressing", extender.getInProgressLabel());
    }

    // deprecated
    public void testActionWearableExtender_setCancelLabel() {
        Action.WearableExtender extender = new Action.WearableExtender();

        extender.setCancelLabel("cancelled");

        assertEquals("has cancel label", "cancelled", extender.getCancelLabel());
    }

    // deprecated
    public void testActionWearableExtender_setConfirmLabel() {
        Action.WearableExtender extender = new Action.WearableExtender();

        extender.setConfirmLabel("confirmed");

        assertEquals("has confirm label", "confirmed", extender.getConfirmLabel());
    }
}