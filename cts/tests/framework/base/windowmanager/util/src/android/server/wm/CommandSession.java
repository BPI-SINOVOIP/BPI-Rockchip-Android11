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
 * limitations under the License
 */

package android.server.wm;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import android.os.SystemClock;
import android.server.wm.TestJournalProvider.TestJournalClient;
import android.util.ArrayMap;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.View;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.TimeoutException;
import java.util.function.Consumer;

/**
 * A mechanism for communication between the started activity and its caller in different package or
 * process. Generally, a test case is the client, and the testing activity is the host. The client
 * can control whether to send an async or sync command with response data.
 * <p>Sample:</p>
 * <pre>
 * try (ActivitySessionClient client = new ActivitySessionClient(context)) {
 *     final ActivitySession session = client.startActivity(
 *             new Intent(context, TestActivity.class));
 *     final Bundle response = session.requestOrientation(
 *             ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
 *     Log.i("Test", "Config: " + CommandSession.getConfigInfo(response));
 *     Log.i("Test", "Callbacks: " + CommandSession.getCallbackHistory(response));
 *
 *     session.startActivity(session.getOriginalLaunchIntent());
 *     Log.i("Test", "New intent callbacks: " + session.takeCallbackHistory());
 * }
 * </pre>
 * <p>To perform custom command, use sendCommand* in {@link ActivitySession} to send the request,
 * and the receiving side (activity) can extend {@link BasicTestActivity} or
 * {@link CommandSessionActivity} with overriding handleCommand to do the corresponding action.</p>
 */
public final class CommandSession {
    private static final boolean DEBUG = "eng".equals(Build.TYPE);
    private static final String TAG = "CommandSession";

    private static final String EXTRA_PREFIX = "s_";

    static final String KEY_FORWARD = EXTRA_PREFIX + "key_forward";

    private static final String KEY_CALLBACK_HISTORY = EXTRA_PREFIX + "key_callback_history";
    private static final String KEY_CLIENT_ID = EXTRA_PREFIX + "key_client_id";
    private static final String KEY_COMMAND = EXTRA_PREFIX + "key_command";
    private static final String KEY_CONFIG_INFO = EXTRA_PREFIX + "key_config_info";
    private static final String KEY_APP_CONFIG_INFO = EXTRA_PREFIX + "key_app_config_info";
    private static final String KEY_HOST_ID = EXTRA_PREFIX + "key_host_id";
    private static final String KEY_ORIENTATION = EXTRA_PREFIX + "key_orientation";
    private static final String KEY_REQUEST_TOKEN = EXTRA_PREFIX + "key_request_id";
    private static final String KEY_UID_HAS_ACCESS_ON_DISPLAY =
            EXTRA_PREFIX + "uid_has_access_on_display";

    private static final String COMMAND_FINISH = EXTRA_PREFIX + "command_finish";
    private static final String COMMAND_GET_CONFIG = EXTRA_PREFIX + "command_get_config";
    private static final String COMMAND_GET_APP_CONFIG = EXTRA_PREFIX + "command_get_app_config";
    private static final String COMMAND_ORIENTATION = EXTRA_PREFIX + "command_orientation";
    private static final String COMMAND_TAKE_CALLBACK_HISTORY = EXTRA_PREFIX
            + "command_take_callback_history";
    private static final String COMMAND_WAIT_IDLE = EXTRA_PREFIX + "command_wait_idle";
    private static final String COMMAND_DISPLAY_ACCESS_CHECK =
            EXTRA_PREFIX + "display_access_check";

    private static final long INVALID_REQUEST_TOKEN = -1;

    private CommandSession() {
    }

    /** Get {@link ConfigInfo} from bundle. */
    public static ConfigInfo getConfigInfo(Bundle data) {
        return data.getParcelable(KEY_CONFIG_INFO);
    }

    /** Get application {@link ConfigInfo} from bundle. */
    public static ConfigInfo getAppConfigInfo(Bundle data) {
        return data.getParcelable(KEY_APP_CONFIG_INFO);
    }

    /** Get list of {@link ActivityCallback} from bundle. */
    public static ArrayList<ActivityCallback> getCallbackHistory(Bundle data) {
        return data.getParcelableArrayList(KEY_CALLBACK_HISTORY);
    }

