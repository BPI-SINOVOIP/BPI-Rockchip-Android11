/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.server.wm.settings;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.Settings.SettingNotFoundException;
import android.util.Log;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.SystemUtil;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Helper class to save, set, and restore global system-level preferences.
 * <p>
 * To use this class, testing APK must be self-instrumented and have
 * {@link android.Manifest.permission#WRITE_SECURE_SETTINGS}.
 * <p>
 * A test that changes system-level preferences can be written easily and reliably.
 * <pre>
 * static class PrefSession extends SettingsSession<String> {
 *     PrefSession() {
 *         super(android.provider.Settings.Secure.getUriFor(
 *                       android.provider.Settings.Secure.PREFERENCE_KEY),
 *               android.provider.Settings.Secure::getString,
 *               android.provider.Settings.Secure::putString);
 *     }
 * }
 *
 * @Test
 * public void doTest() throws Exception {
 *     try (final PrefSession prefSession = new PrefSession()) {
 *         prefSession.set("value 1");
 *         doTest1();
 *         prefSession.set("value 2");
 *         doTest2();
 *     }
 * }
 * </pre>
 */
public class SettingsSession<T> implements AutoCloseable {
    private static final String TAG = SettingsSession.class.getSimpleName();
    private static final boolean DEBUG = false;

    @FunctionalInterface
    public interface SettingsGetter<T> {
        T get(ContentResolver cr, String key) throws SettingNotFoundException;
    }

    @FunctionalInterface
    public interface SettingsSetter<T> {
        void set(ContentResolver cr, String key, T value);
    }

    /**
     * To debug to detect nested sessions for the same key. Enabled when {@link #DEBUG} is true.
     * Note that nested sessions can be merged into one session.
     */
    private static final SessionCounters sSessionCounters = new SessionCounters();

    protected final Uri mUri;
    protected final boolean mHasInitialValue;
    protected final T mInitialValue;
    private final SettingsGetter<T> mGetter;
    private final SettingsSetter<T> mSetter;

    public SettingsSession(final Uri uri, final SettingsGetter<T> getter,
            final SettingsSetter<T> setter) {
        mUri = uri;
        mGetter = getter;
        mSetter = setter;
        T initialValue;
        boolean hasInitialValue;
        try {
            initialValue = get(uri, getter);
            hasInitialValue = true;
        } catch (SettingNotFoundException e) {
            initialValue = null;
            hasInitialValue = false;
        }
        mInitialValue = initialValue;
        mHasInitialValue = hasInitialValue;
        if (DEBUG) {
            Log.i(TAG, "start: uri=" + uri
                    + (mHasInitialValue ? " value=" + mInitialValue : " undefined"));
            sSessionCounters.open(uri);
        }
    }

    public void set(final @NonNull T value) {
        put(mUri, mSetter, value);
        if (DEBUG) {
            Log.i(TAG, "  set: uri=" + mUri + " value=" + value);
        }
    }

    public T get() {
        try {
            return get(mUri, mGetter);
        } catch (SettingNotFoundException e) {
            return null;
        }
    }

    @Override
    public void close() {
        if (mHasInitialValue) {
            put(mUri, mSetter, mInitialValue);
            if (DEBUG) {
                Log.i(TAG, "close: uri=" + mUri + " value=" + mInitialValue);
            }
        } else {
            delete(mUri);
            if (DEBUG) {
                Log.i(TAG, "close: uri=" + mUri + " deleted");
            }
        }
        if (DEBUG) {
            sSessionCounters.close(mUri);
        }
    }

    private static <T> void put(final Uri uri, final SettingsSetter<T> setter, T value) {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            setter.set(getContentResolver(), uri.getLastPathSegment(), value);
        });
    }

    private static <T> T get(final Uri uri, final SettingsGetter<T> getter)
            throws SettingNotFoundException {
        return getter.get(getContentResolver(), uri.getLastPathSegment());
    }

    protected static void delete(final Uri uri) {
        final List<String> segments = uri.getPathSegments();
        if (segments.size() != 2) {
            Log.w(TAG, "Unsupported uri for deletion: " + uri, new Throwable());
            return;
        }
        final String namespace = segments.get(0);
        final String key = segments.get(1);
        // SystemUtil.runWithShellPermissionIdentity (only applies to the permission checking in
        // package manager and appops) does not change calling uid which is enforced in
        // SettingsProvider for deletion, so it requires shell command to pass the restriction.
        SystemUtil.runShellCommand("settings delete " + namespace + " " + key);
    }

    private static ContentResolver getContentResolver() {
        return getInstrumentation().getTargetContext().getContentResolver();
    }

    private static class SessionCounters {
        private final Map<Uri, Integer> mOpenSessions = new HashMap<>();

        void open(final Uri uri) {
            final Integer count = mOpenSessions.get(uri);
            if (count == null) {
                mOpenSessions.put(uri, 1);
                return;
            }
            mOpenSessions.put(uri, count + 1);
            Log.w(TAG, "Open nested session for " + uri, new Throwable());
        }

        void close(final Uri uri) {
            final int count = mOpenSessions.get(uri);
            if (count == 1) {
                mOpenSessions.remove(uri);
                return;
            }
            mOpenSessions.put(uri, count - 1);
            Log.w(TAG, "Close nested session for " + uri, new Throwable());
        }
    }
}
