/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.content.cts;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.app.Activity;
import android.app.AppOpsManager;
import android.app.Instrumentation;
import android.app.WallpaperManager;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.content.res.Resources.NotFoundException;
import android.content.res.Resources.Theme;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.database.Cursor;
import android.database.sqlite.SQLiteCursorDriver;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQuery;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.os.UserHandle;
import android.platform.test.annotations.AppModeFull;
import android.preference.PreferenceManager;
import android.test.AndroidTestCase;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Xml;
import android.view.WindowManager;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;
import com.android.cts.IBinderPermissionTestService;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@AppModeFull // TODO(Instant) Figure out which APIs should work.
public class ContextTest extends AndroidTestCase {
    private static final String TAG = "ContextTest";
    private static final String ACTUAL_RESULT = "ResultSetByReceiver";

    private static final String INTIAL_RESULT = "IntialResult";

    private static final String VALUE_ADDED = "ValueAdded";
    private static final String KEY_ADDED = "AddedByReceiver";

    private static final String VALUE_REMOVED = "ValueWillBeRemove";
    private static final String KEY_REMOVED = "ToBeRemoved";

    private static final String VALUE_KEPT = "ValueKept";
    private static final String KEY_KEPT = "ToBeKept";

    private static final String MOCK_STICKY_ACTION = "android.content.cts.ContextTest."
            + "STICKY_BROADCAST_RESULT";

    private static final String ACTION_BROADCAST_TESTORDER =
            "android.content.cts.ContextTest.BROADCAST_TESTORDER";
    private final static String MOCK_ACTION1 = ACTION_BROADCAST_TESTORDER + "1";
    private final static String MOCK_ACTION2 = ACTION_BROADCAST_TESTORDER + "2";

    // Note: keep these constants in sync with the permissions used by BinderPermissionTestService.
    //
    // A permission that's granted to this test package.
    public static final String GRANTED_PERMISSION = "android.permission.USE_CREDENTIALS";
    // A permission that's not granted to this test package.
    public static final String NOT_GRANTED_PERMISSION = "android.permission.HARDWARE_TEST";

    private static final int BROADCAST_TIMEOUT = 10000;
    private static final int ROOT_UID = 0;

    private Object mLockObj;

    private ArrayList<BroadcastReceiver> mRegisteredReceiverList;

    private boolean mWallpaperChanged;
    private BitmapDrawable mOriginalWallpaper;
    private volatile IBinderPermissionTestService mBinderPermissionTestService;
    private ServiceConnection mBinderPermissionTestConnection;

    protected Context mContext;

    /**
     * Returns the Context object that's being tested.
     */
    protected Context getContextUnderTest() {
        return getContext();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getContextUnderTest();
        mContext.setTheme(R.style.Test_Theme);

        mLockObj = new Object();

        mRegisteredReceiverList = new ArrayList<BroadcastReceiver>();

        mOriginalWallpaper = (BitmapDrawable) mContext.getWallpaper();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mWallpaperChanged) {
            mContext.setWallpaper(mOriginalWallpaper.getBitmap());
        }

        for (BroadcastReceiver receiver : mRegisteredReceiverList) {
            mContext.unregisterReceiver(receiver);
        }