    /** Return non-null if the session info should forward to launch target. */
    public static LaunchInjector handleForward(Bundle data) {
        if (data == null || !data.getBoolean(KEY_FORWARD)) {
            return null;
        }

        // Only keep the necessary data which relates to session.
        final Bundle sessionInfo = new Bundle(data);
        sessionInfo.remove(KEY_FORWARD);
        for (String key : sessionInfo.keySet()) {
            if (key != null && !key.startsWith(EXTRA_PREFIX)) {
                sessionInfo.remove(key);
            }
        }

        return new LaunchInjector() {
            @Override
            public void setupIntent(Intent intent) {
                intent.putExtras(sessionInfo);
            }

            @Override
            public void setupShellCommand(StringBuilder shellCommand) {
                // Currently there is no use case from shell.
                throw new UnsupportedOperationException();
            }
        };
    }

    private static String generateId(String prefix, Object obj) {
        return prefix + "_" + Integer.toHexString(System.identityHashCode(obj));
    }

    private static String commandIntentToString(Intent intent) {
        return intent.getStringExtra(KEY_COMMAND)
                + "@" + intent.getLongExtra(KEY_REQUEST_TOKEN, INVALID_REQUEST_TOKEN);
    }

    /** Get an unique token to match the request and reply. */
    private static long generateRequestToken() {
        return SystemClock.elapsedRealtimeNanos();
    }

    /**
     * As a controller associated with the testing activity. It can only process one sync command
     * (require response) at a time.
     */
    public static class ActivitySession {
        private final ActivitySessionClient mClient;
        private final String mHostId;
        private final Response mPendingResponse = new Response();
        // Only set when requiring response.
        private long mPendingRequestToken = INVALID_REQUEST_TOKEN;
        private String mPendingCommand;
        private boolean mFinished;
        private Intent mOriginalLaunchIntent;

        ActivitySession(ActivitySessionClient client, boolean requireReply) {
            mClient = client;
            mHostId = generateId("activity", this);
            if (requireReply) {
                mPendingRequestToken = generateRequestToken();
                mPendingCommand = COMMAND_WAIT_IDLE;
            }
        }

        /** Start the activity again. The intent must have the same filter as original one. */
        public void startActivity(Intent intent) {
            if (!intent.filterEquals(mOriginalLaunchIntent)) {
                throw new IllegalArgumentException("The intent filter is different " + intent);
            }
            mClient.mContext.startActivity(intent);
            mFinished = false;
        }

        /**
         * Request the activity to set the given orientation. The returned bundle contains the
         * changed config info and activity lifecycles during the change.
         *
         * @param orientation An orientation constant as used in
         *                    {@link android.content.pm.ActivityInfo#screenOrientation}.
         */
        public Bundle requestOrientation(int orientation) {
            final Bundle data = new Bundle();
            data.putInt(KEY_ORIENTATION, orientation);
            return sendCommandAndWaitReply(COMMAND_ORIENTATION, data);
        }

        /** Get {@link ConfigInfo} of the associated activity. */
        public ConfigInfo getConfigInfo() {
            return CommandSession.getConfigInfo(sendCommandAndWaitReply(COMMAND_GET_CONFIG));
        }

        /** Get {@link ConfigInfo} of the Application of the associated activity. */
        public ConfigInfo getAppConfigInfo() {
            return CommandSession.getAppConfigInfo(sendCommandAndWaitReply(COMMAND_GET_APP_CONFIG));
        }

        /**
         * Get executed callbacks of the activity since the last command. The current callback
         * history will also be cleared.
         */
        public ArrayList<ActivityCallback> takeCallbackHistory() {
            return getCallbackHistory(sendCommandAndWaitReply(COMMAND_TAKE_CALLBACK_HISTORY,
                    null /* data */));
        }

        /** Get the intent that launches the activity. Null if launch from shell command. */
        public Intent getOriginalLaunchIntent() {
            return mOriginalLaunchIntent;
        }

        /** Get a name to represent this session by the original launch intent if possible. */
        public String getName() {
            if (mOriginalLaunchIntent != null) {
                final ComponentName componentName = mOriginalLaunchIntent.getComponent();
                if (componentName != null) {
                    return componentName.flattenToShortString();
                }
                return mOriginalLaunchIntent.toString();
            }
            return "Activity";
        }

        public boolean isUidAccesibleOnDisplay() {
            return sendCommandAndWaitReply(COMMAND_DISPLAY_ACCESS_CHECK, null).getBoolean(
                    KEY_UID_HAS_ACCESS_ON_DISPLAY);
        }

        /** Send command to the associated activity. */
        public void sendCommand(String command) {
            sendCommand(command, null /* data */);
        }

        /** Send command with extra parameters to the associated activity. */
        public void sendCommand(String command, Bundle data) {
            if (mFinished) {
                throw new IllegalStateException("The session is finished");
            }

            final Intent intent = new Intent(mHostId);
            if (data != null) {
                intent.putExtras(data);
            }
            intent.putExtra(KEY_COMMAND, command);
            mClient.mContext.sendBroadcast(intent);
            if (DEBUG) {
                Log.i(TAG, mClient.mClientId + " sends " + commandIntentToString(intent)
                        + " to " + mHostId);
            }
        }

