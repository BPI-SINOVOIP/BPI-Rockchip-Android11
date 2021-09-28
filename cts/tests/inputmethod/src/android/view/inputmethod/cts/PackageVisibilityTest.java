/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.view.inputmethod.cts;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.UnlockScreenRule;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.Until;

import com.android.cts.mockime.ImeCommand;
import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.security.InvalidParameterException;
import java.util.concurrent.TimeUnit;

@MediumTest
@RunWith(AndroidJUnit4.class)
public final class PackageVisibilityTest extends EndToEndImeTestBase {
    static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    private static final ComponentName TEST_ACTIVITY = new ComponentName(
            "android.view.inputmethod.ctstestapp",
            "android.view.inputmethod.ctstestapp.MainActivity");

    private static final Uri TEST_ACTIVITY_URI =
            Uri.parse("https://example.com/android/view/inputmethod/ctstestapp");

    private static final String EXTRA_KEY_PRIVATE_IME_OPTIONS =
            "android.view.inputmethod.ctstestapp.EXTRA_KEY_PRIVATE_IME_OPTIONS";

    private static final String TEST_MARKER_PREFIX =
            "android.view.inputmethod.cts.PackageVisibilityTest";

    private static String getTestMarker() {
        return TEST_MARKER_PREFIX + "/"  + SystemClock.elapsedRealtimeNanos();
    }

    @NonNull
    private static Uri formatStringIntentParam(@NonNull Uri uri, @NonNull String key,
            @Nullable String value) {
        if (value == null) {
            return uri;
        }
        return uri.buildUpon().appendQueryParameter(key, value).build();
    }

    @NonNull
    private static String formatStringIntentParam(@NonNull String key, @Nullable String value) {
        if (key.matches("[ \"']")) {
            throw new InvalidParameterException("Unsupported character(s) in key=" + key);
        }
        if (value.matches("[ \"']")) {
            throw new InvalidParameterException("Unsupported character(s) in value=" + value);
        }
        return value != null ? String.format(" --es %s %s", key, value) : "";
    }

    /**
     * Launch the standalone version of the test {@link android.app.Activity} then wait for
     * completions of launch.
     *
     * <p>Note: this method does not use
     * {@link android.app.Instrumentation#startActivitySync(Intent)} because it does not work when
     * both the calling process and the target process run under the instant app mode. Instead this
     * method relies on adb command {@code adb shell am start} to work around that limitation.</p>
     *
     * @param instant {@code true} if the caller and the target is installed as instant apps.
     * @param privateImeOptions If not {@code null},
     *                          {@link android.view.inputmethod.EditorInfo#privateImeOptions} will
     *                          in the test {@link android.app.Activity} will be set to this value.
     * @param timeout timeout in milliseconds.
     */
    private void launchTestActivity(boolean instant, @Nullable String privateImeOptions,
            long timeout) {
        final String command;
        if (instant) {
            final Uri uri = formatStringIntentParam(
                    TEST_ACTIVITY_URI, EXTRA_KEY_PRIVATE_IME_OPTIONS, privateImeOptions);
            command = String.format("am start -a %s -c %s %s",
                    Intent.ACTION_VIEW, Intent.CATEGORY_BROWSABLE, uri.toString());
        } else {
            command = String.format("am start -n %s",
                    TEST_ACTIVITY.flattenToShortString())
                    + formatStringIntentParam(EXTRA_KEY_PRIVATE_IME_OPTIONS, privateImeOptions);
        }
        runShellCommand(command);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .wait(Until.hasObject(By.pkg(TEST_ACTIVITY.getPackageName()).depth(0)), timeout);
    }

    @AppModeFull
    @Test
    public void testTargetPackageIsVisibleFromImeFull() throws Exception {
        testTargetPackageIsVisibleFromIme(false /* instant */);
    }

    @AppModeInstant
    @Test
    public void testTargetPackageIsVisibleFromImeInstant() throws Exception {
        // We need to explicitly check this condition in case tests are executed with atest command.
        // See Bug 158617529 for details.
        assumeTrue("This test should run when and only under the instant app mode.",
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager()
                        .isInstantApp());
        testTargetPackageIsVisibleFromIme(true /* instant */);
    }

    private void testTargetPackageIsVisibleFromIme(boolean instant) throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            launchTestActivity(instant, marker, TIMEOUT);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            final ImeCommand command = imeSession.callGetApplicationInfo(
                    TEST_ACTIVITY.getPackageName(), PackageManager.GET_META_DATA);
            final ImeEvent event = expectCommand(stream, command, TIMEOUT);

            if (event.isNullReturnValue()) {
                fail("getApplicationInfo() returned null.");
            }
            if (event.isExceptionReturnValue()) {
                final Exception exception = event.getReturnExceptionValue();
                fail(exception.toString());
            }
            final ApplicationInfo applicationInfoFromIme = event.getReturnParcelableValue();
            assertEquals(TEST_ACTIVITY.getPackageName(), applicationInfoFromIme.packageName);
        }
    }
}
