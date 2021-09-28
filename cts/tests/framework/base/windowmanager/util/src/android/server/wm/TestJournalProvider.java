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
 * limitations under the License
 */

package android.server.wm;

import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.server.wm.CommandSession.ActivityCallback;
import android.server.wm.CommandSession.ConfigInfo;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.function.Consumer;

/**
 * Let other testing packages put information for test cases to verify.
 *
 * This is a global container that there is no connection between test cases and testing components.
 * If a precise communication is required, use {@link CommandSession.ActivitySessionClient} instead.
 *
 * <p>Sample:</p>
 * <pre>
 * // In test case:
 * void testSomething() {
 *     TestJournalContainer.start();
 *     // Launch the testing activity.
 *     // ...
 *     assertTrue(TestJournalContainer.get(COMPONENT_NAME_OF_TESTING_ACTIVITY).extras
 *             .getBoolean("test"));
 * }
 *
 * // In the testing activity:
 * protected void onResume() {
 *     super.onResume();
 *     TestJournalProvider.putExtras(this, bundle -> bundle.putBoolean("test", true));
 * }
 * </pre>
 */
public class TestJournalProvider extends ContentProvider {
    private static final boolean DEBUG = "eng".equals(Build.TYPE);
    private static final String TAG = TestJournalProvider.class.getSimpleName();
    private static final Uri URI = Uri.parse("content://android.server.wm.testjournalprovider");

    /** Indicates who owns the events. */
    private static final String EXTRA_KEY_OWNER = "key_owner";
    /** Puts a {@link ActivityCallback} into the journal container for who receives the callback. */
    private static final String METHOD_ADD_CALLBACK = "add_callback";
    /** Sets the {@link ConfigInfo} for who reports the configuration info. */
    private static final String METHOD_SET_LAST_CONFIG_INFO = "set_last_config_info";
    /** Puts any additional information. */
    private static final String METHOD_PUT_EXTRAS = "put_extras";

    /** Avoid accidentally getting data from {@link #TestJournalContainer} in another process. */
    private static boolean sCrossProcessAccessGuard;

    @Override
    public boolean onCreate() {
        sCrossProcessAccessGuard = true;
        return true;
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        switch (method) {
            case METHOD_ADD_CALLBACK:
                ensureExtras(method, extras);
                TestJournalContainer.getInstance().addCallback(
                        extras.getString(EXTRA_KEY_OWNER), extras.getParcelable(method));
                break;

            case METHOD_SET_LAST_CONFIG_INFO:
                ensureExtras(method, extras);
                TestJournalContainer.getInstance().setLastConfigInfo(
                        extras.getString(EXTRA_KEY_OWNER), extras.getParcelable(method));
                break;

            case METHOD_PUT_EXTRAS:
                ensureExtras(method, extras);
                TestJournalContainer.getInstance().putExtras(
                        extras.getString(EXTRA_KEY_OWNER), extras);
                break;
        }
        return null;
    }

    private static void ensureExtras(String method, Bundle extras) {
        if (extras == null) {
            throw new IllegalArgumentException(
                    "The calling method=" + method + " does not allow null bundle");
        }
        extras.setClassLoader(TestJournal.class.getClassLoader());
        if (DEBUG) {
            extras.size(); // Trigger unparcel for printing plain text.
            Log.i(TAG, method + " extras=" + extras);
        }
    }

    /** Records the activity is called with the given callback. */
    public static void putActivityCallback(Activity activity, ActivityCallback callback) {
        try (TestJournalClient client = TestJournalClient.create(activity,
                activity.getComponentName())) {
            client.addCallback(callback);
        }
    }

    /** Puts information about the activity. */
    public static void putExtras(Activity activity, Consumer<Bundle> bundleFiller) {
        putExtras(activity, activity.getComponentName(), bundleFiller);
    }

    /** Puts information about the component. */
    public static void putExtras(Context context, ComponentName owner,
            Consumer<Bundle> bundleFiller) {
        putExtras(context, componentNameToKey(owner), bundleFiller);
    }