        public Bundle sendCommandAndWaitReply(String command) {
            return sendCommandAndWaitReply(command, null /* data */);
        }

        /** Returns the reply data by the given command. */
        public Bundle sendCommandAndWaitReply(String command, Bundle data) {
            if (data == null) {
                data = new Bundle();
            }

            if (mPendingRequestToken != INVALID_REQUEST_TOKEN) {
                throw new IllegalStateException("The previous pending request "
                        + mPendingCommand + " has not replied");
            }
            mPendingRequestToken = generateRequestToken();
            mPendingCommand = command;
            data.putLong(KEY_REQUEST_TOKEN, mPendingRequestToken);

            sendCommand(command, data);
            return waitReply();
        }

        private Bundle waitReply() {
            if (mPendingRequestToken == INVALID_REQUEST_TOKEN) {
                throw new IllegalStateException("No pending request to wait");
            }

            if (DEBUG) Log.i(TAG, "Waiting for request " + mPendingRequestToken);
            try {
                return mPendingResponse.takeResult();
            } catch (TimeoutException e) {
                throw new RuntimeException("Timeout on command "
                        + mPendingCommand + " with token " + mPendingRequestToken, e);
            } finally {
                mPendingRequestToken = INVALID_REQUEST_TOKEN;
                mPendingCommand = null;
            }
        }

        // This method should run on an independent thread.
        void receiveReply(Bundle reply) {
            final long incomingToken = reply.getLong(KEY_REQUEST_TOKEN);
            if (incomingToken == mPendingRequestToken) {
                mPendingResponse.setResult(reply);
            } else {
                throw new IllegalStateException("Mismatched token: incoming=" + incomingToken
                        + " pending=" + mPendingRequestToken);
            }
        }

        /** Finish the activity that associates with this session. */
        public void finish() {
            if (!mFinished) {
                sendCommand(COMMAND_FINISH);
                mClient.mSessions.remove(mHostId);
                mFinished = true;
            }
        }

        private static class Response {
            static final int TIMEOUT_MILLIS = 5000;
            private volatile boolean mHasResult;
            private Bundle mResult;

            synchronized void setResult(Bundle result) {
                mHasResult = true;
                mResult = result;
                notifyAll();
            }

            synchronized Bundle takeResult() throws TimeoutException {
                final long startTime = SystemClock.uptimeMillis();
                while (!mHasResult) {
                    try {
                        wait(TIMEOUT_MILLIS);
                    } catch (InterruptedException ignored) {
                    }
                    if (!mHasResult && (SystemClock.uptimeMillis() - startTime > TIMEOUT_MILLIS)) {
                        throw new TimeoutException("No response over " + TIMEOUT_MILLIS + "ms");
                    }
                }

                final Bundle result = mResult;
                mHasResult = false;
                mResult = null;
                return result;
            }
        }
    }

    /** For LaunchProxy to setup launch parameter that establishes session. */
    interface LaunchInjector {
        void setupIntent(Intent intent);
        void setupShellCommand(StringBuilder shellCommand);
    }

    /** A proxy to launch activity by intent or shell command. */
    interface LaunchProxy {
        void setLaunchInjector(LaunchInjector injector);
        default Bundle getExtras() { return null; }
        void execute();
        boolean shouldWaitForLaunched();
    }

    abstract static class DefaultLaunchProxy implements LaunchProxy {
        LaunchInjector mLaunchInjector;

        @Override
        public boolean shouldWaitForLaunched() {
            return true;
        }

        @Override
        public void setLaunchInjector(LaunchInjector injector) {
            mLaunchInjector = injector;
        }
    }

    /** Created by test case to control testing activity that implements the session protocol. */
    public static class ActivitySessionClient extends BroadcastReceiver implements AutoCloseable {
        private final Context mContext;
        private final String mClientId;
        private final HandlerThread mThread;
        private final ArrayMap<String, ActivitySession> mSessions = new ArrayMap<>();
        private boolean mClosed;

        public ActivitySessionClient(Context context) {
            mContext = context;
            mClientId = generateId("testcase", this);
            mThread = new HandlerThread(mClientId);
            mThread.start();
            context.registerReceiver(this, new IntentFilter(mClientId),
                    null /* broadcastPermission */, new Handler(mThread.getLooper()));
        }

        /** Start the activity by the given intent and wait it becomes idle. */
        public ActivitySession startActivity(Intent intent) {
            return startActivity(intent, null /* options */, true /* waitIdle */);
        }

