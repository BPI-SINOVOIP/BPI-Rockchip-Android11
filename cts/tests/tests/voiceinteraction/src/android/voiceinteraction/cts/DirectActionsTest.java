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
 * See the License for the specific language governing permissions andf
 * limitations under the License.
 */

package android.voiceinteraction.cts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.DirectAction;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.voiceinteraction.common.Utils;

import com.android.compatibility.common.util.ThrowingRunnable;

import org.junit.Test;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Tests for the direction action related functions.
 */
public class DirectActionsTest extends AbstractVoiceInteractionTestCase {
    private static final String TAG = DirectActionsTest.class.getSimpleName();
    private static final long OPERATION_TIMEOUT_MS = 5000;

    private final @NonNull SessionControl mSessionControl = new SessionControl();
    private final @NonNull ActivityControl mActivityControl = new ActivityControl();

    @Test
    public void testPerformDirectAction() throws Exception {
        mActivityControl.startActivity();
        mSessionControl.startVoiceInteractionSession();
        try {
            // Get the actions.
            final List<DirectAction> actions = mSessionControl.getDirectActions();
            Log.v(TAG, "actions: " + actions);

            // Only the expected action should be reported
            final DirectAction action = getExpectedDirectActionAssertively(actions);

            // Perform the expected action.
            final Bundle result = mSessionControl.performDirectAction(action,
                    createActionArguments());

            // Assert the action completed successfully.
            assertActionSucceeded(result);
        } finally {
            mSessionControl.stopVoiceInteractionSession();
            mActivityControl.finishActivity();
        }
    }

    @AppModeFull(reason = "testPerformDirectAction() is enough")
    @Test
    public void testCancelPerformedDirectAction() throws Exception {
        mActivityControl.startActivity();
        mSessionControl.startVoiceInteractionSession();
        try {
            // Get the actions.
            final List<DirectAction> actions = mSessionControl.getDirectActions();
            Log.v(TAG, "actions: " + actions);

            // Only the expected action should be reported
            final DirectAction action = getExpectedDirectActionAssertively(actions);

            // Perform the expected action.
            final Bundle result = mSessionControl.performDirectActionAndCancel(action,
                    createActionArguments());

            // Assert the action was cancelled.
            assertActionCancelled(result);
        } finally {
            mSessionControl.stopVoiceInteractionSession();
            mActivityControl.finishActivity();
        }
    }

    @AppModeFull(reason = "testPerformDirectAction() is enough")
    @Test
    public void testVoiceInteractorDestroy() throws Exception {
        mActivityControl.startActivity();
        mSessionControl.startVoiceInteractionSession();
        try {
            // Get the actions to set up the VoiceInteractor
            mSessionControl.getDirectActions();

            assertThat(mActivityControl
                    .detectInteractorDestroyed(() -> mSessionControl.stopVoiceInteractionSession()))
                            .isTrue();
        } finally {
            mSessionControl.stopVoiceInteractionSession();
            mActivityControl.finishActivity();
        }
    }

    @AppModeFull(reason = "testPerformDirectAction() is enough")
    @Test
    public void testNotifyDirectActionsChanged() throws Exception {
        mActivityControl.startActivity();
        mSessionControl.startVoiceInteractionSession();
        try {
            // Get the actions to set up the VoiceInteractor
            mSessionControl.getDirectActions();

            assertThat(mSessionControl.detectDirectActionsInvalidated(
                    () -> mActivityControl.invalidateDirectActions())).isTrue();
        } finally {
            mSessionControl.stopVoiceInteractionSession();
            mActivityControl.finishActivity();
        }
    }
    private final class SessionControl {
        private @Nullable RemoteCallback mControl;

        private void startVoiceInteractionSession() throws Exception {
            final CountDownLatch latch = new CountDownLatch(1);

            final RemoteCallback callback = new RemoteCallback((result) -> {
                mControl = result.getParcelable(Utils.DIRECT_ACTIONS_KEY_CONTROL);
                latch.countDown();
            });

            final Intent intent = new Intent();
            intent.putExtra(Utils.DIRECT_ACTIONS_KEY_CLASS,
                    "android.voiceinteraction.service.DirectActionsSession");
            intent.setClassName("android.voiceinteraction.service",
                    "android.voiceinteraction.service.VoiceInteractionMain");
            intent.putExtra(Utils.DIRECT_ACTIONS_KEY_CALLBACK, callback);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            Log.v(TAG, "startVoiceInteractionSession(): " + intent);
            mContext.startActivity(intent);

            if (!latch.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new TimeoutException("actitvity not started in " + OPERATION_TIMEOUT_MS
                        + "ms");
            }
        }

