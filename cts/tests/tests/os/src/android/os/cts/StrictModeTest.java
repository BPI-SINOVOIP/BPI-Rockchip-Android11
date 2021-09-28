/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.os.cts;

import static android.content.Context.WINDOW_SERVICE;
import static android.content.pm.PackageManager.FEATURE_INPUT_METHODS;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import static com.android.cts.mockime.ImeEventStreamTestUtils.clearAllEvents;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.inputmethodservice.InputMethodService;
import android.net.TrafficStats;
import android.net.Uri;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy.Builder;
import android.os.StrictMode.ViolationInfo;
import android.os.strictmode.CleartextNetworkViolation;
import android.os.strictmode.CustomViolation;
import android.os.strictmode.DiskReadViolation;
import android.os.strictmode.DiskWriteViolation;
import android.os.strictmode.ExplicitGcViolation;
import android.os.strictmode.FileUriExposedViolation;
import android.os.strictmode.InstanceCountViolation;
import android.os.strictmode.LeakedClosableViolation;
import android.os.strictmode.NetworkViolation;
import android.os.strictmode.NonSdkApiUsedViolation;
import android.os.strictmode.UntaggedSocketViolation;
import android.os.strictmode.Violation;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;
import android.view.Display;
import android.view.ViewConfiguration;
import android.view.WindowManager;

import androidx.annotation.IntDef;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.Socket;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Tests for {@link StrictMode} */
@RunWith(AndroidJUnit4.class)
public class StrictModeTest {
    private static final String TAG = "StrictModeTest";
    private static final String REMOTE_SERVICE_ACTION = "android.app.REMOTESERVICE";
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(10); // 10 seconds
    private static final long NOT_EXPECT_TIMEOUT = TimeUnit.SECONDS.toMillis(2);

    private StrictMode.ThreadPolicy mThreadPolicy;
    private StrictMode.VmPolicy mVmPolicy;

    // TODO(b/160143006): re-enable IMS part test.
    private static final boolean DISABLE_VERIFY_IMS = false;

    /**
     * Verify mode to verifying if APIs violates incorrect context violation.
     *
     * @see #VERIFY_MODE_GET_DISPLAY
     * @see #VERIFY_MODE_GET_WINDOW_MANAGER
     * @see #VERIFY_MODE_GET_VIEW_CONFIGURATION
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true, value = {
            VERIFY_MODE_GET_DISPLAY,
            VERIFY_MODE_GET_WINDOW_MANAGER,
            VERIFY_MODE_GET_VIEW_CONFIGURATION,
    })
    private @interface VerifyMode {}

    /**
     * Verifies if {@link Context#getDisplay} from {@link InputMethodService} and context created
     * from {@link InputMethodService#createConfigurationContext(Configuration)} violates
     * incorrect context violation.
     */
    private static final int VERIFY_MODE_GET_DISPLAY = 1;
    /**
     * Verifies if get {@link android.view.WindowManager} from {@link InputMethodService} and
     * context created from {@link InputMethodService#createConfigurationContext(Configuration)}
     * violates incorrect context violation.
     *
     * @see Context#getSystemService(String)
     * @see Context#getSystemService(Class)
     */
    private static final int VERIFY_MODE_GET_WINDOW_MANAGER = 2;
    /**
     * Verifies if passing {@link InputMethodService} and context created
     * from {@link InputMethodService#createConfigurationContext(Configuration)} to
     * {@link android.view.ViewConfiguration#get(Context)} violates incorrect context violation.
     */
    private static final int VERIFY_MODE_GET_VIEW_CONFIGURATION = 3;

    private Context getContext() {
        return ApplicationProvider.getApplicationContext();
    }

    @Before
    public void setUp() {
        mThreadPolicy = StrictMode.getThreadPolicy();
        mVmPolicy = StrictMode.getVmPolicy();
    }

    @After
    public void tearDown() {
        StrictMode.setThreadPolicy(mThreadPolicy);
        StrictMode.setVmPolicy(mVmPolicy);
    }