        /**
         * Launch the activity and establish a new session.
         *
         * @param intent The description of the activity to start.
         * @param options Additional options for how the Activity should be started.
         * @param waitIdle Block in this method until the target activity is idle.
         * @return The session to communicate with the started activity.
         */
        public ActivitySession startActivity(Intent intent, Bundle options, boolean waitIdle) {
            ensureNotClosed();
            final ActivitySession session = new ActivitySession(this, waitIdle);
            mSessions.put(session.mHostId, session);
            setupLaunchIntent(intent, waitIdle, session);

            if (!(mContext instanceof Activity)) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            mContext.startActivity(intent, options);
            if (waitIdle) {
                session.waitReply();
            }
            return session;
        }

        /** Launch activity via proxy that allows to inject session parameters. */
        public ActivitySession startActivity(LaunchProxy proxy) {
            ensureNotClosed();
            final boolean waitIdle = proxy.shouldWaitForLaunched();
            final ActivitySession session = new ActivitySession(this, waitIdle);
            mSessions.put(session.mHostId, session);

            proxy.setLaunchInjector(new LaunchInjector() {
                @Override
                public void setupIntent(Intent intent) {
                    final Bundle bundle = proxy.getExtras();
                    if (bundle != null) {
                        intent.putExtras(bundle);
                    }
                    setupLaunchIntent(intent, waitIdle, session);
                }

                @Override
                public void setupShellCommand(StringBuilder commandBuilder) {
                    commandBuilder.append(" --es " + KEY_HOST_ID + " " + session.mHostId);
                    commandBuilder.append(" --es " + KEY_CLIENT_ID + " " + mClientId);
                    if (waitIdle) {
                        commandBuilder.append(
                                " --el " + KEY_REQUEST_TOKEN + " " + session.mPendingRequestToken);
                        commandBuilder.append(" --es " + KEY_COMMAND + " " + COMMAND_WAIT_IDLE);
                    }
                }
            });

            proxy.execute();
            if (waitIdle) {
                session.waitReply();
            }
            return session;
        }

        private void setupLaunchIntent(Intent intent, boolean waitIdle, ActivitySession session) {
            intent.putExtra(KEY_HOST_ID, session.mHostId);
            intent.putExtra(KEY_CLIENT_ID, mClientId);
            if (waitIdle) {
                intent.putExtra(KEY_REQUEST_TOKEN, session.mPendingRequestToken);
                intent.putExtra(KEY_COMMAND, COMMAND_WAIT_IDLE);
            }
            session.mOriginalLaunchIntent = intent;
        }

        private void ensureNotClosed() {
            if (mClosed) {
                throw new IllegalStateException("This session client is closed.");
            }
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            final ActivitySession session = mSessions.get(intent.getStringExtra(KEY_HOST_ID));
            if (DEBUG) Log.i(TAG, mClientId + " receives " + commandIntentToString(intent));
            if (session != null) {
                session.receiveReply(intent.getExtras());
            } else {
                Log.w(TAG, "No available session for " + commandIntentToString(intent));
            }
        }

        /** Complete cleanup with finishing all associated activities. */
        @Override
        public void close() {
            close(true /* finishSession */);
        }

        /** Cleanup except finish associated activities. */
        public void closeAndKeepSession() {
            close(false /* finishSession */);
        }

        /**
         * Closes this client. Once a client is closed, all methods on it will throw an
         * IllegalStateException and all responses from host are ignored.
         *
         * @param finishSession Whether to finish activities launched from this client.
         */
        public void close(boolean finishSession) {
            ensureNotClosed();
            mClosed = true;
            if (finishSession) {
                for (int i = mSessions.size() - 1; i >= 0; i--) {
                    mSessions.valueAt(i).finish();
                }
            }
            mContext.unregisterReceiver(this);
            mThread.quit();
        }
    }

    /**
     * Interface definition for session host to process command from {@link ActivitySessionClient}.
     */
    interface CommandReceiver {
        /** Called when the session host is receiving command. */
        void receiveCommand(String command, Bundle data);
    }

    /** The host receives command from the test client. */
    public static class ActivitySessionHost extends BroadcastReceiver {
        private final CommandReceiver mCallback;
        private final Context mContext;
        private final String mClientId;
        private final String mHostId;

        ActivitySessionHost(Context context, String hostId, String clientId,
                CommandReceiver callback) {
            mContext = context;
            mHostId = hostId;
            mClientId = clientId;
            mCallback = callback;
            context.registerReceiver(this, new IntentFilter(hostId));
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (DEBUG) {
                Log.i(TAG, mHostId + "(" + mContext.getClass().getSimpleName()
                        + ") receives " + commandIntentToString(intent));
            }
            mCallback.receiveCommand(intent.getStringExtra(KEY_COMMAND), intent.getExtras());
        }

