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
package com.android.cts.verifier.notifications;

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.content.Intent.ACTION_VIEW;
import static android.view.View.INVISIBLE;
import static android.view.View.VISIBLE;

import android.annotation.NonNull;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Person;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Color;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.ArraySet;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Set;

/**
 * Bubble notification tests: This test checks the behaviour of notifications that have a bubble.
 */
public class BubblesVerifierActivity extends PassFailButtons.Activity {

    private static final String CHANNEL_ID = "BubblesVerifierChannel";
    private static final int NOTIFICATION_ID = 1;
    private static final String SHORTCUT_ID = "BubblesVerifierShortcut";
    private static final String SHORTCUT_CATEGORY =
            "com.android.cts.verifier.notifications.SHORTCUT_CATEGORY";

    private NotificationManager mNotificationManager;
    private ShortcutManager mShortcutManager;

    private TextView mTestTitle;
    private TextView mTestDescription;
    private Button mTestAction;
    private ViewGroup mTestStepButtonLayout;
    private Button mTestStepPassed;
    private Button mTestStepFailed;

    private ArrayList<BubblesTestStep> mTests = new ArrayList<>();
    private int mCurrentTestIndex = -1; // gets incremented first time
    private int mStepFailureCount = 0;

    private abstract class BubblesTestStep {

        /** The title of the test step. */
        public abstract int getTestTitle();

        /** What the tester should do & look for to verify this step was successful. */
        public abstract int getTestDescription();

        /**
         * Text of the button that performs the action for this test (e.g. send bubble). If no
         * button is necessary this will return 0.
         */
        public int getButtonText() {
            return 0;
        }

        /** Code to run when the button is activated; only used if {@link #getButtonText()} != 0 */
        public void performTestAction() {
            // optional
        }
    }

    @Override
    protected void onCreate(Bundle savedState) {
        super.onCreate(savedState);

        setContentView(R.layout.bubbles_main);

        mTestTitle = findViewById(R.id.bubble_test_title);
        mTestDescription = findViewById(R.id.bubble_test_description);
        mTestAction = findViewById(R.id.bubble_test_button);
        mTestStepButtonLayout = findViewById(R.id.button_layout);
        mTestStepPassed = findViewById(R.id.test_step_passed);
        mTestStepFailed = findViewById(R.id.test_step_failed);

        mTestStepPassed.setOnClickListener((v) -> runNextTestOrShowSummary());
        mTestStepFailed.setOnClickListener((v) -> {
            mStepFailureCount++;
            runNextTestOrShowSummary();
        });

        // Make sure they're enabled
        mTests.add(new EnableBubbleTest());

        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        if (am.isLowRamDevice()) {
            // Bubbles don't occur on low ram, instead they just show as notifs so test that
            mTests.add(new LowRamBubbleTest());
        } else {
            mTests.add(new SendBubbleTest());
            mTests.add(new SuppressNotifTest());
            mTests.add(new AddNotifTest());
            mTests.add(new RemoveMetadataTest());
            mTests.add(new AddMetadataTest());
            mTests.add(new ExpandBubbleTest());
            mTests.add(new DismissBubbleTest());
            mTests.add(new DismissNotificationTest());
            mTests.add(new AutoExpandBubbleTest());
        }

        setPassFailButtonClickListeners();

        // Pass is are enabled when all the steps are done & succeeded
        getPassButton().setEnabled(false);

        // Sets the text in the dialog
        setInfoResources(R.string.bubbles_notification_title,
                R.string.bubbles_notification_description, -1);

        mNotificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(
                new NotificationChannel(CHANNEL_ID, CHANNEL_ID, IMPORTANCE_DEFAULT));

        createShortcut();

        runNextTestOrShowSummary();
    }

    private void runNextTestOrShowSummary() {
        if (mCurrentTestIndex + 1 >= mTests.size()) {
            updateViewForCompletionSummary();
        } else {
            mCurrentTestIndex++;
            BubblesTestStep currentTest = mTests.get(mCurrentTestIndex);
            updateViewForTest(currentTest);
        }
    }

    /** Populates the UI based on the provided test step */
    private void updateViewForTest(BubblesTestStep test) {
        mTestStepButtonLayout.setVisibility(VISIBLE);
        mTestTitle.setText(test.getTestTitle());
        mTestDescription.setText(test.getTestDescription());

        if (test.getButtonText() != 0) {
            // Can't pass until test action has run
            mTestStepPassed.setEnabled(false);

            // Set up test action
            mTestAction.setOnClickListener((v) -> {
                test.performTestAction();
                mTestStepPassed.setEnabled(true);
            });
            mTestAction.setText(test.getButtonText());
            mTestAction.setVisibility(VISIBLE);
        } else {
            // No test action required
            mTestAction.setOnClickListener(null);
            mTestAction.setVisibility(INVISIBLE);
            mTestStepPassed.setEnabled(true);
        }
    }

    /** Populates the UI indicating results of test & updates test buttons as needed */
    private void updateViewForCompletionSummary() {
        // No longer need any of these buttons
        mTestStepButtonLayout.setVisibility(INVISIBLE);
        mTestAction.setVisibility(INVISIBLE);

        boolean didEverythingSucceed = mStepFailureCount == 0;
        int totalTests = mTests.size();
        int totalPasses = totalTests - mStepFailureCount;

        mTestTitle.setText(R.string.bubbles_test_summary_title);
        mTestDescription.setText(getString(R.string.bubbles_test_summary, totalPasses, totalTests));

        if (didEverythingSucceed) {
            getPassButton().setEnabled(true);
        }
    }