        private void stopVoiceInteractionSession() throws Exception {
            executeCommand(Utils.DIRECT_ACTIONS_SESSION_CMD_FINISH,
                    null /*directAction*/, null /*arguments*/, null /*postActionCommand*/);
        }

        @Nullable List<DirectAction> getDirectActions() throws Exception {
            final ArrayList<DirectAction> actions = new ArrayList<>();
            final Bundle result = executeCommand(Utils.DIRECT_ACTIONS_SESSION_CMD_GET_ACTIONS,
                    null /*directAction*/, null /*arguments*/, null /*postActionCommand*/);
            actions.addAll(result.getParcelableArrayList(Utils.DIRECT_ACTIONS_KEY_RESULT));
            return actions;
        }

        @Nullable Bundle performDirectAction(@NonNull DirectAction directAction,
                @NonNull Bundle arguments) throws Exception {
            return executeCommand(Utils.DIRECT_ACTIONS_SESSION_CMD_PERFORM_ACTION,
                    directAction, arguments, null /*postActionCommand*/);
        }

        @Nullable Bundle performDirectActionAndCancel(@NonNull DirectAction directAction,
                @NonNull Bundle arguments) throws Exception {
            return executeCommand(Utils.DIRECT_ACTIONS_SESSION_CMD_PERFORM_ACTION_CANCEL,
                    directAction, arguments, null /*postActionCommand*/);
        }

        @Nullable
        boolean detectDirectActionsInvalidated(@NonNull ThrowingRunnable postActionCommand)
                throws Exception {
            final Bundle result = executeCommand(
                    Utils.DIRECT_ACTIONS_SESSION_CMD_DETECT_ACTIONS_CHANGED,
                    null /*directAction*/, null /*arguments*/, postActionCommand);
            return result.getBoolean(Utils.DIRECT_ACTIONS_KEY_RESULT);
        }

        @Nullable Bundle executeCommand(@NonNull String action, @Nullable DirectAction directAction,
                @Nullable Bundle arguments, @Nullable ThrowingRunnable postActionCommand)
                throws Exception {
            final CountDownLatch latch = new CountDownLatch(1);

            final Bundle result = new Bundle();

            final RemoteCallback callback = new RemoteCallback((b) -> {
                result.putAll(b);
                latch.countDown();
            });

            final Bundle command = new Bundle();
            command.putString(Utils.DIRECT_ACTIONS_KEY_COMMAND, action);
            command.putParcelable(Utils.DIRECT_ACTIONS_KEY_ACTION, directAction);
            command.putBundle(Utils.DIRECT_ACTIONS_KEY_ARGUMENTS, arguments);
            command.putParcelable(Utils.DIRECT_ACTIONS_KEY_CALLBACK, callback);

            Log.v(TAG, "executeCommand(): action=" + action + " command="
                    + Utils.toBundleString(command));
            mControl.sendResult(command);

            if (postActionCommand != null) {
                Log.v(TAG, "Executing post-action command for " + action);
                postActionCommand.run();
            }

            if (!latch.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new TimeoutException("result not received in " + OPERATION_TIMEOUT_MS + "ms");
            }

            Log.v(TAG, "returning " + Utils.toBundleString(result));

            return result;
        }
    }

    private final class ActivityControl {
        private @Nullable RemoteCallback mControl;

        void startActivity() throws Exception {
            final CountDownLatch latch = new CountDownLatch(1);

            final RemoteCallback callback = new RemoteCallback((result) -> {
                Log.v(TAG, "ActivityControl: testapp called the callback: "
                        + Utils.toBundleString(result));
                mControl = result.getParcelable(Utils.DIRECT_ACTIONS_KEY_CONTROL);
                latch.countDown();
            });

            final Intent intent = new Intent()
                    .setAction(Intent.ACTION_VIEW)
                    .addCategory(Intent.CATEGORY_BROWSABLE)
                    .setData(Uri.parse("https://android.voiceinteraction.testapp"
                            + "/DirectActionsActivity"))
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(Utils.DIRECT_ACTIONS_KEY_CALLBACK, callback);

            if (!mContext.getPackageManager().isInstantApp()) {
                intent.setPackage("android.voiceinteraction.testapp");
            }

            Log.v(TAG, "startActivity: " + intent);

            mContext.startActivity(intent);

            if (!latch.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new TimeoutException("actitvity not started in " + OPERATION_TIMEOUT_MS
                        + "ms");
            }
        }