        void reply(String command, Bundle data) {
            final Intent intent = new Intent(mClientId);
            intent.putExtras(data);
            intent.putExtra(KEY_COMMAND, command);
            intent.putExtra(KEY_HOST_ID, mHostId);
            mContext.sendBroadcast(intent);
            if (DEBUG) {
                Log.i(TAG, mHostId + "(" + mContext.getClass().getSimpleName()
                        + ") replies " + commandIntentToString(intent) + " to " + mClientId);
            }
        }

        void destory() {
            mContext.unregisterReceiver(this);
        }
    }

    /**
     * A map to store data by host id. The usage should be declared as static that is able to keep
     * data after activity is relaunched.
     */
    private static class StaticHostStorage<T> {
        final ArrayMap<String, ArrayList<T>> mStorage = new ArrayMap<>();

        void add(String hostId, T data) {
            ArrayList<T> commands = mStorage.get(hostId);
            if (commands == null) {
                commands = new ArrayList<>();
                mStorage.put(hostId, commands);
            }
            commands.add(data);
        }

        ArrayList<T> get(String hostId) {
            return mStorage.get(hostId);
        }

        void clear(String hostId) {
            mStorage.remove(hostId);
        }
    }

    /** Store the commands which have not been handled. */
    private static class CommandStorage extends StaticHostStorage<Bundle> {

        /** Remove the oldest matched command and return its request token. */
        long consume(String hostId, String command) {
            final ArrayList<Bundle> commands = mStorage.get(hostId);
            if (commands != null) {
                final Iterator<Bundle> iterator = commands.iterator();
                while (iterator.hasNext()) {
                    final Bundle data = iterator.next();
                    if (command.equals(data.getString(KEY_COMMAND))) {
                        iterator.remove();
                        return data.getLong(KEY_REQUEST_TOKEN);
                    }
                }
                if (commands.isEmpty()) {
                    clear(hostId);
                }
            }
            return INVALID_REQUEST_TOKEN;
        }

