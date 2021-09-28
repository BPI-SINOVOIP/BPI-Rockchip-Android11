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

package android.server.wm.lifecycle;

import static android.server.wm.StateLogger.log;

import android.app.Activity;
import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Pair;

import java.util.ArrayList;
import java.util.List;

/**
 * Used as a shared log storage of activity lifecycle transitions. Methods must be synchronized to
 * prevent concurrent modification of the log store.
 */
public class LifecycleLog extends ContentProvider {

    public enum ActivityCallback {
        PRE_ON_CREATE,
        ON_CREATE,
        ON_START,
        ON_RESUME,
        ON_PAUSE,
        ON_STOP,
        ON_RESTART,
        ON_DESTROY,
        ON_ACTIVITY_RESULT,
        ON_POST_CREATE,
        ON_NEW_INTENT,
        ON_MULTI_WINDOW_MODE_CHANGED,
        ON_TOP_POSITION_GAINED,
        ON_TOP_POSITION_LOST
    }

    interface LifecycleTrackerCallback {
        void onActivityLifecycleChanged();
    }

    /** Identifies the activity to which the event corresponds. */
    private static final String EXTRA_KEY_ACTIVITY = "key_activity";
    /** Puts a lifecycle or callback into the container. */
    private static final String METHOD_ADD_CALLBACK = "add_callback";
    /** Content provider URI for cross-process lifecycle transitions collecting. */
    private static final Uri URI = Uri.parse("content://android.server.wm.lifecycle.logprovider");

    /**
     * Log for encountered activity callbacks. Note that methods accessing or modifying this
     * list should be synchronized as it can be accessed from different threads.
     */
    private final static List<Pair<String, ActivityCallback>> sLog = new ArrayList<>();

    /**
     * Lifecycle tracker interface that waits for correct states or sequences.
     */
    private static LifecycleTrackerCallback sLifecycleTracker;

    /** Clear the entire transition log. */
    void clear() {
        synchronized(sLog) {
            sLog.clear();
        }
    }

    public void setLifecycleTracker(LifecycleTrackerCallback lifecycleTracker) {
        sLifecycleTracker = lifecycleTracker;
    }

    /** Add activity callback to the log. */
    private void onActivityCallback(String activityCanonicalName,
            ActivityCallback callback) {
        synchronized (sLog) {
            sLog.add(new Pair<>(activityCanonicalName, callback));
        }
        log("Activity " + activityCanonicalName + " receiver callback " + callback);
        // Trigger check for valid state in the tracker
        if (sLifecycleTracker != null) {
            sLifecycleTracker.onActivityLifecycleChanged();
        }
    }

    /** Get logs for all recorded transitions. */
    List<Pair<String, ActivityCallback>> getLog() {
        // Wrap in a new list to prevent concurrent modification
        synchronized(sLog) {
            return new ArrayList<>(sLog);
        }
    }

    /** Get transition logs for the specified activity. */
    List<ActivityCallback> getActivityLog(Class<? extends Activity> activityClass) {
        final String activityName = activityClass.getCanonicalName();
        log("Looking up log for activity: " + activityName);
        final List<ActivityCallback> activityLog = new ArrayList<>();
        synchronized(sLog) {
            for (Pair<String, ActivityCallback> transition : sLog) {
                if (transition.first.equals(activityName)) {
                    activityLog.add(transition.second);
                }
            }
        }
        return activityLog;
    }


    // ContentProvider implementation for cross-process tracking

    public static class LifecycleLogClient implements AutoCloseable {
        private static final String EMPTY_ARG = "";
        private final ContentProviderClient mClient;
        private final String mOwner;

        LifecycleLogClient(ContentProviderClient client, Activity owner) {
            mClient = client;
            mOwner = owner.getClass().getCanonicalName();
        }

        void onActivityCallback(ActivityCallback callback) {
            final Bundle extras = new Bundle();
            extras.putInt(METHOD_ADD_CALLBACK, callback.ordinal());
            extras.putString(EXTRA_KEY_ACTIVITY, mOwner);
            try {
                mClient.call(METHOD_ADD_CALLBACK, EMPTY_ARG, extras);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public void close() {
            mClient.close();
        }

        static LifecycleLogClient create(Activity owner) {
            final ContentProviderClient client = owner.getContentResolver()
                    .acquireContentProviderClient(URI);
            if (client == null) {
                throw new RuntimeException("Unable to acquire " + URI);
            }
            return new LifecycleLogClient(client, owner);
        }
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        if (!METHOD_ADD_CALLBACK.equals(method)) {
            throw new UnsupportedOperationException();
        }
        onActivityCallback(extras.getString(EXTRA_KEY_ACTIVITY),
                ActivityCallback.values()[extras.getInt(method)]);
        return null;
    }

    @Override
    public boolean onCreate() {
        return true;
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
}