        private boolean detectInteractorDestroyed(ThrowingRunnable destroyTrigger)
                throws Exception {
            final Bundle result = executeRemoteCommand(
                    Utils.DIRECT_ACTIONS_ACTIVITY_CMD_DESTROYED_INTERACTOR,
                    destroyTrigger);
            return result.getBoolean(Utils.DIRECT_ACTIONS_KEY_RESULT);
        }

        void finishActivity() throws Exception {
            executeRemoteCommand(Utils.DIRECT_ACTIONS_ACTIVITY_CMD_FINISH);
        }

        void invalidateDirectActions() throws Exception {
            executeRemoteCommand(Utils.DIRECT_ACTIONS_ACTIVITY_CMD_INVALIDATE_ACTIONS);
        }

        @NonNull Bundle executeRemoteCommand(@NonNull String action) throws Exception {
            return executeRemoteCommand(action, /* postActionCommand= */ null);
        }

        @NonNull Bundle executeRemoteCommand(@NonNull String action,
                @Nullable ThrowingRunnable postActionCommand) throws Exception {
            final Bundle result = new Bundle();

            final CountDownLatch latch = new CountDownLatch(1);

            final RemoteCallback callback = new RemoteCallback((b) -> {
                Log.v(TAG, "executeRemoteCommand(): received result from '" + action + "': "
                        + Utils.toBundleString(b));
                if (b != null) {
                    result.putAll(b);
                }
                latch.countDown();
            });

            final Bundle command = new Bundle();
            command.putString(Utils.DIRECT_ACTIONS_KEY_COMMAND, action);
            command.putParcelable(Utils.DIRECT_ACTIONS_KEY_CALLBACK, callback);

            Log.v(TAG, "executeRemoteCommand(): sending command for '" + action + "'");
            mControl.sendResult(command);

            if (postActionCommand != null) {
                try {
                    postActionCommand.run();
                } catch (TimeoutException e) {
                    Log.e(TAG, "action '" + action + "' timed out" );
                } catch (Exception e) {
                    throw e;
                }
            }

            if (!latch.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new TimeoutException("result not received in " + OPERATION_TIMEOUT_MS + "ms");
            }
            return result;
        }
    }

    private @NonNull DirectAction getExpectedDirectActionAssertively(
            @Nullable List<DirectAction> actions) {
        assertWithMessage("no actions").that(actions).isNotEmpty();
        final DirectAction action = actions.get(0);
        assertThat(action.getId()).isEqualTo(Utils.DIRECT_ACTIONS_ACTION_ID);
        assertThat(action.getExtras().getString(Utils.DIRECT_ACTION_EXTRA_KEY))
                .isEqualTo(Utils.DIRECT_ACTION_EXTRA_VALUE);
        assertThat(action.getLocusId().getId()).isEqualTo(Utils.DIRECT_ACTIONS_LOCUS_ID.getId());
        return action;
    }

    private @NonNull Bundle createActionArguments() {
        final Bundle args = new Bundle();
        args.putString(Utils.DIRECT_ACTIONS_KEY_ARGUMENTS, Utils.DIRECT_ACTIONS_KEY_ARGUMENTS);
        Log.v(TAG, "createActionArguments(): " + Utils.toBundleString(args));
        return args;
    }

    private void assertActionSucceeded(@NonNull Bundle result) {
        final Bundle bundle = result.getBundle(Utils.DIRECT_ACTIONS_KEY_RESULT);
        final String status = bundle.getString(Utils.DIRECT_ACTIONS_KEY_RESULT);
        assertWithMessage("assertActionSucceeded(%s)", Utils.toBundleString(result))
                .that(Utils.DIRECT_ACTIONS_RESULT_PERFORMED).isEqualTo(status);
    }

    private void assertActionCancelled(@NonNull Bundle result) {
        final Bundle bundle = result.getBundle(Utils.DIRECT_ACTIONS_KEY_RESULT);
        final String status = bundle.getString(Utils.DIRECT_ACTIONS_KEY_RESULT);
        assertWithMessage("assertActionCancelled(%s)", Utils.toBundleString(result))
                .that(Utils.DIRECT_ACTIONS_RESULT_CANCELLED).isEqualTo(status);
    }
}
