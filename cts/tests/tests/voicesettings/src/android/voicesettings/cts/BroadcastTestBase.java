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

package android.voicesettings.cts;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.util.Log;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.BroadcastUtils;
import com.android.compatibility.common.util.SettingsStateChangerRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public abstract class BroadcastTestBase {

    static final String TAG = "BroadcastTestBase";
    protected static final int TIMEOUT_MS = 20 * 1000;

    protected final Context mContext = getInstrumentation().getTargetContext();
    protected Bundle mResultExtras;
    private CountDownLatch mLatch;
    protected ActivityDoneReceiver mActivityDoneReceiver = null;
    private BroadcastTestStartActivity mActivity;
    private BroadcastUtils.TestcaseType mTestCaseType;
    protected boolean mHasFeature;

    private final SettingsStateChangerRule mServiceSetterRule = new SettingsStateChangerRule(
            mContext, Settings.Secure.VOICE_INTERACTION_SERVICE,
            "android.voicesettings.service/.MainInteractionService");
    private final ActivityTestRule<BroadcastTestStartActivity> mActivityTestRule =
            new ActivityTestRule<>(BroadcastTestStartActivity.class, false, false);

    @Rule
    public final RuleChain mgRules = RuleChain
            .outerRule(mServiceSetterRule)
            .around(mActivityTestRule);

    @Before
    public void setUp() throws Exception {
        mHasFeature = false;

        customSetup();
    }

    @After
    public final void tearDown() throws Exception {
        Log.v(TAG, getClass().getSimpleName() + ".tearDown(): hasFeature=" + mHasFeature
                + " receiver=" + mActivityDoneReceiver);

        if (mHasFeature && mActivityDoneReceiver != null) {
            try {
                mContext.unregisterReceiver(mActivityDoneReceiver);
            } catch (IllegalArgumentException e) {
                // This exception is thrown if mActivityDoneReceiver in
                // the above call to unregisterReceiver is never registered.
                // If so, no harm done by ignoring this exception.
            }
            mActivityDoneReceiver = null;
        }
    }

    /**
     * Test-specific setup - doesn't need to call {@code super} neither use <code>@Before</code>.
     */
    protected void customSetup() throws Exception {
    }

    protected boolean isIntentSupported(String intentStr) {
        Intent intent = new Intent(intentStr);
        final PackageManager manager = mContext.getPackageManager();
        assertThat(manager).isNotNull();
        if (manager.resolveActivity(intent, 0) == null) {
            Log.i(TAG, "No Activity found for the intent: " + intentStr);
            return false;
        }
        return true;
    }

    protected void startTestActivity(String intentSuffix) {
        Intent intent = new Intent();
        intent.setAction("android.intent.action.TEST_START_ACTIVITY_" + intentSuffix);
        intent.setComponent(new ComponentName(mContext, BroadcastTestStartActivity.class));
        mActivity = mActivityTestRule.launchActivity(intent);
    }

    protected void registerBroadcastReceiver(BroadcastUtils.TestcaseType testCaseType)
            throws Exception {
        mTestCaseType = testCaseType;
        mLatch = new CountDownLatch(1);
        mActivityDoneReceiver = new ActivityDoneReceiver();
        mContext.registerReceiver(mActivityDoneReceiver,
                new IntentFilter(BroadcastUtils.BROADCAST_INTENT + testCaseType.toString()));
    }

    protected boolean startTestAndWaitForBroadcast(BroadcastUtils.TestcaseType testCaseType,
            String pkg, String cls) throws Exception {
        Log.i(TAG, "Begin Testing: " + testCaseType);
        registerBroadcastReceiver(testCaseType);
        mActivity.startTest(testCaseType.toString(), pkg, cls);
        if (!mLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Failed to receive broadcast in " + TIMEOUT_MS + "msec");
            return false;
        }
        return true;
    }

    class ActivityDoneReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(
                    BroadcastUtils.BROADCAST_INTENT +
                            BroadcastTestBase.this.mTestCaseType.toString())) {
                Bundle extras = intent.getExtras();
                Log.i(TAG, "received_broadcast for " + BroadcastUtils.toBundleString(extras));
                BroadcastTestBase.this.mResultExtras = extras;
                mLatch.countDown();
            }
        }
    }

    protected CountDownLatch registerForChanges(Uri uri) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final ContentResolver resolver = mActivity.getContentResolver();
        mActivity.runOnUiThread(() -> {
            resolver.registerContentObserver(uri, true,
                    new ContentObserver(new Handler()) {
                        @Override
                        public void onChange(boolean selfChange) {
                            latch.countDown();
                            resolver.unregisterContentObserver(this);
                        }
                    });
        });
        return latch;
    }

    protected boolean startTestAndWaitForChange(BroadcastUtils.TestcaseType testCaseType, Uri uri,
            String pkg, String cls)
            throws Exception {
        Log.i(TAG, "Begin Testing: " + testCaseType);

        // We also wait for broadcast because some results are obtained by
        // ActivityDoneReceiver#onReceive
        registerBroadcastReceiver(testCaseType);

        CountDownLatch latch = registerForChanges(uri);
        mActivity.startTest(testCaseType.toString(), pkg, cls);
        if (!mLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)
                || !latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            fail("Failed to change in " + TIMEOUT_MS + "msec");
            return false;
        }
        return true;
    }
}