    public interface ThrowingRunnable {
        void run() throws Exception;
    }

    @Test
    public void testThreadBuilder() throws Exception {
        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().detectDiskReads().penaltyLog().build();
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder(policy).build());

        final File test = File.createTempFile("foo", "bar");
        inspectViolation(
                test::exists,
                info -> {
                    assertThat(info.getViolationDetails()).isNull();
                    assertThat(info.getStackTrace()).contains("DiskReadViolation");
                });
    }

    @Test
    public void testUnclosedCloseable() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectLeakedClosableObjects().build());

        inspectViolation(
                () -> leakCloseable("leaked.txt"),
                info -> {
                    assertThat(info.getViolationDetails())
                            .isEqualTo(
                                    "A resource was acquired at attached stack trace but never released. See java.io.Closeable for information on avoiding resource leaks.");
                    assertThat(info.getStackTrace())
                            .contains("Explicit termination method 'close' not called");
                    assertThat(info.getStackTrace()).contains("leakCloseable");
                    assertThat(info.getViolationClass())
                            .isAssignableTo(LeakedClosableViolation.class);
                });
    }

    private void leakCloseable(String fileName) throws InterruptedException {
        final CountDownLatch finalizedSignal = new CountDownLatch(1);
        try {
            new FileOutputStream(new File(getContext().getFilesDir(), fileName)) {
                @Override
                protected void finalize() throws IOException {
                    super.finalize();
                    finalizedSignal.countDown();
                }
            };
        } catch (FileNotFoundException e) {
            throw new RuntimeException(e);
        }
        Runtime.getRuntime().gc();
        Runtime.getRuntime().runFinalization();
        // Sometimes it needs extra prodding.
        if (!finalizedSignal.await(5, TimeUnit.SECONDS)) {
            Runtime.getRuntime().gc();
            Runtime.getRuntime().runFinalization();
        }
    }

    @Test
    public void testClassInstanceLimit() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder()
                        .setClassInstanceLimit(LimitedClass.class, 1)
                        .build());
        List<LimitedClass> references = new ArrayList<>();
        assertNoViolation(() -> references.add(new LimitedClass()));
        references.add(new LimitedClass());
        inspectViolation(
                StrictMode::conditionallyCheckInstanceCounts,
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(InstanceCountViolation.class));
    }

    private static final class LimitedClass {}

    /** Insecure connection should be detected */
    @AppModeFull
    @Test
    public void testCleartextNetwork() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testCleartextNetwork() ignored on device without Internet");
            return;
        }

        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectCleartextNetwork().penaltyLog().build());

        inspectViolation(
                () ->
                        ((HttpURLConnection) new URL("http://example.com/").openConnection())
                                .getResponseCode(),
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(CleartextNetworkViolation.class));
    }

    /** Secure connection should be ignored */
    @Test
    public void testEncryptedNetwork() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testEncryptedNetwork() ignored on device without Internet");
            return;
        }

        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectCleartextNetwork().penaltyLog().build());

        assertNoViolation(
                () ->
                        ((HttpURLConnection) new URL("https://example.com/").openConnection())
                                .getResponseCode());
    }

    @Test
    public void testFileUriExposure() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectFileUriExposure().penaltyLog().build());

        final Uri badUri = Uri.fromFile(new File("/sdcard/meow.jpg"));
        inspectViolation(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setDataAndType(badUri, "image/jpeg");
                    getContext().startActivity(intent);
                },
                info -> {
                    assertThat(info.getStackTrace()).contains(badUri + " exposed beyond app");
                });

        final Uri goodUri = Uri.parse("content://com.example/foobar");
        assertNoViolation(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setDataAndType(goodUri, "image/jpeg");
                    getContext().startActivity(intent);
                });
    }

    @Test
    public void testFileUriExposure_Chooser() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectFileUriExposure().penaltyLog().build());

        final Uri badUri = Uri.fromFile(new File("/sdcard/meow.jpg"));
        inspectViolation(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_SEND);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setType("image/jpeg");
                    intent.putExtra(Intent.EXTRA_STREAM, badUri);

                    Intent chooser = Intent.createChooser(intent, "CTS");
                    chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    getContext().startActivity(chooser);
                },
                info -> {
                    assertThat(info.getStackTrace()).contains(badUri + " exposed beyond app");
                });
    }

    @Test
    public void testContentUriWithoutPermission() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder()
                        .detectContentUriWithoutPermission()
                        .penaltyLog()
                        .build());

        final Uri uri = Uri.parse("content://com.example/foobar");
        inspectViolation(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setDataAndType(uri, "image/jpeg");
                    getContext().startActivity(intent);
                },
                info ->
                        assertThat(info.getStackTrace())
                                .contains(uri + " exposed beyond app"));

        assertNoViolation(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setDataAndType(uri, "image/jpeg");
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    getContext().startActivity(intent);
                });
    }

    @AppModeFull
    @Test
    public void testUntaggedSocketsHttp() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testUntaggedSockets() ignored on device without Internet");
            return;
        }

        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectUntaggedSockets().penaltyLog().build());

        inspectViolation(
                () ->
                        ((HttpURLConnection) new URL("http://example.com/").openConnection())
                                .getResponseCode(),
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(UntaggedSocketViolation.class));

        assertNoViolation(
                () -> {
                    TrafficStats.setThreadStatsTag(0xDECAFBAD);
                    try {
                        ((HttpURLConnection) new URL("http://example.com/").openConnection())
                                .getResponseCode();
                    } finally {
                        TrafficStats.clearThreadStatsTag();
                    }
                });
    }

    @Test
    public void testUntaggedSocketsRaw() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testUntaggedSockets() ignored on device without Internet");
            return;
        }

        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectUntaggedSockets().penaltyLog().build());

        assertNoViolation(
                () -> {
                    TrafficStats.setThreadStatsTag(0xDECAFBAD);
                    try (Socket socket = new Socket("example.com", 80)) {
                        socket.getOutputStream().close();
                    } finally {
                        TrafficStats.clearThreadStatsTag();
                    }
                });

        inspectViolation(
                () -> {
                    try (Socket socket = new Socket("example.com", 80)) {
                        socket.getOutputStream().close();
                    }
                },
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(UntaggedSocketViolation.class));
    }

    private static final int PERMISSION_USER_ONLY = 0600;

    @Test
    public void testRead() throws Exception {
        final File test = File.createTempFile("foo", "bar");
        final File dir = test.getParentFile();

        final FileInputStream is = new FileInputStream(test);
        final FileDescriptor fd =
                Os.open(test.getAbsolutePath(), OsConstants.O_RDONLY, PERMISSION_USER_ONLY);

        StrictMode.setThreadPolicy(
                new StrictMode.ThreadPolicy.Builder().detectDiskReads().penaltyLog().build());
        inspectViolation(
                test::exists,
                info -> {
                    assertThat(info.getViolationDetails()).isNull();
                    assertThat(info.getStackTrace()).contains("DiskReadViolation");
                });

        Consumer<ViolationInfo> assertDiskReadPolicy = info -> assertThat(
                info.getViolationClass()).isAssignableTo(DiskReadViolation.class);
        inspectViolation(test::exists, assertDiskReadPolicy);
        inspectViolation(test::length, assertDiskReadPolicy);
        inspectViolation(dir::list, assertDiskReadPolicy);
        inspectViolation(is::read, assertDiskReadPolicy);

        inspectViolation(() -> new FileInputStream(test), assertDiskReadPolicy);
        inspectViolation(
                () -> Os.open(test.getAbsolutePath(), OsConstants.O_RDONLY, PERMISSION_USER_ONLY),
                assertDiskReadPolicy);
        inspectViolation(() -> Os.read(fd, new byte[10], 0, 1), assertDiskReadPolicy);
    }

    @Test
    public void testWrite() throws Exception {
        File file = File.createTempFile("foo", "bar");

        final FileOutputStream os = new FileOutputStream(file);
        final FileDescriptor fd =
                Os.open(file.getAbsolutePath(), OsConstants.O_RDWR, PERMISSION_USER_ONLY);

        StrictMode.setThreadPolicy(
                new StrictMode.ThreadPolicy.Builder().detectDiskWrites().penaltyLog().build());

        inspectViolation(
                file::createNewFile,
                info -> {
                    assertThat(info.getViolationDetails()).isNull();
                    assertThat(info.getStackTrace()).contains("DiskWriteViolation");
                });

        Consumer<ViolationInfo> assertDiskWritePolicy = info -> assertThat(
                info.getViolationClass()).isAssignableTo(DiskWriteViolation.class);

        inspectViolation(() -> File.createTempFile("foo", "bar"), assertDiskWritePolicy);
        inspectViolation(() -> new FileOutputStream(file), assertDiskWritePolicy);
        inspectViolation(file::delete, assertDiskWritePolicy);
        inspectViolation(file::createNewFile, assertDiskWritePolicy);
        inspectViolation(() -> os.write(32), assertDiskWritePolicy);

        inspectViolation(
                () -> Os.open(file.getAbsolutePath(), OsConstants.O_RDWR, PERMISSION_USER_ONLY),
                assertDiskWritePolicy);
        inspectViolation(() -> Os.write(fd, new byte[10], 0, 1), assertDiskWritePolicy);
        inspectViolation(() -> Os.fsync(fd), assertDiskWritePolicy);
        inspectViolation(
                () -> file.renameTo(new File(file.getParent(), "foobar")), assertDiskWritePolicy);
    }

    @AppModeFull
    @Test
    public void testNetwork() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testUntaggedSockets() ignored on device without Internet");
            return;
        }

        StrictMode.setThreadPolicy(
                new StrictMode.ThreadPolicy.Builder().detectNetwork().penaltyLog().build());

        inspectViolation(
                () -> {
                    try (Socket socket = new Socket("example.com", 80)) {
                        socket.getOutputStream().close();
                    }
                },
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(NetworkViolation.class));
        inspectViolation(
                () ->
                        ((HttpURLConnection) new URL("http://example.com/").openConnection())
                                .getResponseCode(),
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(NetworkViolation.class));
    }

    @Test
    public void testExplicitGc() throws Exception {
        StrictMode.setThreadPolicy(
                new StrictMode.ThreadPolicy.Builder().detectExplicitGc().penaltyLog().build());

        inspectViolation(
                () -> { Runtime.getRuntime().gc(); },
                info -> assertThat(info.getViolationClass())
                        .isAssignableTo(ExplicitGcViolation.class));
    }

    @Test
    public void testViolationAcrossBinder() throws Exception {
        runWithRemoteServiceBound(
                getContext(),
                service -> {
                    StrictMode.setThreadPolicy(
                            new Builder().detectDiskWrites().penaltyLog().build());

                    try {
                        inspectViolation(
                                () -> service.performDiskWrite(),
                                (info) -> {
                                    assertThat(info.getViolationClass())
                                            .isAssignableTo(DiskWriteViolation.class);
                                    assertThat(info.getViolationDetails())
                                            .isNull(); // Disk write has no message.
                                    assertThat(info.getStackTrace())
                                            .contains("DiskWriteViolation");
                                    assertThat(info.getStackTrace())
                                            .contains(
                                                    "at android.os.StrictMode$AndroidBlockGuardPolicy.onWriteToDisk");
                                    assertThat(info.getStackTrace())
                                            .contains("# via Binder call with stack:");
                                    assertThat(info.getStackTrace())
                                            .contains(
                                                    "at android.os.cts.ISecondary$Stub$Proxy.performDiskWrite");
                                });
                        assertNoViolation(() -> service.getPid());
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    private void checkNonSdkApiUsageViolation(boolean blacklist, String className,
            String methodName, Class<?>... paramTypes) throws Exception {
        Class<?> clazz = Class.forName(className);
        inspectViolation(
            () -> {
                try {
                    java.lang.reflect.Method m = clazz.getDeclaredMethod(methodName, paramTypes);
                    if (blacklist) {
                        fail();
                    }
                } catch (NoSuchMethodException expected) {
                  if (!blacklist) {
                    fail();
                  }
                }
            },
            info -> {
                assertThat(info).isNotNull();
                assertThat(info.getViolationClass())
                        .isAssignableTo(NonSdkApiUsedViolation.class);
                assertThat(info.getViolationDetails()).contains(methodName);
                assertThat(info.getStackTrace()).contains("checkNonSdkApiUsageViolation");
            }
        );
    }

    @Test
    public void testNonSdkApiUsage() throws Exception {
        StrictMode.VmPolicy oldVmPolicy = StrictMode.getVmPolicy();
        StrictMode.ThreadPolicy oldThreadPolicy = StrictMode.getThreadPolicy();
        try {
            StrictMode.setVmPolicy(
                    new StrictMode.VmPolicy.Builder().detectNonSdkApiUsage().build());
            checkNonSdkApiUsageViolation(
                true, "dalvik.system.VMRuntime", "setHiddenApiExemptions", String[].class);
            // verify that mutliple uses of a light greylist API are detected.
            checkNonSdkApiUsageViolation(false, "dalvik.system.VMRuntime", "getRuntime");
            checkNonSdkApiUsageViolation(false, "dalvik.system.VMRuntime", "getRuntime");

            // Verify that the VM policy is turned off after a call to permitNonSdkApiUsage.
            StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().permitNonSdkApiUsage().build());
            assertNoViolation(() -> {
                  Class<?> clazz = Class.forName("dalvik.system.VMRuntime");
                  try {
                      clazz.getDeclaredMethod("getRuntime");
                  } catch (NoSuchMethodException maybe) {
                  }
            });
        } finally {
            StrictMode.setVmPolicy(oldVmPolicy);
            StrictMode.setThreadPolicy(oldThreadPolicy);
        }
    }

    @Test
    public void testThreadPenaltyListener() throws Exception {
        final BlockingQueue<Violation> violations = new ArrayBlockingQueue<>(1);
        StrictMode.setThreadPolicy(
                new StrictMode.ThreadPolicy.Builder().detectCustomSlowCalls()
                        .penaltyListener(getContext().getMainExecutor(), (v) -> {
                            violations.add(v);
                        }).build());

        StrictMode.noteSlowCall("foo");

        final Violation v = violations.poll(5, TimeUnit.SECONDS);
        assertTrue(v instanceof CustomViolation);
    }

    @Test
    public void testVmPenaltyListener() throws Exception {
        final BlockingQueue<Violation> violations = new ArrayBlockingQueue<>(1);
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectFileUriExposure()
                        .penaltyListener(getContext().getMainExecutor(), (v) -> {
                            violations.add(v);
                        }).build());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setDataAndType(Uri.fromFile(new File("/sdcard/meow.jpg")), "image/jpeg");
        getContext().startActivity(intent);

        final Violation v = violations.poll(5, TimeUnit.SECONDS);
        assertTrue(v instanceof FileUriExposedViolation);
    }

    @AppModeInstant
    @Test
    public void testNoCleartextHttpTrafficAllowed() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testNoCleartextHttpTrafficAllowed() ignored on device without Internet");
            return;
        }

        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectCleartextNetwork().penaltyLog().build());

        try {
            inspectViolation(
                    () ->
                            ((HttpURLConnection) new URL("http://example.com/").openConnection())
                                    .getResponseCode(),
                    info -> assertThat(info.getViolationClass())
                            .isAssignableTo(CleartextNetworkViolation.class));
            fail("Instant app was able to send cleartext http traffic.");
        } catch (IOException ex) {
            // Expected
        }
    }

    @Test
    public void testIncorrectContextUse_GetSystemService() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder()
                        .detectIncorrectContextUse()
                        .penaltyLog()
                        .build());

        final String wmClassName = WindowManager.class.getSimpleName();
        inspectViolation(
                () -> getContext().getApplicationContext().getSystemService(WindowManager.class),
                info -> assertThat(info.getStackTrace()).contains(
                        "Tried to access visual service " + wmClassName));

        final Display display = getContext().getSystemService(DisplayManager.class)
                .getDisplay(DEFAULT_DISPLAY);
        final Context visualContext = getContext().createDisplayContext(display)
                .createWindowContext(TYPE_APPLICATION_OVERLAY, null /* options */);
        assertNoViolation(() -> visualContext.getSystemService(WINDOW_SERVICE));

        Intent intent = new Intent(getContext(), SimpleTestActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final Activity activity = InstrumentationRegistry.getInstrumentation()
                .startActivitySync(intent);
        assertNoViolation(() -> activity.getSystemService(WINDOW_SERVICE));

        // TODO(b/159593676): move the logic to CtsInputMethodTestCases
        verifyIms(VERIFY_MODE_GET_WINDOW_MANAGER);
    }

    @Test
    public void testIncorrectContextUse_GetDisplay() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder()
                        .detectIncorrectContextUse()
                        .penaltyLog()
                        .build());

        final Display display = getContext().getSystemService(DisplayManager.class)
                .getDisplay(DEFAULT_DISPLAY);

        final Context displayContext = getContext().createDisplayContext(display);
        assertNoViolation(displayContext::getDisplay);

        final Context windowContext =
                displayContext.createWindowContext(TYPE_APPLICATION_OVERLAY, null /* options */);
        assertNoViolation(windowContext::getDisplay);

        Intent intent = new Intent(getContext(), SimpleTestActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Activity activity = InstrumentationRegistry.getInstrumentation()
                .startActivitySync(intent);
        assertNoViolation(() -> activity.getDisplay());

        // TODO(b/159593676): move the logic to CtsInputMethodTestCases
        verifyIms(VERIFY_MODE_GET_DISPLAY);
        try {
            getContext().getApplicationContext().getDisplay();
        } catch (UnsupportedOperationException e) {
            return;
        }
        fail("Expected to get incorrect use exception from calling getDisplay() on Application");
    }

    @Test
    public void testIncorrectContextUse_GetViewConfiguration() throws Exception {
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder()
                        .detectIncorrectContextUse()
                        .penaltyLog()
                        .build());

        final Context baseContext = getContext();
        assertViolation(
                "Tried to access UI constants from a non-visual Context:",
                () -> ViewConfiguration.get(baseContext));

        final Display display = baseContext.getSystemService(DisplayManager.class)
                .getDisplay(DEFAULT_DISPLAY);
        final Context displayContext = baseContext.createDisplayContext(display);
        assertViolation(
                "Tried to access UI constants from a non-visual Context:",
                () -> ViewConfiguration.get(displayContext));

        final Context windowContext =
                displayContext.createWindowContext(TYPE_APPLICATION_OVERLAY, null /* options */);
        assertNoViolation(() -> ViewConfiguration.get(windowContext));

        Intent intent = new Intent(baseContext, SimpleTestActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final Activity activity = InstrumentationRegistry.getInstrumentation()
                .startActivitySync(intent);
        assertNoViolation(() -> ViewConfiguration.get(activity));

        // TODO(b/159593676): move the logic to CtsInputMethodTestCases
        verifyIms(VERIFY_MODE_GET_VIEW_CONFIGURATION);
    }

    // TODO(b/159593676): move the logic to CtsInputMethodTestCases
    /**
     * Verify if APIs violates incorrect context violations by {@code mode}.
     *
     * @see VerifyMode
     */
    private void verifyIms(@VerifyMode int mode) throws Exception {
        // If devices do not support installable IMEs, finish the test gracefully. We don't use
        // assumeTrue here because we do pass some cases, so showing "pass" instead of "skip" makes
        // sense here.
        // TODO(b/160143006): re-enable IMS part test.
        if (!supportsInstallableIme() || DISABLE_VERIFY_IMS) {
            return;
        }

        try (final MockImeSession imeSession = MockImeSession.create(getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder().setStrictModeEnabled(true))) {
            final ImeEventStream stream = imeSession.openEventStream();
            expectEvent(stream, event -> "onStartInput".equals(event.getEventName()), TIMEOUT);
            final ImeEventStream forkedStream = clearAllEvents(stream, "onStrictModeViolated");
            final ImeEvent imeEvent;
            switch (mode) {
                case VERIFY_MODE_GET_DISPLAY:
                    imeEvent = expectCommand(forkedStream, imeSession.callVerifyGetDisplay(),
                            TIMEOUT);
                    break;
                case VERIFY_MODE_GET_WINDOW_MANAGER:
                    imeEvent = expectCommand(forkedStream, imeSession.callVerifyGetWindowManager(),
                            TIMEOUT);
                    break;
                case VERIFY_MODE_GET_VIEW_CONFIGURATION:
                    imeEvent = expectCommand(forkedStream,
                            imeSession.callVerifyGetViewConfiguration(), TIMEOUT);
                    break;
                default:
                    imeEvent = null;
            }
            assertTrue(imeEvent.getReturnBooleanValue());
            notExpectEvent(stream, event -> "onStrictModeViolated".equals(event.getEventName()),
                    NOT_EXPECT_TIMEOUT);
        }
    }

    private boolean supportsInstallableIme() {
        return getContext().getPackageManager().hasSystemFeature(FEATURE_INPUT_METHODS);
    }

    private static void runWithRemoteServiceBound(Context context, Consumer<ISecondary> consumer)
            throws ExecutionException, InterruptedException, RemoteException {
        BlockingQueue<IBinder> binderHolder = new ArrayBlockingQueue<>(1);
        ServiceConnection secondaryConnection =
                new ServiceConnection() {
                    public void onServiceConnected(ComponentName className, IBinder service) {
                        binderHolder.add(service);
                    }

                    public void onServiceDisconnected(ComponentName className) {
                        binderHolder.drainTo(new ArrayList<>());
                    }
                };
        Intent intent = new Intent(REMOTE_SERVICE_ACTION);
        intent.setPackage(context.getPackageName());

        Intent secondaryIntent = new Intent(ISecondary.class.getName());
        secondaryIntent.setPackage(context.getPackageName());
        assertThat(
                        context.bindService(
                                secondaryIntent, secondaryConnection, Context.BIND_AUTO_CREATE))
                .isTrue();
        IBinder binder = binderHolder.take();
        assertThat(binder.queryLocalInterface(binder.getInterfaceDescriptor())).isNull();
        consumer.accept(ISecondary.Stub.asInterface(binder));
        context.unbindService(secondaryConnection);
        context.stopService(intent);
    }

    private static void assertViolation(String expected, ThrowingRunnable r) throws Exception {
        inspectViolation(r, info -> assertThat(info.getStackTrace()).contains(expected));
    }

    private static void assertNoViolation(ThrowingRunnable r) throws Exception {
        inspectViolation(
                r, info -> assertWithMessage("Unexpected violation").that(info).isNull());
    }

    private static void inspectViolation(
            ThrowingRunnable violating, Consumer<ViolationInfo> consume) throws Exception {
        final LinkedBlockingQueue<ViolationInfo> violations = new LinkedBlockingQueue<>();
        StrictMode.setViolationLogger(violations::add);

        try {
            violating.run();
            consume.accept(violations.poll(5, TimeUnit.SECONDS));
        } finally {
            StrictMode.setViolationLogger(null);
        }
    }

    private boolean hasInternetConnection() {
        final PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)
                || pm.hasSystemFeature(PackageManager.FEATURE_WIFI)
                || pm.hasSystemFeature(PackageManager.FEATURE_ETHERNET);
    }
}