        super.tearDown();
    }

    public void testGetString() {
        String testString = mContext.getString(R.string.context_test_string1);
        assertEquals("This is %s string.", testString);

        testString = mContext.getString(R.string.context_test_string1, "expected");
        assertEquals("This is expected string.", testString);

        testString = mContext.getString(R.string.context_test_string2);
        assertEquals("This is test string.", testString);

        // Test wrong resource id
        try {
            testString = mContext.getString(0, "expected");
            fail("Wrong resource id should not be accepted.");
        } catch (NotFoundException e) {
        }

        // Test wrong resource id
        try {
            testString = mContext.getString(0);
            fail("Wrong resource id should not be accepted.");
        } catch (NotFoundException e) {
        }
    }

    public void testGetText() {
        CharSequence testCharSequence = mContext.getText(R.string.context_test_string2);
        assertEquals("This is test string.", testCharSequence.toString());

        // Test wrong resource id
        try {
            testCharSequence = mContext.getText(0);
            fail("Wrong resource id should not be accepted.");
        } catch (NotFoundException e) {
        }
    }

    /**
     * Ensure that default and device encrypted storage areas are stored
     * separately on disk. All devices must support these storage areas, even if
     * they don't have file-based encryption, so that apps can go through a
     * backup/restore cycle between FBE and non-FBE devices.
     */
    public void testCreateDeviceProtectedStorageContext() throws Exception {
        final Context deviceContext = mContext.createDeviceProtectedStorageContext();

        assertFalse(mContext.isDeviceProtectedStorage());
        assertTrue(deviceContext.isDeviceProtectedStorage());

        final File defaultFile = new File(mContext.getFilesDir(), "test");
        final File deviceFile = new File(deviceContext.getFilesDir(), "test");

        assertFalse(deviceFile.equals(defaultFile));

        deviceFile.createNewFile();

        // Make sure storage areas are mutually exclusive
        assertFalse(defaultFile.exists());
        assertTrue(deviceFile.exists());
    }

    public void testMoveSharedPreferencesFrom() throws Exception {
        final Context deviceContext = mContext.createDeviceProtectedStorageContext();

        mContext.getSharedPreferences("test", Context.MODE_PRIVATE).edit().putInt("answer", 42)
                .commit();

        // Verify that we can migrate
        assertTrue(deviceContext.moveSharedPreferencesFrom(mContext, "test"));
        assertEquals(0, mContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));
        assertEquals(42, deviceContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));

        // Trying to migrate again when already done is a no-op
        assertTrue(deviceContext.moveSharedPreferencesFrom(mContext, "test"));
        assertEquals(0, mContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));
        assertEquals(42, deviceContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));

        // Add a new value and verify that we can migrate back
        deviceContext.getSharedPreferences("test", Context.MODE_PRIVATE).edit()
                .putInt("question", 24).commit();

        assertTrue(mContext.moveSharedPreferencesFrom(deviceContext, "test"));
        assertEquals(42, mContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));
        assertEquals(24, mContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("question", 0));
        assertEquals(0, deviceContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("answer", 0));
        assertEquals(0, deviceContext.getSharedPreferences("test", Context.MODE_PRIVATE)
                .getInt("question", 0));
    }

    public void testMoveDatabaseFrom() throws Exception {
        final Context deviceContext = mContext.createDeviceProtectedStorageContext();

        SQLiteDatabase db = mContext.openOrCreateDatabase("test.db",
                Context.MODE_PRIVATE | Context.MODE_ENABLE_WRITE_AHEAD_LOGGING, null);
        db.execSQL("CREATE TABLE list(item TEXT);");
        db.execSQL("INSERT INTO list VALUES ('cat')");
        db.execSQL("INSERT INTO list VALUES ('dog')");
        db.close();

        // Verify that we can migrate
        assertTrue(deviceContext.moveDatabaseFrom(mContext, "test.db"));
        db = deviceContext.openOrCreateDatabase("test.db",
                Context.MODE_PRIVATE | Context.MODE_ENABLE_WRITE_AHEAD_LOGGING, null);
        Cursor c = db.query("list", null, null, null, null, null, null);
        assertEquals(2, c.getCount());
        assertTrue(c.moveToFirst());
        assertEquals("cat", c.getString(0));
        assertTrue(c.moveToNext());
        assertEquals("dog", c.getString(0));
        c.close();
        db.execSQL("INSERT INTO list VALUES ('mouse')");
        db.close();

        // Trying to migrate again when already done is a no-op
        assertTrue(deviceContext.moveDatabaseFrom(mContext, "test.db"));

        // Verify that we can migrate back
        assertTrue(mContext.moveDatabaseFrom(deviceContext, "test.db"));
        db = mContext.openOrCreateDatabase("test.db",
                Context.MODE_PRIVATE | Context.MODE_ENABLE_WRITE_AHEAD_LOGGING, null);
        c = db.query("list", null, null, null, null, null, null);
        assertEquals(3, c.getCount());
        assertTrue(c.moveToFirst());
        assertEquals("cat", c.getString(0));
        assertTrue(c.moveToNext());
        assertEquals("dog", c.getString(0));
        assertTrue(c.moveToNext());
        assertEquals("mouse", c.getString(0));
        c.close();
        db.close();
    }

    public void testAccessTheme() {
        mContext.setTheme(R.style.Test_Theme);
        final Theme testTheme = mContext.getTheme();
        assertNotNull(testTheme);

        int[] attrs = {
            android.R.attr.windowNoTitle,
            android.R.attr.panelColorForeground,
            android.R.attr.panelColorBackground
        };
        TypedArray attrArray = null;
        try {
            attrArray = testTheme.obtainStyledAttributes(attrs);
            assertTrue(attrArray.getBoolean(0, false));
            assertEquals(0xff000000, attrArray.getColor(1, 0));
            assertEquals(0xffffffff, attrArray.getColor(2, 0));
        } finally {
            if (attrArray != null) {
                attrArray.recycle();
                attrArray = null;
            }
        }

        // setTheme only works for the first time
        mContext.setTheme(android.R.style.Theme_Black);
        assertSame(testTheme, mContext.getTheme());
    }

    public void testObtainStyledAttributes() {
        // Test obtainStyledAttributes(int[])
        TypedArray testTypedArray = mContext
                .obtainStyledAttributes(android.R.styleable.View);
        assertNotNull(testTypedArray);
        assertTrue(testTypedArray.length() > 2);
        assertTrue(testTypedArray.length() > 0);
        testTypedArray.recycle();

        // Test obtainStyledAttributes(int, int[])
        testTypedArray = mContext.obtainStyledAttributes(android.R.style.TextAppearance_Small,
                android.R.styleable.TextAppearance);
        assertNotNull(testTypedArray);
        assertTrue(testTypedArray.length() > 2);
        testTypedArray.recycle();

        // Test wrong null array pointer
        try {
            testTypedArray = mContext.obtainStyledAttributes(-1, null);
            fail("obtainStyledAttributes will throw a NullPointerException here.");
        } catch (NullPointerException e) {
        }

        // Test obtainStyledAttributes(AttributeSet, int[]) with unavailable resource id.
        int testInt[] = { 0, 0 };
        testTypedArray = mContext.obtainStyledAttributes(-1, testInt);
        // fail("Wrong resource id should not be accepted.");
        assertNotNull(testTypedArray);
        assertEquals(2, testTypedArray.length());
        testTypedArray.recycle();

        // Test obtainStyledAttributes(AttributeSet, int[])
        int[] attrs = android.R.styleable.DatePicker;
        testTypedArray = mContext.obtainStyledAttributes(getAttributeSet(R.layout.context_layout),
                attrs);
        assertNotNull(testTypedArray);
        assertEquals(attrs.length, testTypedArray.length());
        testTypedArray.recycle();

        // Test obtainStyledAttributes(AttributeSet, int[], int, int)
        testTypedArray = mContext.obtainStyledAttributes(getAttributeSet(R.layout.context_layout),
                attrs, 0, 0);
        assertNotNull(testTypedArray);
        assertEquals(attrs.length, testTypedArray.length());
        testTypedArray.recycle();
    }

    public void testGetSystemService() {
        // Test invalid service name
        assertNull(mContext.getSystemService("invalid"));

        // Test valid service name
        assertNotNull(mContext.getSystemService(Context.WINDOW_SERVICE));
    }

    public void testGetSystemServiceByClass() {
        // Test invalid service class
        assertNull(mContext.getSystemService(Object.class));

        // Test valid service name
        assertNotNull(mContext.getSystemService(WindowManager.class));
        assertEquals(mContext.getSystemService(Context.WINDOW_SERVICE),
                mContext.getSystemService(WindowManager.class));
    }

    public void testGetColorStateList() {
        try {
            mContext.getColorStateList(0);
            fail("Failed at testGetColorStateList");
        } catch (NotFoundException e) {
            //expected
        }

        final ColorStateList colorStateList = mContext.getColorStateList(R.color.color2);
        final int[] focusedState = {android.R.attr.state_focused};
        final int focusColor = colorStateList.getColorForState(focusedState, R.color.failColor);
        assertEquals(0xffff0000, focusColor);
    }

    public void testGetColor() {
        try {
            mContext.getColor(0);
            fail("Failed at testGetColor");
        } catch (NotFoundException e) {
            //expected
        }

        final int color = mContext.getColor(R.color.color2);
        assertEquals(0xffffff00, color);
    }

    /**
     * Developers have come to expect at least ext4-style filename behavior, so
     * verify that the underlying filesystem supports them.
     */
    public void testFilenames() throws Exception {
        final File base = mContext.getFilesDir();
        assertValidFile(new File(base, "foo"));
        assertValidFile(new File(base, ".bar"));
        assertValidFile(new File(base, "foo.bar"));
        assertValidFile(new File(base, "\u2603"));
        assertValidFile(new File(base, "\uD83D\uDCA9"));

        final int pid = android.os.Process.myPid();
        final StringBuilder sb = new StringBuilder(255);
        while (sb.length() <= 255) {
            sb.append(pid);
            sb.append(mContext.getPackageName());
        }
        sb.setLength(255);

        final String longName = sb.toString();
        final File longDir = new File(base, longName);
        assertValidFile(longDir);
        longDir.mkdir();
        final File longFile = new File(longDir, longName);
        assertValidFile(longFile);
    }

    public void testMainLooper() throws Exception {
        final Thread mainThread = Looper.getMainLooper().getThread();
        final Handler handler = new Handler(mContext.getMainLooper());
        handler.post(() -> {
            assertEquals(mainThread, Thread.currentThread());
        });
    }

    public void testMainExecutor() throws Exception {
        final Thread mainThread = Looper.getMainLooper().getThread();
        mContext.getMainExecutor().execute(() -> {
            assertEquals(mainThread, Thread.currentThread());
        });
    }

    private void assertValidFile(File file) throws Exception {
        Log.d(TAG, "Checking " + file);
        if (file.exists()) {
            assertTrue("File already exists and couldn't be deleted before test: " + file,
                    file.delete());
        }
        assertTrue("Failed to create " + file, file.createNewFile());
        assertTrue("Doesn't exist after create " + file, file.exists());
        assertTrue("Failed to delete after create " + file, file.delete());
        new FileOutputStream(file).close();
        assertTrue("Doesn't exist after stream " + file, file.exists());
        assertTrue("Failed to delete after stream " + file, file.delete());
    }

    static void beginDocument(XmlPullParser parser, String firstElementName)
            throws XmlPullParserException, IOException
    {
        int type;
        while ((type=parser.next()) != parser.START_TAG
                && type != parser.END_DOCUMENT) {
            ;
        }

        if (type != parser.START_TAG) {
            throw new XmlPullParserException("No start tag found");
        }

        if (!parser.getName().equals(firstElementName)) {
            throw new XmlPullParserException("Unexpected start tag: found " + parser.getName() +
                    ", expected " + firstElementName);
        }
    }

    private AttributeSet getAttributeSet(int resourceId) {
        final XmlResourceParser parser = mContext.getResources().getXml(
                resourceId);

        try {
            beginDocument(parser, "RelativeLayout");
        } catch (XmlPullParserException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }

        final AttributeSet attr = Xml.asAttributeSet(parser);
        assertNotNull(attr);
        return attr;
    }

    private void registerBroadcastReceiver(BroadcastReceiver receiver, IntentFilter filter) {
        mContext.registerReceiver(receiver, filter);

        mRegisteredReceiverList.add(receiver);
    }

    public void testSendOrderedBroadcast1() throws InterruptedException {
        final HighPriorityBroadcastReceiver highPriorityReceiver =
                new HighPriorityBroadcastReceiver();
        final LowPriorityBroadcastReceiver lowPriorityReceiver =
                new LowPriorityBroadcastReceiver();

        final IntentFilter filterHighPriority = new IntentFilter(ResultReceiver.MOCK_ACTION);
        filterHighPriority.setPriority(1);
        final IntentFilter filterLowPriority = new IntentFilter(ResultReceiver.MOCK_ACTION);
        registerBroadcastReceiver(highPriorityReceiver, filterHighPriority);
        registerBroadcastReceiver(lowPriorityReceiver, filterLowPriority);

        final Intent broadcastIntent = new Intent(ResultReceiver.MOCK_ACTION);
        broadcastIntent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        mContext.sendOrderedBroadcast(broadcastIntent, null);
        new PollingCheck(BROADCAST_TIMEOUT) {
            @Override
            protected boolean check() {
                return highPriorityReceiver.hasReceivedBroadCast()
                        && !lowPriorityReceiver.hasReceivedBroadCast();
            }
        }.run();

        synchronized (highPriorityReceiver) {
            highPriorityReceiver.notify();
        }

        new PollingCheck(BROADCAST_TIMEOUT) {
            @Override
            protected boolean check() {
                return highPriorityReceiver.hasReceivedBroadCast()
                        && lowPriorityReceiver.hasReceivedBroadCast();
            }
        }.run();
    }

    public void testSendOrderedBroadcast2() throws InterruptedException {
        final TestBroadcastReceiver broadcastReceiver = new TestBroadcastReceiver();
        broadcastReceiver.mIsOrderedBroadcasts = true;

        Bundle bundle = new Bundle();
        bundle.putString(KEY_KEPT, VALUE_KEPT);
        bundle.putString(KEY_REMOVED, VALUE_REMOVED);
        Intent intent = new Intent(ResultReceiver.MOCK_ACTION);
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        mContext.sendOrderedBroadcast(intent, null, broadcastReceiver, null, 1,
                INTIAL_RESULT, bundle);

        synchronized (mLockObj) {
            try {
                mLockObj.wait(BROADCAST_TIMEOUT);
            } catch (InterruptedException e) {
                fail("unexpected InterruptedException.");
            }
        }

        assertTrue("Receiver didn't make any response.", broadcastReceiver.hadReceivedBroadCast());
        assertEquals("Incorrect code: " + broadcastReceiver.getResultCode(), 3,
                broadcastReceiver.getResultCode());
        assertEquals(ACTUAL_RESULT, broadcastReceiver.getResultData());
        Bundle resultExtras = broadcastReceiver.getResultExtras(false);
        assertEquals(VALUE_ADDED, resultExtras.getString(KEY_ADDED));
        assertEquals(VALUE_KEPT, resultExtras.getString(KEY_KEPT));
        assertNull(resultExtras.getString(KEY_REMOVED));
    }

    public void testSendOrderedBroadcastWithAppOp() {
        // we use a HighPriorityBroadcastReceiver because the final receiver should get the
        // broadcast only at the end.
        final ResultReceiver receiver = new HighPriorityBroadcastReceiver();
        final ResultReceiver finalReceiver = new ResultReceiver();

        AppOpsManager aom =
                (AppOpsManager) getContextUnderTest().getSystemService(Context.APP_OPS_SERVICE);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(aom,
                (appOpsMan) -> appOpsMan.setUidMode(AppOpsManager.OPSTR_READ_CELL_BROADCASTS,
                Process.myUid(), AppOpsManager.MODE_ALLOWED));

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendOrderedBroadcast(
                new Intent(ResultReceiver.MOCK_ACTION),
                null, // permission
                AppOpsManager.OPSTR_READ_CELL_BROADCASTS,
                finalReceiver,
                null, // scheduler
                0, // initial code
                null, //initial data
                null); // initial extras

        new PollingCheck(BROADCAST_TIMEOUT){
            @Override
            protected boolean check() {
                return receiver.hasReceivedBroadCast()
                        && !finalReceiver.hasReceivedBroadCast();
            }
        }.run();

        synchronized (receiver) {
            receiver.notify();
        }

        new PollingCheck(BROADCAST_TIMEOUT){
            @Override
            protected boolean check() {
                // ensure that first receiver has received broadcast before final receiver
                return receiver.hasReceivedBroadCast()
                        && finalReceiver.hasReceivedBroadCast();
            }
        }.run();
    }

    public void testSendOrderedBroadcastWithAppOp_NotGranted() {
        final ResultReceiver receiver = new ResultReceiver();

        AppOpsManager aom =
                (AppOpsManager) getContextUnderTest().getSystemService(Context.APP_OPS_SERVICE);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(aom,
                (appOpsMan) -> appOpsMan.setUidMode(AppOpsManager.OPSTR_READ_CELL_BROADCASTS,
                        Process.myUid(), AppOpsManager.MODE_ERRORED));

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendOrderedBroadcast(
                new Intent(ResultReceiver.MOCK_ACTION),
                null, // permission
                AppOpsManager.OPSTR_READ_CELL_BROADCASTS,
                null, // final receiver
                null, // scheduler
                0, // initial code
                null, //initial data
                null); // initial extras

        boolean broadcastNeverSent = false;
        try {
            new PollingCheck(BROADCAST_TIMEOUT) {
                @Override
                protected boolean check() {
                    return receiver.hasReceivedBroadCast();
                }

                public void runWithInterruption() throws InterruptedException {
                    if (check()) {
                        return;
                    }

                    long timeout = BROADCAST_TIMEOUT;
                    while (timeout > 0) {
                        try {
                            Thread.sleep(50 /* time slice */);
                        } catch (InterruptedException e) {
                            fail("unexpected InterruptedException");
                        }

                        if (check()) {
                            return;
                        }

                        timeout -= 50; // time slice
                    }
                    throw new InterruptedException();
                }
            }.runWithInterruption();
        } catch (InterruptedException e) {
            broadcastNeverSent = true;
        }

        assertTrue(broadcastNeverSent);
    }

    public void testRegisterReceiver1() throws InterruptedException {
        final FilteredReceiver broadcastReceiver = new FilteredReceiver();
        final IntentFilter filter = new IntentFilter(MOCK_ACTION1);

        // Test registerReceiver
        mContext.registerReceiver(broadcastReceiver, filter);

        // Test unwanted intent(action = MOCK_ACTION2)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION2);
        assertFalse(broadcastReceiver.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        // Send wanted intent(action = MOCK_ACTION1)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION1);
        assertTrue(broadcastReceiver.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        mContext.unregisterReceiver(broadcastReceiver);

        // Test unregisterReceiver
        FilteredReceiver broadcastReceiver2 = new FilteredReceiver();
        mContext.registerReceiver(broadcastReceiver2, filter);
        mContext.unregisterReceiver(broadcastReceiver2);

        // Test unwanted intent(action = MOCK_ACTION2)
        broadcastReceiver2.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION2);
        assertFalse(broadcastReceiver2.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver2.hadReceivedBroadCast2());

        // Send wanted intent(action = MOCK_ACTION1), but the receiver is unregistered.
        broadcastReceiver2.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION1);
        assertFalse(broadcastReceiver2.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver2.hadReceivedBroadCast2());
    }

    public void testRegisterReceiver2() throws InterruptedException {
        FilteredReceiver broadcastReceiver = new FilteredReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(MOCK_ACTION1);

        // Test registerReceiver
        mContext.registerReceiver(broadcastReceiver, filter, null, null);

        // Test unwanted intent(action = MOCK_ACTION2)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION2);
        assertFalse(broadcastReceiver.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        // Send wanted intent(action = MOCK_ACTION1)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION1);
        assertTrue(broadcastReceiver.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        mContext.unregisterReceiver(broadcastReceiver);
    }

    public void testRegisterReceiverForAllUsers() throws InterruptedException {
        FilteredReceiver broadcastReceiver = new FilteredReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(MOCK_ACTION1);

        // Test registerReceiverForAllUsers without permission: verify SecurityException.
        try {
            mContext.registerReceiverForAllUsers(broadcastReceiver, filter, null, null);
            fail("testRegisterReceiverForAllUsers: "
                    + "SecurityException expected on registerReceiverForAllUsers");
        } catch (SecurityException se) {
            // expected
        }

        // Test registerReceiverForAllUsers with permission.
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mContext,
                    (ctx) -> ctx.registerReceiverForAllUsers(broadcastReceiver, filter, null, null)
            );
        } catch (SecurityException se) {
            fail("testRegisterReceiverForAllUsers: SecurityException not expected");
        }

        // Test unwanted intent(action = MOCK_ACTION2)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION2);
        assertFalse(broadcastReceiver.hadReceivedBroadCast1());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        // Send wanted intent(action = MOCK_ACTION1)
        broadcastReceiver.reset();
        waitForFilteredIntent(mContext, MOCK_ACTION1);
        assertTrue(broadcastReceiver.hadReceivedBroadCast1());
        assertEquals(broadcastReceiver.getSendingUser(), Process.myUserHandle());
        assertFalse(broadcastReceiver.hadReceivedBroadCast2());

        mContext.unregisterReceiver(broadcastReceiver);
    }

    public void testAccessWallpaper() throws IOException, InterruptedException {
        if (!isWallpaperSupported()) return;

        // set Wallpaper by context#setWallpaper(Bitmap)
        Bitmap bitmap = Bitmap.createBitmap(20, 30, Bitmap.Config.RGB_565);
        // Test getWallpaper
        Drawable testDrawable = mContext.getWallpaper();
        // Test peekWallpaper
        Drawable testDrawable2 = mContext.peekWallpaper();

        mContext.setWallpaper(bitmap);
        mWallpaperChanged = true;
        synchronized(this) {
            wait(500);
        }

        assertNotSame(testDrawable, mContext.peekWallpaper());
        assertNotNull(mContext.getWallpaper());
        assertNotSame(testDrawable2, mContext.peekWallpaper());
        assertNotNull(mContext.peekWallpaper());

        // set Wallpaper by context#setWallpaper(InputStream)
        mContext.clearWallpaper();

        testDrawable = mContext.getWallpaper();
        InputStream stream = mContext.getResources().openRawResource(R.drawable.scenery);

        mContext.setWallpaper(stream);
        synchronized (this) {
            wait(1000);
        }

        assertNotSame(testDrawable, mContext.peekWallpaper());
    }

    public void testAccessDatabase() {
        String DATABASE_NAME = "databasetest";
        String DATABASE_NAME1 = DATABASE_NAME + "1";
        String DATABASE_NAME2 = DATABASE_NAME + "2";
        SQLiteDatabase mDatabase;
        File mDatabaseFile;

        SQLiteDatabase.CursorFactory factory = new SQLiteDatabase.CursorFactory() {
            public Cursor newCursor(SQLiteDatabase db, SQLiteCursorDriver masterQuery,
                                    String editTable, SQLiteQuery query) {
                return new android.database.sqlite.SQLiteCursor(db, masterQuery, editTable, query) {
                    @Override
                    public boolean requery() {
                        setSelectionArguments(new String[] { "2" });
                        return super.requery();
                    }
                };
            }
        };

        // FIXME: Move cleanup into tearDown()
        for (String db : mContext.databaseList()) {
            File f = mContext.getDatabasePath(db);
            if (f.exists()) {
                mContext.deleteDatabase(db);
            }
        }

        // Test openOrCreateDatabase with null and actual factory
        mDatabase = mContext.openOrCreateDatabase(DATABASE_NAME1,
                Context.MODE_ENABLE_WRITE_AHEAD_LOGGING, factory);
        assertNotNull(mDatabase);
        mDatabase.close();
        mDatabase = mContext.openOrCreateDatabase(DATABASE_NAME2,
                Context.MODE_ENABLE_WRITE_AHEAD_LOGGING, factory);
        assertNotNull(mDatabase);
        mDatabase.close();

        // Test getDatabasePath
        File actualDBPath = mContext.getDatabasePath(DATABASE_NAME1);

        // Test databaseList()
        List<String> list = Arrays.asList(mContext.databaseList());
        assertTrue("1) database list: " + list, list.contains(DATABASE_NAME1));
        assertTrue("2) database list: " + list, list.contains(DATABASE_NAME2));

        // Test deleteDatabase()
        for (int i = 1; i < 3; i++) {
            mDatabaseFile = mContext.getDatabasePath(DATABASE_NAME + i);
            assertTrue(mDatabaseFile.exists());
            mContext.deleteDatabase(DATABASE_NAME + i);
            mDatabaseFile = new File(actualDBPath, DATABASE_NAME + i);
            assertFalse(mDatabaseFile.exists());
        }
    }

    public void testEnforceUriPermission1() {
        try {
            Uri uri = Uri.parse("content://ctstest");
            mContext.enforceUriPermission(uri, Binder.getCallingPid(),
                    Binder.getCallingUid(), Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                    "enforceUriPermission is not working without possessing an IPC.");
            fail("enforceUriPermission is not working without possessing an IPC.");
        } catch (SecurityException e) {
            // If the function is OK, it should throw a SecurityException here because currently no
            // IPC is handled by this process.
        }
    }

    public void testEnforceUriPermission2() {
        Uri uri = Uri.parse("content://ctstest");
        try {
            mContext.enforceUriPermission(uri, NOT_GRANTED_PERMISSION,
                    NOT_GRANTED_PERMISSION, Binder.getCallingPid(), Binder.getCallingUid(),
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                    "enforceUriPermission is not working without possessing an IPC.");
            fail("enforceUriPermission is not working without possessing an IPC.");
        } catch (SecurityException e) {
            // If the function is ok, it should throw a SecurityException here because currently no
            // IPC is handled by this process.
        }
    }

    public void testGetPackageResourcePath() {
        assertNotNull(mContext.getPackageResourcePath());
    }

    public void testStartActivityWithActivityNotFound() {
        Intent intent = new Intent(mContext, ContextCtsActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            mContext.startActivity(intent);
            fail("Test startActivity should throw a ActivityNotFoundException here.");
        } catch (ActivityNotFoundException e) {
            // Because ContextWrapper is a wrapper class, so no need to test
            // the details of the function's performance. Getting a result
            // from the wrapped class is enough for testing.
        }
    }

    public void testStartActivities() throws Exception {
        final Intent[] intents = {
                new Intent().setComponent(new ComponentName(mContext,
                        AvailableIntentsActivity.class)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                new Intent().setComponent(new ComponentName(mContext,
                        ImageCaptureActivity.class)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        };

        final Instrumentation.ActivityMonitor firstMonitor = getInstrumentation()
                .addMonitor(AvailableIntentsActivity.class.getName(), null /* result */,
                        false /* block */);
        final Instrumentation.ActivityMonitor secondMonitor = getInstrumentation()
                .addMonitor(ImageCaptureActivity.class.getName(), null /* result */,
                        false /* block */);

        mContext.startActivities(intents);

        Activity firstActivity = getInstrumentation().waitForMonitorWithTimeout(firstMonitor, 5000);
        assertNotNull(firstActivity);

        Activity secondActivity = getInstrumentation().waitForMonitorWithTimeout(secondMonitor,
                5000);
        assertNotNull(secondActivity);
    }

    public void testStartActivityAsUser() {
        try (ActivitySession activitySession = new ActivitySession()) {
            Intent intent = new Intent(mContext, AvailableIntentsActivity.class);

            activitySession.assertActivityLaunched(intent.getComponent().getClassName(),
                    () -> SystemUtil.runWithShellPermissionIdentity(() ->
                            mContext.startActivityAsUser(intent, UserHandle.CURRENT)));
        }
    }

    public void testStartActivity()  {
        try (ActivitySession activitySession = new ActivitySession()) {
            Intent intent = new Intent(mContext, AvailableIntentsActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            activitySession.assertActivityLaunched(intent.getComponent().getClassName(),
                    () -> mContext.startActivity(intent));
        }
    }

    /**
     * Helper class to launch / close test activity.
     */
    private class ActivitySession implements AutoCloseable {
        private Activity mTestActivity;
        private static final int ACTIVITY_LAUNCH_TIMEOUT = 5000;

        void assertActivityLaunched(String activityClassName, Runnable activityStarter) {
            final Instrumentation.ActivityMonitor monitor = getInstrumentation()
                    .addMonitor(activityClassName, null /* result */,
                            false /* block */);
            activityStarter.run();
            // Wait for activity launch with timeout.
            mTestActivity = getInstrumentation().waitForMonitorWithTimeout(monitor,
                    ACTIVITY_LAUNCH_TIMEOUT);
            assertNotNull(mTestActivity);
        }

        @Override
        public void close() {
            if (mTestActivity != null) {
                mTestActivity.finishAndRemoveTask();
            }
        }
    }

    public void testCreatePackageContext() throws PackageManager.NameNotFoundException {
        Context actualContext = mContext.createPackageContext(getValidPackageName(),
                Context.CONTEXT_IGNORE_SECURITY);

        assertNotNull(actualContext);
    }

    public void testCreatePackageContextAsUser() throws Exception {
        for (UserHandle user : new UserHandle[] {
                android.os.Process.myUserHandle(),
                UserHandle.ALL, UserHandle.CURRENT, UserHandle.SYSTEM
        }) {
            assertEquals(user, mContext
                    .createPackageContextAsUser(getValidPackageName(), 0, user).getUser());
        }
    }

    public void testCreateContextAsUser() throws Exception {
        for (UserHandle user : new UserHandle[] {
                android.os.Process.myUserHandle(),
                UserHandle.ALL, UserHandle.CURRENT, UserHandle.SYSTEM
        }) {
            assertEquals(user, mContext.createContextAsUser(user, 0).getUser());
        }
    }

    /**
     * Helper method to retrieve a valid application package name to use for tests.
     */
    protected String getValidPackageName() {
        List<PackageInfo> packages = mContext.getPackageManager().getInstalledPackages(
                PackageManager.GET_ACTIVITIES);
        assertTrue(packages.size() >= 1);
        return packages.get(0).packageName;
    }

    public void testGetMainLooper() {
        assertNotNull(mContext.getMainLooper());
    }

    public void testGetApplicationContext() {
        assertSame(mContext.getApplicationContext(), mContext.getApplicationContext());
    }

    public void testGetSharedPreferences() {
        SharedPreferences sp;
        SharedPreferences localSP;

        sp = PreferenceManager.getDefaultSharedPreferences(mContext);
        String packageName = mContext.getPackageName();
        localSP = mContext.getSharedPreferences(packageName + "_preferences",
                Context.MODE_PRIVATE);
        assertSame(sp, localSP);
    }

    public void testRevokeUriPermission() {
        Uri uri = Uri.parse("contents://ctstest");
        mContext.revokeUriPermission(uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
    }

    public void testAccessService() throws InterruptedException {
        MockContextService.reset();
        bindExpectResult(mContext, new Intent(mContext, MockContextService.class));

        // Check startService
        assertTrue(MockContextService.hadCalledOnStart());
        // Check bindService
        assertTrue(MockContextService.hadCalledOnBind());

        assertTrue(MockContextService.hadCalledOnDestory());
        // Check unbinService
        assertTrue(MockContextService.hadCalledOnUnbind());
    }

    public void testGetPackageCodePath() {
        assertNotNull(mContext.getPackageCodePath());
    }

    public void testGetPackageName() {
        assertEquals("android.content.cts", mContext.getPackageName());
    }

    public void testGetCacheDir() {
        assertNotNull(mContext.getCacheDir());
    }

    public void testGetContentResolver() {
        assertSame(mContext.getContentResolver(), mContext.getContentResolver());
    }

    public void testGetFileStreamPath() {
        String TEST_FILENAME = "TestGetFileStreamPath";

        // Test the path including the input filename
        String fileStreamPath = mContext.getFileStreamPath(TEST_FILENAME).toString();
        assertTrue(fileStreamPath.indexOf(TEST_FILENAME) >= 0);
    }

    public void testGetClassLoader() {
        assertSame(mContext.getClassLoader(), mContext.getClassLoader());
    }

    public void testGetWallpaperDesiredMinimumHeightAndWidth() {
        if (!isWallpaperSupported()) return;

        int height = mContext.getWallpaperDesiredMinimumHeight();
        int width = mContext.getWallpaperDesiredMinimumWidth();

        // returned value is <= 0, the caller should use the height of the
        // default display instead.
        // That is to say, the return values of desired minimumHeight and
        // minimunWidth are at the same side of 0-dividing line.
        assertTrue((height > 0 && width > 0) || (height <= 0 && width <= 0));
    }

    public void testAccessStickyBroadcast() throws InterruptedException {
        ResultReceiver resultReceiver = new ResultReceiver();

        Intent intent = new Intent(MOCK_STICKY_ACTION);
        TestBroadcastReceiver stickyReceiver = new TestBroadcastReceiver();

        mContext.sendStickyBroadcast(intent);

        waitForReceiveBroadCast(resultReceiver);

        assertEquals(intent.getAction(), mContext.registerReceiver(stickyReceiver,
                new IntentFilter(MOCK_STICKY_ACTION)).getAction());

        synchronized (mLockObj) {
            mLockObj.wait(BROADCAST_TIMEOUT);
        }

        assertTrue("Receiver didn't make any response.", stickyReceiver.hadReceivedBroadCast());

        mContext.unregisterReceiver(stickyReceiver);
        mContext.removeStickyBroadcast(intent);

        assertNull(mContext.registerReceiver(stickyReceiver,
                new IntentFilter(MOCK_STICKY_ACTION)));
        mContext.unregisterReceiver(stickyReceiver);
    }

    public void testCheckCallingOrSelfUriPermission() {
        Uri uri = Uri.parse("content://ctstest");

        int retValue = mContext.checkCallingOrSelfUriPermission(uri,
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testGrantUriPermission() {
        mContext.grantUriPermission("com.android.mms", Uri.parse("contents://ctstest"),
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
    }

    public void testCheckPermissionGranted() {
        int returnValue = mContext.checkPermission(
                GRANTED_PERMISSION, Process.myPid(), Process.myUid());
        assertEquals(PackageManager.PERMISSION_GRANTED, returnValue);
    }

    public void testCheckPermissionNotGranted() {
        int returnValue = mContext.checkPermission(
                NOT_GRANTED_PERMISSION, Process.myPid(), Process.myUid());
        assertEquals(PackageManager.PERMISSION_DENIED, returnValue);
    }

    public void testCheckPermissionRootUser() {
        // Test with root user, everything will be granted.
        int returnValue = mContext.checkPermission(NOT_GRANTED_PERMISSION, 1, ROOT_UID);
        assertEquals(PackageManager.PERMISSION_GRANTED, returnValue);
    }

    public void testCheckPermissionInvalidRequest() {
        // Test with null permission.
        try {
            int returnValue = mContext.checkPermission(null, 0, ROOT_UID);
            fail("checkPermission should not accept null permission");
        } catch (IllegalArgumentException e) {
        }

        // Test with invalid uid and included granted permission.
        int returnValue = mContext.checkPermission(GRANTED_PERMISSION, 1, -11);
        assertEquals(PackageManager.PERMISSION_DENIED, returnValue);
    }

    public void testCheckSelfPermissionGranted() {
        int returnValue = mContext.checkSelfPermission(GRANTED_PERMISSION);
        assertEquals(PackageManager.PERMISSION_GRANTED, returnValue);
    }

    public void testCheckSelfPermissionNotGranted() {
        int returnValue = mContext.checkSelfPermission(NOT_GRANTED_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, returnValue);
    }

    public void testEnforcePermissionGranted() {
        mContext.enforcePermission(
                GRANTED_PERMISSION, Process.myPid(), Process.myUid(),
                "permission isn't granted");
    }

    public void testEnforcePermissionNotGranted() {
        try {
            mContext.enforcePermission(
                    NOT_GRANTED_PERMISSION, Process.myPid(), Process.myUid(),
                    "permission isn't granted");
            fail("Permission shouldn't be granted.");
        } catch (SecurityException expected) {
        }
    }

    public void testCheckCallingOrSelfPermission_noIpc() {
        // There's no ongoing Binder call, so this package's permissions are checked.
        int retValue = mContext.checkCallingOrSelfPermission(GRANTED_PERMISSION);
        assertEquals(PackageManager.PERMISSION_GRANTED, retValue);

        retValue = mContext.checkCallingOrSelfPermission(NOT_GRANTED_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testCheckCallingOrSelfPermission_ipc() throws Exception {
        bindBinderPermissionTestService();
        try {
            int retValue = mBinderPermissionTestService.doCheckCallingOrSelfPermission(
                    GRANTED_PERMISSION);
            assertEquals(PackageManager.PERMISSION_GRANTED, retValue);

            retValue = mBinderPermissionTestService.doCheckCallingOrSelfPermission(
                    NOT_GRANTED_PERMISSION);
            assertEquals(PackageManager.PERMISSION_DENIED, retValue);
        } finally {
            mContext.unbindService(mBinderPermissionTestConnection);
        }
    }

    public void testEnforceCallingOrSelfPermission_noIpc() {
        // There's no ongoing Binder call, so this package's permissions are checked.
        mContext.enforceCallingOrSelfPermission(
                GRANTED_PERMISSION, "permission isn't granted");

        try {
            mContext.enforceCallingOrSelfPermission(
                    NOT_GRANTED_PERMISSION, "permission isn't granted");
            fail("Permission shouldn't be granted.");
        } catch (SecurityException expected) {
        }
    }

    public void testEnforceCallingOrSelfPermission_ipc() throws Exception {
        bindBinderPermissionTestService();
        try {
            mBinderPermissionTestService.doEnforceCallingOrSelfPermission(GRANTED_PERMISSION);

            try {
                mBinderPermissionTestService.doEnforceCallingOrSelfPermission(
                        NOT_GRANTED_PERMISSION);
                fail("Permission shouldn't be granted.");
            } catch (SecurityException expected) {
            }
        } finally {
            mContext.unbindService(mBinderPermissionTestConnection);
        }
    }

    public void testCheckCallingPermission_noIpc() {
        // Denied because no IPC is active.
        int retValue = mContext.checkCallingPermission(GRANTED_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testEnforceCallingPermission_noIpc() {
        try {
            mContext.enforceCallingPermission(
                    GRANTED_PERMISSION,
                    "enforceCallingPermission is not working without possessing an IPC.");
            fail("enforceCallingPermission is not working without possessing an IPC.");
        } catch (SecurityException e) {
            // Currently no IPC is handled by this process, this exception is expected
        }
    }

    public void testEnforceCallingPermission_ipc() throws Exception {
        bindBinderPermissionTestService();
        try {
            mBinderPermissionTestService.doEnforceCallingPermission(GRANTED_PERMISSION);

            try {
                mBinderPermissionTestService.doEnforceCallingPermission(NOT_GRANTED_PERMISSION);
                fail("Permission shouldn't be granted.");
            } catch (SecurityException expected) {
            }
        } finally {
            mContext.unbindService(mBinderPermissionTestConnection);
        }
    }

    public void testCheckCallingPermission_ipc() throws Exception {
        bindBinderPermissionTestService();
        try {
            int returnValue = mBinderPermissionTestService.doCheckCallingPermission(
                    GRANTED_PERMISSION);
            assertEquals(PackageManager.PERMISSION_GRANTED, returnValue);

            returnValue = mBinderPermissionTestService.doCheckCallingPermission(
                    NOT_GRANTED_PERMISSION);
            assertEquals(PackageManager.PERMISSION_DENIED, returnValue);
        } finally {
            mContext.unbindService(mBinderPermissionTestConnection);
        }
    }

    private void bindBinderPermissionTestService() {
        Intent intent = new Intent(mContext, IBinderPermissionTestService.class);
        intent.setComponent(new ComponentName(
                "com.android.cts", "com.android.cts.BinderPermissionTestService"));

        mBinderPermissionTestConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
                mBinderPermissionTestService =
                        IBinderPermissionTestService.Stub.asInterface(iBinder);
            }

            @Override
            public void onServiceDisconnected(ComponentName componentName) {
            }
        };

        assertTrue("Service not bound", mContext.bindService(
                intent, mBinderPermissionTestConnection, Context.BIND_AUTO_CREATE));

        new PollingCheck(15 * 1000) {
            protected boolean check() {
                return mBinderPermissionTestService != null; // Service was bound.
            }
        }.run();
    }

    public void testCheckUriPermission1() {
        Uri uri = Uri.parse("content://ctstest");

        int retValue = mContext.checkUriPermission(uri, Binder.getCallingPid(), 0,
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_GRANTED, retValue);

        retValue = mContext.checkUriPermission(uri, Binder.getCallingPid(),
                Binder.getCallingUid(), Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testCheckUriPermission2() {
        Uri uri = Uri.parse("content://ctstest");

        int retValue = mContext.checkUriPermission(uri, NOT_GRANTED_PERMISSION,
                NOT_GRANTED_PERMISSION, Binder.getCallingPid(), 0,
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_GRANTED, retValue);

        retValue = mContext.checkUriPermission(uri, NOT_GRANTED_PERMISSION,
                NOT_GRANTED_PERMISSION, Binder.getCallingPid(), Binder.getCallingUid(),
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testCheckCallingUriPermission() {
        Uri uri = Uri.parse("content://ctstest");

        int retValue = mContext.checkCallingUriPermission(uri,
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        assertEquals(PackageManager.PERMISSION_DENIED, retValue);
    }

    public void testEnforceCallingUriPermission() {
        try {
            Uri uri = Uri.parse("content://ctstest");
            mContext.enforceCallingUriPermission(uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                    "enforceCallingUriPermission is not working without possessing an IPC.");
            fail("enforceCallingUriPermission is not working without possessing an IPC.");
        } catch (SecurityException e) {
            // If the function is OK, it should throw a SecurityException here because currently no
            // IPC is handled by this process.
        }
    }

    public void testGetDir() {
        File dir = mContext.getDir("testpath", Context.MODE_PRIVATE);
        assertNotNull(dir);
        dir.delete();
    }

    public void testGetPackageManager() {
        assertSame(mContext.getPackageManager(), mContext.getPackageManager());
    }

    public void testSendBroadcast1() throws InterruptedException {
        final ResultReceiver receiver = new ResultReceiver();

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendBroadcast(new Intent(ResultReceiver.MOCK_ACTION));

        new PollingCheck(BROADCAST_TIMEOUT){
            @Override
            protected boolean check() {
                return receiver.hasReceivedBroadCast();
            }
        }.run();
    }

    public void testSendBroadcast2() throws InterruptedException {
        final ResultReceiver receiver = new ResultReceiver();

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendBroadcast(new Intent(ResultReceiver.MOCK_ACTION), null);

        new PollingCheck(BROADCAST_TIMEOUT){
            @Override
            protected boolean check() {
                return receiver.hasReceivedBroadCast();
            }
        }.run();
    }

    /** The receiver should get the broadcast if it has all the permissions. */
    public void testSendBroadcastWithMultiplePermissions_receiverHasAllPermissions()
            throws Exception {
        final ResultReceiver receiver = new ResultReceiver();

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendBroadcastWithMultiplePermissions(
                new Intent(ResultReceiver.MOCK_ACTION),
                new String[] { // this test APK has both these permissions
                        android.Manifest.permission.ACCESS_WIFI_STATE,
                        android.Manifest.permission.ACCESS_NETWORK_STATE,
                });

        new PollingCheck(BROADCAST_TIMEOUT) {
            @Override
            protected boolean check() {
                return receiver.hasReceivedBroadCast();
            }
        }.run();
    }

    /** The receiver should not get the broadcast if it does not have all the permissions. */
    public void testSendBroadcastWithMultiplePermissions_receiverHasSomePermissions()
            throws Exception {
        final ResultReceiver receiver = new ResultReceiver();

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendBroadcastWithMultiplePermissions(
                new Intent(ResultReceiver.MOCK_ACTION),
                new String[] { // this test APK only has ACCESS_WIFI_STATE
                        android.Manifest.permission.ACCESS_WIFI_STATE,
                        android.Manifest.permission.NETWORK_STACK,
                });

        Thread.sleep(BROADCAST_TIMEOUT);
        assertFalse(receiver.hasReceivedBroadCast());
    }

    /** The receiver should not get the broadcast if it has none of the permissions. */
    public void testSendBroadcastWithMultiplePermissions_receiverHasNoPermissions()
            throws Exception {
        final ResultReceiver receiver = new ResultReceiver();

        registerBroadcastReceiver(receiver, new IntentFilter(ResultReceiver.MOCK_ACTION));

        mContext.sendBroadcastWithMultiplePermissions(
                new Intent(ResultReceiver.MOCK_ACTION),
                new String[] { // this test APK has neither of these permissions
                        android.Manifest.permission.NETWORK_SETTINGS,
                        android.Manifest.permission.NETWORK_STACK,
                });

        Thread.sleep(BROADCAST_TIMEOUT);
        assertFalse(receiver.hasReceivedBroadCast());
    }

    public void testEnforceCallingOrSelfUriPermission() {
        try {
            Uri uri = Uri.parse("content://ctstest");
            mContext.enforceCallingOrSelfUriPermission(uri,
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                    "enforceCallingOrSelfUriPermission is not working without possessing an IPC.");
            fail("enforceCallingOrSelfUriPermission is not working without possessing an IPC.");
        } catch (SecurityException e) {
            // If the function is OK, it should throw a SecurityException here because currently no
            // IPC is handled by this process.
        }
    }

    public void testGetAssets() {
        assertSame(mContext.getAssets(), mContext.getAssets());
    }

    public void testGetResources() {
        assertSame(mContext.getResources(), mContext.getResources());
    }

    public void testStartInstrumentation() {
        // Use wrong name
        ComponentName cn = new ComponentName("com.android",
                "com.android.content.FalseLocalSampleInstrumentation");
        assertNotNull(cn);
        assertNotNull(mContext);
        // If the target instrumentation is wrong, the function should return false.
        assertFalse(mContext.startInstrumentation(cn, null, null));
    }

    private void bindExpectResult(Context context, Intent service)
            throws InterruptedException {
        if (service == null) {
            fail("No service created!");
        }
        TestConnection conn = new TestConnection(true, false);

        context.bindService(service, conn, Context.BIND_AUTO_CREATE);
        context.startService(service);

        // Wait for a short time, so the service related operations could be
        // working.
        synchronized (this) {
            wait(2500);
        }
        // Test stop Service
        assertTrue(context.stopService(service));
        context.unbindService(conn);

        synchronized (this) {
            wait(1000);
        }
    }

    private interface Condition {
        public boolean onCondition();
    }

    private synchronized void waitForCondition(Condition con) throws InterruptedException {
        // check the condition every 1 second until the condition is fulfilled
        // and wait for 3 seconds at most
        for (int i = 0; !con.onCondition() && i <= 3; i++) {
            wait(1000);
        }
    }

    private void waitForReceiveBroadCast(final ResultReceiver receiver)
            throws InterruptedException {
        Condition con = new Condition() {
            public boolean onCondition() {
                return receiver.hasReceivedBroadCast();
            }
        };
        waitForCondition(con);
    }

    private void waitForFilteredIntent(Context context, final String action)
            throws InterruptedException {
        context.sendBroadcast(new Intent(action), null);

        synchronized (mLockObj) {
            mLockObj.wait(BROADCAST_TIMEOUT);
        }
    }

    private final class TestBroadcastReceiver extends BroadcastReceiver {
        boolean mHadReceivedBroadCast;
        boolean mIsOrderedBroadcasts;

        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (this) {
                if (mIsOrderedBroadcasts) {
                    setResultCode(3);
                    setResultData(ACTUAL_RESULT);
                }

                Bundle map = getResultExtras(false);
                if (map != null) {
                    map.remove(KEY_REMOVED);
                    map.putString(KEY_ADDED, VALUE_ADDED);
                }
                mHadReceivedBroadCast = true;
                this.notifyAll();
            }

            synchronized (mLockObj) {
                mLockObj.notify();
            }
        }

        boolean hadReceivedBroadCast() {
            return mHadReceivedBroadCast;
        }

        void reset(){
            mHadReceivedBroadCast = false;
        }
    }

    private class FilteredReceiver extends BroadcastReceiver {
        private boolean mHadReceivedBroadCast1 = false;
        private boolean mHadReceivedBroadCast2 = false;

        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (MOCK_ACTION1.equals(action)) {
                mHadReceivedBroadCast1 = true;
            } else if (MOCK_ACTION2.equals(action)) {
                mHadReceivedBroadCast2 = true;
            }

            synchronized (mLockObj) {
                mLockObj.notify();
            }
        }

        public boolean hadReceivedBroadCast1() {
            return mHadReceivedBroadCast1;
        }

        public boolean hadReceivedBroadCast2() {
            return mHadReceivedBroadCast2;
        }

        public void reset(){
            mHadReceivedBroadCast1 = false;
            mHadReceivedBroadCast2 = false;
        }
    }

    private class TestConnection implements ServiceConnection {
        public TestConnection(boolean expectDisconnect, boolean setReporter) {
        }

        void setMonitor(boolean v) {
        }

        public void onServiceConnected(ComponentName name, IBinder service) {
        }

        public void onServiceDisconnected(ComponentName name) {
        }
    }

    public void testOpenFileOutput_mustNotCreateWorldReadableFile() throws Exception {
        try {
            mContext.openFileOutput("test.txt", Context.MODE_WORLD_READABLE);
            fail("Exception expected");
        } catch (SecurityException expected) {
        }
    }

    public void testOpenFileOutput_mustNotCreateWorldWriteableFile() throws Exception {
        try {
            mContext.openFileOutput("test.txt", Context.MODE_WORLD_WRITEABLE);
            fail("Exception expected");
        } catch (SecurityException expected) {
        }
    }

    public void testOpenFileOutput_mustNotWriteToParentDirectory() throws Exception {
        try {
            // Created files must be under the application's private directory.
            mContext.openFileOutput("../test.txt", Context.MODE_PRIVATE);
            fail("Exception expected");
        } catch (IllegalArgumentException expected) {
        }
    }

    public void testOpenFileOutput_mustNotUseAbsolutePath() throws Exception {
        try {
            // Created files must be under the application's private directory.
            mContext.openFileOutput("/tmp/test.txt", Context.MODE_PRIVATE);
            fail("Exception expected");
        } catch (IllegalArgumentException expected) {
        }
    }

    private boolean isWallpaperSupported() {
        return WallpaperManager.getInstance(mContext).isWallpaperSupported();
    }
}