    /** Puts information about the keyword. */
    public static void putExtras(Context context, String owner, Consumer<Bundle> bundleFiller) {
        try (TestJournalClient client = TestJournalClient.create(context, owner)) {
            final Bundle extras = new Bundle();
            bundleFiller.accept(extras);
            client.putExtras(extras);
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }

    private static String componentNameToKey(ComponentName name) {
        return name.flattenToShortString();
    }

    /**
     * Executes from the testing component to put their info to {@link TestJournalProvider}.
     * The caller can be from any process or package.
     */
    public static class TestJournalClient implements AutoCloseable {
        private static final String EMPTY_ARG = "";
        private final ContentProviderClient mClient;
        private final String mOwner;

        public TestJournalClient(ContentProviderClient client, String owner) {
            mClient = client;
            mOwner = owner;
        }

        private void callWithExtras(String method, Bundle extras) {
            extras.putString(EXTRA_KEY_OWNER, mOwner);
            try {
                mClient.call(method, EMPTY_ARG, extras);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        }

        public void addCallback(ActivityCallback callback) {
            final Bundle extras = new Bundle();
            extras.putParcelable(METHOD_ADD_CALLBACK, callback);
            callWithExtras(METHOD_ADD_CALLBACK, extras);
        }

        public void setLastConfigInfo(ConfigInfo configInfo) {
            final Bundle extras = new Bundle();
            extras.putParcelable(METHOD_SET_LAST_CONFIG_INFO, configInfo);
            callWithExtras(METHOD_SET_LAST_CONFIG_INFO, extras);
        }

        public void putExtras(Bundle extras) {
            callWithExtras(METHOD_PUT_EXTRAS, extras);
        }

        @Override
        public void close() {
            mClient.close();
        }

        @NonNull
        static TestJournalClient create(Context context, ComponentName owner) {
            return create(context, componentNameToKey(owner));
        }

        @NonNull
        static TestJournalClient create(Context context, String owner) {
            final ContentProviderClient client = context.getContentResolver()
                    .acquireContentProviderClient(URI);
            if (client == null) {
                throw new RuntimeException("Unable to acquire " + URI);
            }
            return new TestJournalClient(client, owner);
        }
    }

    /** The basic unit to store testing information. */
    public static class TestJournal {
        @NonNull
        public final ArrayList<ActivityCallback> callbacks = new ArrayList<>();
        @NonNull
        public final Bundle extras = new Bundle();
        @Nullable
        public ConfigInfo lastConfigInfo;
    }

    /**
     * The container lives in test case side. It stores the information from testing components.
     * The caller must be in the same process as {@link TestJournalProvider}.
     */
    public static class TestJournalContainer {
        private static TestJournalContainer sInstance;
        private final ArrayMap<String, TestJournal> mContainer = new ArrayMap<>();

        private TestJournalContainer() {
        }

        @NonNull
        public static TestJournal get(ComponentName owner) {
            return get(componentNameToKey(owner));
        }

        @NonNull
        public static TestJournal get(String owner) {
            return getInstance().getTestJournal(owner);
        }

        /**
         * Perform the action which may have thread safety concerns when accessing the fields of
         * {@link TestJournal}.
         */
        public static void withThreadSafeAccess(Runnable action) {
            synchronized (getInstance()) {
                action.run();
            }
        }

        private synchronized TestJournal getTestJournal(String owner) {
            TestJournal info = mContainer.get(owner);
            if (info == null) {
                info = new TestJournal();
                mContainer.put(owner, info);
            }
            return info;
        }

        synchronized void addCallback(String owner, ActivityCallback callback) {
            getTestJournal(owner).callbacks.add(callback);
        }

        synchronized void setLastConfigInfo(String owner, ConfigInfo configInfo) {
            getTestJournal(owner).lastConfigInfo = configInfo;
        }

        synchronized void putExtras(String owner, Bundle extras) {
            getTestJournal(owner).extras.putAll(extras);
        }

        private synchronized static TestJournalContainer getInstance() {
            if (!TestJournalProvider.sCrossProcessAccessGuard) {
                throw new IllegalAccessError(TestJournalProvider.class.getSimpleName()
                        + " is not alive in this process");
            }
            if (sInstance == null) {
                sInstance = new TestJournalContainer();
            }
            return sInstance;
        }

        /**
         * The method should be called when we are only interested in the following events. It
         * actually clears the previous records.
         */
        @NonNull
        public static TestJournalContainer start() {
            final TestJournalContainer instance = getInstance();
            synchronized (instance) {
                instance.mContainer.clear();
            }
            return instance;
        }
    }
}