        boolean containsCommand(String receiverId, String command) {
            final ArrayList<Bundle> dataList = mStorage.get(receiverId);
            if (dataList != null) {
                for (Bundle data : dataList) {
                    if (command.equals(data.getString(KEY_COMMAND))) {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    /**
     * The base activity which supports the session protocol. If the caller does not use
     * {@link ActivitySessionClient}, it behaves as a normal activity.
     */
    public static class CommandSessionActivity extends Activity implements CommandReceiver {
        /** Static command storage for across relaunch. */
        private static CommandStorage sCommandStorage;
        private ActivitySessionHost mReceiver;

        /** The subclasses can disable the test journal client if its information is not used. */
        protected boolean mUseTestJournal = true;
        protected TestJournalClient mTestJournalClient;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            if (mUseTestJournal) {
                mTestJournalClient = TestJournalClient.create(this /* context */,
                        getComponentName());
            }

            final String hostId = getIntent().getStringExtra(KEY_HOST_ID);
            final String clientId = getIntent().getStringExtra(KEY_CLIENT_ID);
            if (hostId != null && clientId != null) {
                if (sCommandStorage == null) {
                    sCommandStorage = new CommandStorage();
                }
                mReceiver = new ActivitySessionHost(this /* context */, hostId, clientId,
                        this /* callback */);
            }
        }

        @Override
        protected void onDestroy() {
            super.onDestroy();
            if (mReceiver != null) {
                if (!isChangingConfigurations()) {
                    sCommandStorage.clear(getHostId());
                }
                mReceiver.destory();
            }
            if (mTestJournalClient != null) {
                mTestJournalClient.close();
            }
        }

        @Override
        public final void receiveCommand(String command, Bundle data) {
            if (mReceiver == null) {
                throw new IllegalStateException("The receiver is not created");
            }
            sCommandStorage.add(getHostId(), data);
            handleCommand(command, data);
        }

        /** Handle the incoming command from client. */
        protected void handleCommand(String command, Bundle data) {
        }

        protected final void reply(String command) {
            reply(command, null /* data */);
        }

        /** Reply data to client for the command. */
        protected final void reply(String command, Bundle data) {
            if (mReceiver == null) {
                throw new IllegalStateException("The receiver is not created");
            }
            final long requestToke = sCommandStorage.consume(getHostId(), command);
            if (requestToke == INVALID_REQUEST_TOKEN) {
                throw new IllegalStateException("There is no pending command " + command);
            }
            if (data == null) {
                data = new Bundle();
            }
            data.putLong(KEY_REQUEST_TOKEN, requestToke);
            mReceiver.reply(command, data);
        }

        protected boolean hasPendingCommand(String command) {
            return mReceiver != null && sCommandStorage.containsCommand(getHostId(), command);
        }

        /** Returns null means this activity does support the session protocol. */
        final String getHostId() {
            return mReceiver != null ? mReceiver.mHostId : null;
        }
    }

    /** The default implementation that supports basic commands to interact with activity. */
    public static class BasicTestActivity extends CommandSessionActivity {
        /** Static callback history for across relaunch. */
        private static final StaticHostStorage<ActivityCallback> sCallbackStorage =
                new StaticHostStorage<>();

        protected boolean mPrintCallbackLog;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            onCallback(ActivityCallback.ON_CREATE);

            if (getHostId() != null) {
                final int orientation = getIntent().getIntExtra(KEY_ORIENTATION, Integer.MIN_VALUE);
                if (orientation != Integer.MIN_VALUE) {
                    setRequestedOrientation(orientation);
                }
                if (COMMAND_WAIT_IDLE.equals(getIntent().getStringExtra(KEY_COMMAND))) {
                    receiveCommand(COMMAND_WAIT_IDLE, getIntent().getExtras());
                    // No need to execute again if the activity is relaunched.
                    getIntent().removeExtra(KEY_COMMAND);
                }
            }
        }

        @Override
        public void handleCommand(String command, Bundle data) {
            switch (command) {
                case COMMAND_ORIENTATION:
                    clearCallbackHistory();
                    setRequestedOrientation(data.getInt(KEY_ORIENTATION));
                    getWindow().getDecorView().postDelayed(() -> {
                        if (reportConfigIfNeeded()) {
                            Log.w(getTag(), "Fallback report. The orientation may not change.");
                        }
                    }, ActivitySession.Response.TIMEOUT_MILLIS / 2);
                    break;

                case COMMAND_GET_CONFIG:
                    runWhenIdle(() -> {
                        final Bundle replyData = new Bundle();
                        replyData.putParcelable(KEY_CONFIG_INFO, getConfigInfo());
                        reply(COMMAND_GET_CONFIG, replyData);
                    });
                    break;

                case COMMAND_GET_APP_CONFIG:
                    runWhenIdle(() -> {
                        final Bundle replyData = new Bundle();
                        replyData.putParcelable(KEY_APP_CONFIG_INFO, getAppConfigInfo());
                        reply(COMMAND_GET_APP_CONFIG, replyData);
                    });
                    break;

                case COMMAND_FINISH:
                    if (!isFinishing()) {
                        finish();
                    }
                    break;

                case COMMAND_TAKE_CALLBACK_HISTORY:
                    final Bundle replyData = new Bundle();
                    replyData.putParcelableArrayList(KEY_CALLBACK_HISTORY, getCallbackHistory());
                    reply(command, replyData);
                    clearCallbackHistory();
                    break;

                case COMMAND_WAIT_IDLE:
                    runWhenIdle(() -> reply(command));
                    break;

                case COMMAND_DISPLAY_ACCESS_CHECK:
                    final Bundle result = new Bundle();
                    final boolean displayHasAccess = getDisplay().hasAccess(Process.myUid());
                    result.putBoolean(KEY_UID_HAS_ACCESS_ON_DISPLAY, displayHasAccess);
                    reply(command, result);
                    break;

                default:
                    break;
            }
        }

        protected final void clearCallbackHistory() {
            sCallbackStorage.clear(getHostId());
        }

        protected final ArrayList<ActivityCallback> getCallbackHistory() {
            return sCallbackStorage.get(getHostId());
        }

        protected void runWhenIdle(Runnable r) {
            Looper.getMainLooper().getQueue().addIdleHandler(() -> {
                r.run();
                return false;
            });
        }

        protected boolean reportConfigIfNeeded() {
            if (!hasPendingCommand(COMMAND_ORIENTATION)) {
                return false;
            }
            runWhenIdle(() -> {
                final Bundle replyData = new Bundle();
                replyData.putParcelable(KEY_CONFIG_INFO, getConfigInfo());
                replyData.putParcelableArrayList(KEY_CALLBACK_HISTORY, getCallbackHistory());
                reply(COMMAND_ORIENTATION, replyData);
                clearCallbackHistory();
            });
            return true;
        }

        @Override
        protected void onStart() {
            super.onStart();
            onCallback(ActivityCallback.ON_START);
        }

        @Override
        protected void onRestart() {
            super.onRestart();
            onCallback(ActivityCallback.ON_RESTART);
        }

        @Override
        protected void onResume() {
            super.onResume();
            onCallback(ActivityCallback.ON_RESUME);
            reportConfigIfNeeded();
        }

        @Override
        protected void onPause() {
            super.onPause();
            onCallback(ActivityCallback.ON_PAUSE);
        }

        @Override
        protected void onStop() {
            super.onStop();
            onCallback(ActivityCallback.ON_STOP);
        }

        @Override
        protected void onDestroy() {
            super.onDestroy();
            onCallback(ActivityCallback.ON_DESTROY);
        }

        @Override
        protected void onActivityResult(int requestCode, int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            onCallback(ActivityCallback.ON_ACTIVITY_RESULT);
        }

        @Override
        protected void onUserLeaveHint() {
            super.onUserLeaveHint();
            onCallback(ActivityCallback.ON_USER_LEAVE_HINT);
        }

        @Override
        protected void onNewIntent(Intent intent) {
            super.onNewIntent(intent);
            onCallback(ActivityCallback.ON_NEW_INTENT);
        }

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            onCallback(ActivityCallback.ON_CONFIGURATION_CHANGED);
            reportConfigIfNeeded();
        }

        @Override
        public void onMultiWindowModeChanged(boolean isInMultiWindowMode, Configuration newConfig) {
            super.onMultiWindowModeChanged(isInMultiWindowMode, newConfig);
            onCallback(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED);
        }

        @Override
        public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode,
                Configuration newConfig) {
            super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
            onCallback(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED);
        }

        @Override
        public void onMovedToDisplay(int displayId, Configuration config) {
            super.onMovedToDisplay(displayId, config);
            onCallback(ActivityCallback.ON_MOVED_TO_DISPLAY);
        }

        public void onCallback(ActivityCallback callback) {
            if (mPrintCallbackLog) {
                Log.i(getTag(), callback + " @ "
                        + Integer.toHexString(System.identityHashCode(this)));
            }
            final String hostId = getHostId();
            if (hostId != null) {
                sCallbackStorage.add(hostId, callback);
            }
            if (mTestJournalClient != null) {
                mTestJournalClient.addCallback(callback);
            }
        }

        protected void withTestJournalClient(Consumer<TestJournalClient> client) {
            if (mTestJournalClient != null) {
                client.accept(mTestJournalClient);
            }
        }

        protected String getTag() {
            return getClass().getSimpleName();
        }

        /** Get configuration and display info. It should be called only after resumed. */
        protected ConfigInfo getConfigInfo() {
            final View view = getWindow().getDecorView();
            if (!view.isAttachedToWindow()) {
                Log.w(getTag(), "Decor view has not attached");
            }
            return new ConfigInfo(view.getContext(), view.getDisplay());
        }

        /** Same as {@link #getConfigInfo()}, but for Application. */
        private ConfigInfo getAppConfigInfo() {
            final Application application = (Application) getApplicationContext();
            return new ConfigInfo(application, getDisplay());
        }
    }

