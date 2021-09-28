/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ComponentName;
import android.content.Context;
import android.media.MediaController2;
import android.media.MediaMetadata;
import android.media.MediaSession2;
import android.media.Session2Command;
import android.media.Session2CommandGroup;
import android.media.Session2Token;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Tests {@link android.media.MediaController2}.
 */
@RunWith(AndroidJUnit4.class)
@SmallTest
public class MediaController2Test {
    private static final long WAIT_TIME_MS = 100L;

    static final Object sTestLock = new Object();

    static final int ALLOWED_COMMAND_CODE = 100;
    static final Session2CommandGroup SESSION_ALLOWED_COMMANDS = new Session2CommandGroup.Builder()
            .addCommand(new Session2Command(ALLOWED_COMMAND_CODE)).build();
    static final int SESSION_RESULT_CODE = 101;
    static final String SESSION_RESULT_KEY = "test_result_key";
    static final String SESSION_RESULT_VALUE = "test_result_value";
    static final Session2Command.Result SESSION_COMMAND_RESULT;

    private static final String TEST_KEY = "test_key";
    private static final String TEST_VALUE = "test_value";

    static {
        Bundle resultData = new Bundle();
        resultData.putString(SESSION_RESULT_KEY, SESSION_RESULT_VALUE);
        SESSION_COMMAND_RESULT = new Session2Command.Result(SESSION_RESULT_CODE, resultData);
    }

    static Handler sHandler;
    static Executor sHandlerExecutor;

    private Context mContext;
    private Bundle mExtras;
    private MediaSession2 mSession;
    private Session2Callback mSessionCallback;

    @BeforeClass
    public static void setUpThread() {
        synchronized (MediaSession2Test.class) {
            if (sHandler != null) {
                return;
            }
            HandlerThread handlerThread = new HandlerThread("MediaSessionTestBase");
            handlerThread.start();
            sHandler = new Handler(handlerThread.getLooper());
            sHandlerExecutor = (runnable) -> {
                Handler handler;
                synchronized (MediaSession2Test.class) {
                    handler = sHandler;
                }
                if (handler != null) {
                    handler.post(() -> {
                        synchronized (sTestLock) {
                            runnable.run();
                        }
                    });
                }
            };
        }
    }

    @AfterClass
    public static void cleanUpThread() {
        synchronized (MediaSession2Test.class) {
            if (sHandler == null) {
                return;
            }
            sHandler.getLooper().quitSafely();
            sHandler = null;
            sHandlerExecutor = null;
        }
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        mSessionCallback = new Session2Callback();
        mExtras = new Bundle();
        mExtras.putString(TEST_KEY, TEST_VALUE);
        mSession = new MediaSession2.Builder(mContext)
                .setSessionCallback(sHandlerExecutor, mSessionCallback)
                .setExtras(mExtras)
                .build();
    }

    @After
    public void cleanUp() {
        if (mSession != null) {
            mSession.close();
            mSession = null;
        }
    }