    /** Creates a shortcut to use for the notifications to be considered conversations */
    private void createShortcut() {
        Context context = getApplicationContext();
        Intent intent = new Intent(context, BubbleActivity.class);
        intent.setAction(ACTION_VIEW);
        Person person = new Person.Builder()
                .setBot(false)
                .setIcon(Icon.createWithResource(context, R.drawable.ic_android))
                .setName("BubbleBot")
                .setImportant(true)
                .build();
        Set<String> categorySet = new ArraySet<>();
        categorySet.add(SHORTCUT_CATEGORY);
        ShortcutInfo shortcut = new ShortcutInfo.Builder(context, SHORTCUT_ID)
                .setShortLabel("BubbleChat")
                .setLongLived(true)
                .setPerson(person)
                .setCategories(categorySet)
                .setIcon(Icon.createWithResource(context, R.drawable.ic_android))
                .setIntent(intent)
                .build();
        ShortcutManager shortcutManager = (ShortcutManager) getSystemService(
                Context.SHORTCUT_SERVICE);
        shortcutManager.addDynamicShortcuts(Arrays.asList(shortcut));
    }

    private class EnableBubbleTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_enable_bubbles_button;
        }


        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_enable_bubbles_title;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_enable_bubbles_verify;
        }

        @Override
        public void performTestAction() {
            final String packageName = getApplicationContext().getPackageName();
            final int appUid = getApplicationInfo().uid;
            final Intent intent = new Intent(Settings.ACTION_APP_NOTIFICATION_BUBBLE_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, packageName);
            intent.putExtra(Settings.EXTRA_APP_UID, appUid);
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
        }
    }

    private class SendBubbleTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_1;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_1;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_1;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "1: SendBubbleTest");
            builder.setBubbleMetadata(getIntentBubble().build());

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class SuppressNotifTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_2;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_2;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_2;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "2: SuppressNotifTest");

            Notification.BubbleMetadata metadata = getIntentBubble()
                    .setSuppressNotification(true)
                    .build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class AddNotifTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_3;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_3;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_3;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "3: AddNotifTest");

            Notification.BubbleMetadata metadata = getIntentBubble()
                    .setSuppressNotification(false)
                    .build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class RemoveMetadataTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_4;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_4;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_4;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "4: RemoveMetadataTest");
            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class AddMetadataTest extends BubblesTestStep {

        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_5;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_5;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_5;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "5: AddMetadataTest");

            Notification.BubbleMetadata metadata = getIntentBubble().build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class ExpandBubbleTest extends BubblesTestStep {
        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_6;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_6;
        }
    }

    private class DismissBubbleTest extends BubblesTestStep {
        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_7;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_7;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_7;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "7: DismissBubbleTest");

            Notification.BubbleMetadata metadata = getIntentBubble().build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class DismissNotificationTest extends BubblesTestStep {
        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_8;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_8;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_8;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification",
                            "8: DismissNotificationTest: Dismiss me!!");

            Notification.BubbleMetadata metadata = getIntentBubble().build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class AutoExpandBubbleTest extends BubblesTestStep {
        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_9;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_9;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_9;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "9: Auto expanded bubble");

            Notification.BubbleMetadata metadata =
                    getIntentBubble().setAutoExpandBubble(true).build();
            builder.setBubbleMetadata(metadata);

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    private class LowRamBubbleTest extends BubblesTestStep {
        @Override
        public int getButtonText() {
            return R.string.bubbles_notification_test_button_10;
        }

        @Override
        public int getTestTitle() {
            return R.string.bubbles_notification_test_title_10;
        }

        @Override
        public int getTestDescription() {
            return R.string.bubbles_notification_test_verify_10;
        }

        @Override
        public void performTestAction() {
            Notification.Builder builder =
                    getConversationNotif("Bubble notification", "Low ram test");
            builder.setBubbleMetadata(getIntentBubble().build());

            mNotificationManager.notify(NOTIFICATION_ID, builder.build());
        }
    }

    /** Creates a minimally filled out {@link android.app.Notification.BubbleMetadata.Builder} */
    private Notification.BubbleMetadata.Builder getIntentBubble() {
        Context context = getApplicationContext();
        Intent intent = new Intent(context, BubbleActivity.class);
        final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        return new Notification.BubbleMetadata.Builder(pendingIntent,
                Icon.createWithResource(getApplicationContext(),
                        R.drawable.ic_android));
    }

    /** Creates a {@link Notification.Builder} that is a conversation. */
    private Notification.Builder getConversationNotif(@NonNull CharSequence title,
            @NonNull CharSequence content) {
        Context context = getApplicationContext();
        Intent intent = new Intent(context, BubbleActivity.class);
        final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        Person person = new Person.Builder()
                .setName("bubblebot")
                .build();
        RemoteInput remoteInput = new RemoteInput.Builder("reply_key").setLabel("reply").build();
        PendingIntent inputIntent = PendingIntent.getActivity(getApplicationContext(), 0,
                new Intent(), 0);
        Icon icon = Icon.createWithResource(getApplicationContext(), R.drawable.ic_android);
        Notification.Action replyAction = new Notification.Action.Builder(icon, "Reply",
                inputIntent).addRemoteInput(remoteInput)
                .build();

        return new Notification.Builder(context, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_android)
                .setContentTitle(title)
                .setContentText(content)
                .setColor(Color.GREEN)
                .setShortcutId(SHORTCUT_ID)
                .setContentIntent(pendingIntent)
                .setActions(replyAction)
                .setStyle(new Notification.MessagingStyle(person)
                        .setConversationTitle("Bubble Chat")
                        .addMessage("Hello?",
                                SystemClock.currentThreadTimeMillis() - 300000, person)
                        .addMessage("Is it me you're looking for?",
                                SystemClock.currentThreadTimeMillis(), person)
                );
    }
}