    public enum ActivityCallback implements Parcelable {
        ON_CREATE,
        ON_START,
        ON_RESUME,
        ON_PAUSE,
        ON_STOP,
        ON_RESTART,
        ON_DESTROY,
        ON_ACTIVITY_RESULT,
        ON_USER_LEAVE_HINT,
        ON_NEW_INTENT,
        ON_CONFIGURATION_CHANGED,
        ON_MULTI_WINDOW_MODE_CHANGED,
        ON_PICTURE_IN_PICTURE_MODE_CHANGED,
        ON_MOVED_TO_DISPLAY,
        ON_PICTURE_IN_PICTURE_REQUESTED;

        private static final ActivityCallback[] sValues = ActivityCallback.values();
        public static final int SIZE = sValues.length;

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(final Parcel dest, final int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<ActivityCallback> CREATOR = new Creator<ActivityCallback>() {
            @Override
            public ActivityCallback createFromParcel(final Parcel source) {
                return sValues[source.readInt()];
            }

            @Override
            public ActivityCallback[] newArray(final int size) {
                return new ActivityCallback[size];
            }
        };
    }

    public static class ConfigInfo implements Parcelable {
        public int displayId = Display.INVALID_DISPLAY;
        public int rotation;
        public SizeInfo sizeInfo;

        ConfigInfo() {
        }

        public ConfigInfo(Context context, Display display) {
            final Resources res = context.getResources();
            final DisplayMetrics metrics = res.getDisplayMetrics();
            final Configuration config = res.getConfiguration();

            if (display != null) {
                displayId = display.getDisplayId();
                rotation = display.getRotation();
            }
            sizeInfo = new SizeInfo(display, metrics, config);
        }