    @Test
    public void testBuilder_withIllegalArguments() {
        final Session2Token token = new Session2Token(
                mContext, new ComponentName(mContext, this.getClass()));

        try {
            MediaController2.Builder builder = new MediaController2.Builder(null, token);
            fail("null context shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }

        try {
            MediaController2.Builder builder = new MediaController2.Builder(mContext, null);
            fail("null token shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }

        try {
            MediaController2.Builder builder = new MediaController2.Builder(mContext, token);
            builder.setConnectionHints(null);
            fail("null connectionHints shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }

        try {
            MediaController2.Builder builder = new MediaController2.Builder(mContext, token);
            builder.setControllerCallback(null, new MediaController2.ControllerCallback() {});
            fail("null Executor shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }

        try {
            MediaController2.Builder builder = new MediaController2.Builder(mContext, token);
            builder.setControllerCallback(Executors.newSingleThreadExecutor(), null);
            fail("null ControllerCallback shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }
    }

    @Test
    public void testBuilder_setConnectionHints_withFrameworkParcelable() throws Exception {
        final List<MediaSession2.ControllerInfo> controllerInfoList = new ArrayList<>();
        final CountDownLatch latch = new CountDownLatch(1);

        try (MediaSession2 session = new MediaSession2.Builder(mContext)
                .setId("testBuilder_setConnectionHints_withFrameworkParcelable")
                .setSessionCallback(sHandlerExecutor, new MediaSession2.SessionCallback() {
                    @Override
                    public Session2CommandGroup onConnect(MediaSession2 session,
                            MediaSession2.ControllerInfo controller) {
                        if (controller.getUid() == Process.myUid()) {
                            controllerInfoList.add(controller);
                            latch.countDown();
                            return new Session2CommandGroup.Builder().build();
                        }
                        return null;
                    }
                })
                .build()) {

            final Session2Token frameworkParcelable = new Session2Token(
                    mContext, new ComponentName(mContext, this.getClass()));
            final String testKey = "test_key";

            Bundle connectionHints = new Bundle();
            connectionHints.putParcelable(testKey, frameworkParcelable);

            MediaController2 controller = new MediaController2.Builder(mContext, session.getToken())
                    .setConnectionHints(connectionHints)
                    .build();
            assertTrue(latch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));

            Bundle connectionHintsOut = controllerInfoList.get(0).getConnectionHints();
            assertTrue(connectionHintsOut.containsKey(testKey));
            assertEquals(frameworkParcelable, connectionHintsOut.getParcelable(testKey));
        }
    }

    @Test
    public void testBuilder_setConnectionHints_withCustomParcelable() {
        final Session2Token token = new Session2Token(
                mContext, new ComponentName(mContext, this.getClass()));
        final String testKey = "test_key";
        final MediaSession2Test.CustomParcelable customParcelable =
                new MediaSession2Test.CustomParcelable(1);

        Bundle connectionHints = new Bundle();
        connectionHints.putParcelable(testKey, customParcelable);

        try (MediaController2 controller = new MediaController2.Builder(mContext, token)
                .setConnectionHints(connectionHints)
                .build()) {
            fail("Custom Parcelables shouldn't be accepted!");
        } catch (IllegalArgumentException e) {
            // Expected
        }
    }

    @Test
    public void testCreatingControllerWithoutCallback() throws Exception {
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken()).build()) {
            assertTrue(mSessionCallback.mOnConnectedLatch.await(
                    WAIT_TIME_MS, TimeUnit.MILLISECONDS));
            assertEquals(mContext.getPackageName(),
                    mSessionCallback.mControllerInfo.getPackageName());
        }
    }

    @Test
    public void testGetConnectedToken() {
        Controller2Callback controllerCallback = new Controller2Callback();
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken())
                             .setControllerCallback(sHandlerExecutor, controllerCallback)
                             .build()) {
            assertTrue(controllerCallback.awaitOnConnected(WAIT_TIME_MS));
            assertEquals(controller, controllerCallback.mController);
            assertEquals(mSession.getToken(), controller.getConnectedToken());

            Bundle extrasFromConnectedSessionToken =
                    controller.getConnectedToken().getExtras();
            assertNotNull(extrasFromConnectedSessionToken);
            assertEquals(TEST_VALUE, extrasFromConnectedSessionToken.getString(TEST_KEY));
        } finally {
            assertTrue(controllerCallback.awaitOnDisconnected(WAIT_TIME_MS));
            assertNull(controllerCallback.mController.getConnectedToken());
        }
    }

    @Test
    public void testCallback_onConnected_onDisconnected() {
        Controller2Callback controllerCallback = new Controller2Callback();
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken())
                             .setControllerCallback(sHandlerExecutor, controllerCallback)
                             .build()) {
            assertTrue(controllerCallback.awaitOnConnected(WAIT_TIME_MS));
            assertEquals(controller, controllerCallback.mController);
            assertTrue(controllerCallback.mAllowedCommands.hasCommand(ALLOWED_COMMAND_CODE));
        } finally {
            assertTrue(controllerCallback.awaitOnDisconnected(WAIT_TIME_MS));
        }
    }

    @Test
    public void testCallback_onSessionCommand() {
        Controller2Callback controllerCallback = new Controller2Callback();
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken())
                             .setControllerCallback(sHandlerExecutor, controllerCallback)
                             .build()) {
            assertTrue(controllerCallback.awaitOnConnected(WAIT_TIME_MS));

            String commandStr = "test_command";
            String commandExtraKey = "test_extra_key";
            String commandExtraValue = "test_extra_value";
            Bundle commandExtra = new Bundle();
            commandExtra.putString(commandExtraKey, commandExtraValue);
            Session2Command command = new Session2Command(commandStr, commandExtra);

            String commandArgKey = "test_arg_key";
            String commandArgValue = "test_arg_value";
            Bundle commandArg = new Bundle();
            commandArg.putString(commandArgKey, commandArgValue);
            mSession.sendSessionCommand(mSessionCallback.mControllerInfo, command, commandArg);

            assertTrue(controllerCallback.awaitOnSessionCommand(WAIT_TIME_MS));
            assertEquals(controller, controllerCallback.mController);
            assertEquals(commandStr, controllerCallback.mCommand.getCustomAction());
            assertEquals(commandExtraValue,
                    controllerCallback.mCommand.getCustomExtras().getString(commandExtraKey));
            assertEquals(commandArgValue, controllerCallback.mCommandArgs.getString(commandArgKey));
        } finally {
            assertTrue(controllerCallback.awaitOnDisconnected(WAIT_TIME_MS));
        }
    }

    @Test
    public void testCallback_onCommandResult() {
        Controller2Callback controllerCallback = new Controller2Callback();
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken())
                             .setControllerCallback(sHandlerExecutor, controllerCallback)
                             .build()) {
            assertTrue(controllerCallback.awaitOnConnected(WAIT_TIME_MS));

            String commandStr = "test_command";
            String commandExtraKey = "test_extra_key";
            String commandExtraValue = "test_extra_value";
            Bundle commandExtra = new Bundle();
            commandExtra.putString(commandExtraKey, commandExtraValue);
            Session2Command command = new Session2Command(commandStr, commandExtra);

            String commandArgKey = "test_arg_key";
            String commandArgValue = "test_arg_value";
            Bundle commandArg = new Bundle();
            commandArg.putString(commandArgKey, commandArgValue);
            controller.sendSessionCommand(command, commandArg);

            assertTrue(controllerCallback.awaitOnCommandResult(WAIT_TIME_MS));
            assertEquals(controller, controllerCallback.mController);
            assertEquals(SESSION_RESULT_CODE, controllerCallback.mCommandResult.getResultCode());
            assertEquals(SESSION_RESULT_VALUE,
                    controllerCallback.mCommandResult.getResultData()
                            .getString(SESSION_RESULT_KEY));
        } finally {
            assertTrue(controllerCallback.awaitOnDisconnected(WAIT_TIME_MS));
        }
    }

    @Test
    public void testCancelSessionCommand() {
        Controller2Callback controllerCallback = new Controller2Callback();
        try (MediaController2 controller =
                     new MediaController2.Builder(mContext, mSession.getToken())
                             .setControllerCallback(sHandlerExecutor, controllerCallback)
                             .build()) {
            assertTrue(controllerCallback.awaitOnConnected(WAIT_TIME_MS));

            String commandStr = "test_command_";
            String commandExtraKey = "test_extra_key_";
            String commandExtraValue = "test_extra_value_";
            Bundle commandExtra = new Bundle();
            commandExtra.putString(commandExtraKey, commandExtraValue);
            Session2Command command = new Session2Command(commandStr, commandExtra);

            String commandArgKey = "test_arg_key_";
            String commandArgValue = "test_arg_value_";
            Bundle commandArg = new Bundle();
            commandArg.putString(commandArgKey, commandArgValue);
            synchronized (sTestLock) {
                Object token = controller.sendSessionCommand(command, commandArg);
                controller.cancelSessionCommand(token);
            }
            assertTrue(controllerCallback.awaitOnCommandResult(WAIT_TIME_MS));
            assertEquals(Session2Command.Result.RESULT_INFO_SKIPPED,
                    controllerCallback.mCommandResult.getResultCode());
        } finally {
            assertTrue(controllerCallback.awaitOnDisconnected(WAIT_TIME_MS));
        }
    }

    class Session2Callback extends MediaSession2.SessionCallback {
        MediaSession2.ControllerInfo mControllerInfo;
        CountDownLatch mOnConnectedLatch = new CountDownLatch(1);

        @Override
        public Session2CommandGroup onConnect(MediaSession2 session,
                MediaSession2.ControllerInfo controller) {
            if (controller.getUid() != Process.myUid()) {
                return null;
            }
            mControllerInfo = controller;
            mOnConnectedLatch.countDown();
            return SESSION_ALLOWED_COMMANDS;
        }

        @Override
        public Session2Command.Result onSessionCommand(MediaSession2 session,
                MediaSession2.ControllerInfo controller, Session2Command command, Bundle args) {
            return SESSION_COMMAND_RESULT;
        }
    }

    class Controller2Callback extends MediaController2.ControllerCallback {
        CountDownLatch mOnConnectedLatch = new CountDownLatch(1);
        CountDownLatch mOnDisconnectedLatch = new CountDownLatch(1);
        private CountDownLatch mOnSessionCommandLatch = new CountDownLatch(1);
        private CountDownLatch mOnCommandResultLatch = new CountDownLatch(1);

        MediaController2 mController;
        Session2Command mCommand;
        Session2CommandGroup mAllowedCommands;
        Bundle mCommandArgs;
        Session2Command.Result mCommandResult;

        public boolean await(long waitMs) {
            try {
                return mOnSessionCommandLatch.await(waitMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        @Override
        public void onConnected(MediaController2 controller, Session2CommandGroup allowedCommands) {
            super.onConnected(controller, allowedCommands);
            mController = controller;
            mAllowedCommands = allowedCommands;
            mOnConnectedLatch.countDown();
        }

        @Override
        public void onDisconnected(MediaController2 controller) {
            super.onDisconnected(controller);
            mController = controller;
            mOnDisconnectedLatch.countDown();
        }

        @Override
        public Session2Command.Result onSessionCommand(MediaController2 controller,
                Session2Command command, Bundle args) {
            super.onSessionCommand(controller, command, args);
            mController = controller;
            mCommand = command;
            mCommandArgs = args;
            mOnSessionCommandLatch.countDown();
            return SESSION_COMMAND_RESULT;
        }

        @Override
        public void onCommandResult(MediaController2 controller, Object token,
                Session2Command command, Session2Command.Result result) {
            super.onCommandResult(controller, token, command, result);
            mController = controller;
            mCommand = command;
            mCommandResult = result;
            mOnCommandResultLatch.countDown();
        }

        @Override
        public void onPlaybackActiveChanged(MediaController2 controller, boolean playbackActive) {
            super.onPlaybackActiveChanged(controller, playbackActive);
        }

        public boolean awaitOnConnected(long waitTimeMs) {
            try {
                return mOnConnectedLatch.await(waitTimeMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        public boolean awaitOnDisconnected(long waitTimeMs) {
            try {
                return mOnDisconnectedLatch.await(waitTimeMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        public boolean awaitOnSessionCommand(long waitTimeMs) {
            try {
                return mOnSessionCommandLatch.await(waitTimeMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        public boolean awaitOnCommandResult(long waitTimeMs) {
            try {
                return mOnCommandResultLatch.await(waitTimeMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }
    }
}