        public ConfigInfo(Resources res) {
            final DisplayMetrics metrics = res.getDisplayMetrics();
            final Configuration config = res.getConfiguration();
            sizeInfo = new SizeInfo(null /* display */, metrics, config);
        }

        @Override
        public String toString() {
            return "ConfigInfo: {displayId=" + displayId + " rotation=" + rotation
                    + " " + sizeInfo + "}";
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(displayId);
            dest.writeInt(rotation);
            dest.writeParcelable(sizeInfo, 0 /* parcelableFlags */);
        }

        public void readFromParcel(Parcel in) {
            displayId = in.readInt();
            rotation = in.readInt();
            sizeInfo = in.readParcelable(SizeInfo.class.getClassLoader());
        }

        public static final Creator<ConfigInfo> CREATOR = new Creator<ConfigInfo>() {
            @Override
            public ConfigInfo createFromParcel(Parcel source) {
                final ConfigInfo sizeInfo = new ConfigInfo();
                sizeInfo.readFromParcel(source);
                return sizeInfo;
            }

            @Override
            public ConfigInfo[] newArray(int size) {
                return new ConfigInfo[size];
            }
        };
    }

    public static class SizeInfo implements Parcelable {
        public int widthDp;
        public int heightDp;
        public int displayWidth;
        public int displayHeight;
        public int metricsWidth;
        public int metricsHeight;
        public int smallestWidthDp;
        public int densityDpi;
        public int orientation;

        SizeInfo() {
        }

        public SizeInfo(Display display, DisplayMetrics metrics, Configuration config) {
            if (display != null) {
                final Point displaySize = new Point();
                display.getSize(displaySize);
                displayWidth = displaySize.x;
                displayHeight = displaySize.y;
            }

            widthDp = config.screenWidthDp;
            heightDp = config.screenHeightDp;
            metricsWidth = metrics.widthPixels;
            metricsHeight = metrics.heightPixels;
            smallestWidthDp = config.smallestScreenWidthDp;
            densityDpi = config.densityDpi;
            orientation = config.orientation;
        }

        @Override
        public String toString() {
            return "SizeInfo: {widthDp=" + widthDp + " heightDp=" + heightDp
                    + " displayWidth=" + displayWidth + " displayHeight=" + displayHeight
                    + " metricsWidth=" + metricsWidth + " metricsHeight=" + metricsHeight
                    + " smallestWidthDp=" + smallestWidthDp + " densityDpi=" + densityDpi
                    + " orientation=" + orientation + "}";
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) {
                return true;
            }
            if (!(obj instanceof SizeInfo)) {
                return false;
            }
            final SizeInfo that = (SizeInfo) obj;
            return widthDp == that.widthDp
                    && heightDp == that.heightDp
                    && displayWidth == that.displayWidth
                    && displayHeight == that.displayHeight
                    && metricsWidth == that.metricsWidth
                    && metricsHeight == that.metricsHeight
                    && smallestWidthDp == that.smallestWidthDp
                    && densityDpi == that.densityDpi
                    && orientation == that.orientation;
        }

        @Override
        public int hashCode() {
            int result = 0;
            result = 31 * result + widthDp;
            result = 31 * result + heightDp;
            result = 31 * result + displayWidth;
            result = 31 * result + displayHeight;
            result = 31 * result + metricsWidth;
            result = 31 * result + metricsHeight;
            result = 31 * result + smallestWidthDp;
            result = 31 * result + densityDpi;
            result = 31 * result + orientation;
            return result;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(widthDp);
            dest.writeInt(heightDp);
            dest.writeInt(displayWidth);
            dest.writeInt(displayHeight);
            dest.writeInt(metricsWidth);
            dest.writeInt(metricsHeight);
            dest.writeInt(smallestWidthDp);
            dest.writeInt(densityDpi);
            dest.writeInt(orientation);
        }

        public void readFromParcel(Parcel in) {
            widthDp = in.readInt();
            heightDp = in.readInt();
            displayWidth = in.readInt();
            displayHeight = in.readInt();
            metricsWidth = in.readInt();
            metricsHeight = in.readInt();
            smallestWidthDp = in.readInt();
            densityDpi = in.readInt();
            orientation = in.readInt();
        }

        public static final Creator<SizeInfo> CREATOR = new Creator<SizeInfo>() {
            @Override
            public SizeInfo createFromParcel(Parcel source) {
                final SizeInfo sizeInfo = new SizeInfo();
                sizeInfo.readFromParcel(source);
                return sizeInfo;
            }

            @Override
            public SizeInfo[] newArray(int size) {
                return new SizeInfo[size];
            }
        };
    }
}
