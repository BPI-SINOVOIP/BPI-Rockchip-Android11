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
 * limitations under the License.
 */

package android.app.appops.cts

import android.app.AppOpsManager
import android.app.AppOpsManager.MAX_PRIORITY_UID_STATE
import android.app.AppOpsManager.MIN_PRIORITY_UID_STATE
import android.app.AppOpsManager.OPSTR_WIFI_SCAN
import android.app.AppOpsManager.OP_FLAGS_ALL
import android.app.AppOpsManager.OP_FLAG_SELF
import android.app.AppOpsManager.OP_FLAG_TRUSTED_PROXIED
import android.app.AppOpsManager.OP_FLAG_TRUSTED_PROXY
import android.app.AppOpsManager.OP_FLAG_UNTRUSTED_PROXIED
import android.app.AppOpsManager.UID_STATE_TOP
import android.content.Intent
import android.content.Intent.ACTION_INSTALL_PACKAGE
import android.net.Uri
import android.os.SystemClock
import android.platform.test.annotations.AppModeFull
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import com.google.common.truth.Truth.assertThat
import org.junit.Assume.assumeNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.lang.Thread.sleep

private const val BACKGROUND_PACKAGE = "android.app.appops.cts.appinbackground"

class AppOpEventCollectionTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val appOpsManager = context.getSystemService(AppOpsManager::class.java)

    private val myUid = android.os.Process.myUid()
    private val myPackage = context.packageName

    // Start an activity to make sure this app counts as being in the foreground
    @Rule
    @JvmField
    var activityRule = ActivityTestRule(UidStateForceActivity::class.java)

    @Before
    fun wakeScreenUp() {
        val uiDevice = UiDevice.getInstance(instrumentation)
        uiDevice.wakeUp()
        uiDevice.executeShellCommand("wm dismiss-keyguard")
    }

    @Before
    fun makeSureTimeStampsAreDistinct() {
        sleep(1)
    }

    @Test
    fun switchUidStateWhileOpsAreRunning() {
        val before = System.currentTimeMillis()

        // Start twice to also test switching uid state with nested starts running
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)

        val beforeUidChange = System.currentTimeMillis()
        sleep(1)

        assertThat(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!
                .getLastAccessTime(MAX_PRIORITY_UID_STATE, UID_STATE_TOP, OP_FLAGS_ALL))
                .isIn(before..beforeUidChange)

        try {
            activityRule.activity.finish()
            UidStateForceActivity.waitForDestroyed()

            eventually {
                // The system remembers the time before and after the uid change as separate events
                assertThat(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!
                        .getLastAccessTime(UID_STATE_TOP + 1, MIN_PRIORITY_UID_STATE,
                        OP_FLAGS_ALL)).isAtLeast(beforeUidChange)
            }
        } finally {
            appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, null)
            appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, null)
        }
    }

    @Test
    fun noteWithAttributionAndCheckOpEntries() {
        val before = System.currentTimeMillis()
        appOpsManager.noteOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)
        val after = System.currentTimeMillis()

        val opEntry = getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!
        val attributionOpEntry = opEntry.attributedOpEntries[TEST_ATTRIBUTION_TAG]!!

        assertThat(attributionOpEntry.getLastAccessForegroundTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(opEntry.getLastAccessForegroundTime(OP_FLAG_SELF)).isIn(before..after)

        // Access should should also show up in the combined state for all op-flags
        assertThat(attributionOpEntry.getLastAccessForegroundTime(OP_FLAGS_ALL)).isIn(before..after)
        assertThat(opEntry.getLastAccessForegroundTime(OP_FLAGS_ALL)).isIn(before..after)

        // Foreground access should should also show up in the combined state for fg and bg
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(before..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(before..after)

        // The access was in foreground, hence there is no background access
        assertThat(attributionOpEntry.getLastAccessBackgroundTime(OP_FLAG_SELF)).isLessThan(before)
        assertThat(opEntry.getLastAccessBackgroundTime(OP_FLAG_SELF)).isLessThan(before)
        assertThat(attributionOpEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL)).isLessThan(before)
        assertThat(opEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL)).isLessThan(before)

        // The access was for a attribution, hence there is no access for the default attribution
        if (null in opEntry.attributedOpEntries) {
            assertThat(opEntry.attributedOpEntries[null]!!
                .getLastAccessForegroundTime(OP_FLAG_SELF)).isLessThan(before)
        }

        // The access does not show up for other op-flags
        assertThat(
            attributionOpEntry.getLastAccessForegroundTime(OP_FLAGS_ALL and OP_FLAG_SELF.inv())
        ).isLessThan(before)
        assertThat(opEntry.getLastAccessForegroundTime(OP_FLAGS_ALL and OP_FLAG_SELF.inv()))
            .isLessThan(before)
    }

    @AppModeFull(reason = "instant apps cannot see other packages")
    @Test
    fun noteInBackgroundWithAttributionAndCheckOpEntries() {
        val uid = context.packageManager.getPackageUid(BACKGROUND_PACKAGE, 0)

        val before = System.currentTimeMillis()
        assertThat(
            runWithShellPermissionIdentity {
                appOpsManager.noteOp(
                    OPSTR_WIFI_SCAN, uid, BACKGROUND_PACKAGE, TEST_ATTRIBUTION_TAG, null
                )
            }
        ).isEqualTo(AppOpsManager.MODE_ALLOWED)
        val after = System.currentTimeMillis()

        val opEntry = getOpEntry(uid, BACKGROUND_PACKAGE, OPSTR_WIFI_SCAN)!!
        val attributionOpEntry = opEntry.attributedOpEntries[TEST_ATTRIBUTION_TAG]!!

        assertThat(attributionOpEntry.getLastAccessBackgroundTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(opEntry.getLastAccessBackgroundTime(OP_FLAG_SELF)).isIn(before..after)

        // Access should should also show up in the combined state for all op-flags
        assertThat(attributionOpEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL)).isIn(before..after)
        assertThat(opEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL)).isIn(before..after)

        // Background access should should also show up in the combined state for fg and bg
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(before..after)
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(before..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(before..after)

        // The access was in background, hence there is no foreground access
        assertThat(attributionOpEntry.getLastAccessForegroundTime(OP_FLAG_SELF)).isLessThan(before)
        assertThat(opEntry.getLastAccessForegroundTime(OP_FLAG_SELF)).isLessThan(before)
        assertThat(attributionOpEntry.getLastAccessForegroundTime(OP_FLAGS_ALL)).isLessThan(before)
        assertThat(opEntry.getLastAccessForegroundTime(OP_FLAGS_ALL)).isLessThan(before)

        // The access was for a attribution, hence there is no access for the default attribution
        if (null in opEntry.attributedOpEntries) {
            assertThat(opEntry.attributedOpEntries[null]!!
                .getLastAccessBackgroundTime(OP_FLAG_SELF)).isLessThan(before)
        }

        // The access does not show up for other op-flags
        assertThat(
            attributionOpEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL and OP_FLAG_SELF.inv())
        ).isLessThan(before)
        assertThat(opEntry.getLastAccessBackgroundTime(OP_FLAGS_ALL and OP_FLAG_SELF.inv()))
            .isLessThan(before)
    }

    @Test
    fun noteSelfAndTrustedAccessAndCheckOpEntries() {
        val before = System.currentTimeMillis()

        // Using the shell identity causes a trusted proxy note
        runWithShellPermissionIdentity {
            appOpsManager.noteProxyOp(OPSTR_WIFI_SCAN, myPackage, myUid, null, null)
        }
        val afterTrusted = System.currentTimeMillis()

        // Make sure timestamps are distinct
        sleep(1)

        // self note
        appOpsManager.noteOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)
        val after = System.currentTimeMillis()

        val opEntry = getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!
        val attributionOpEntry = opEntry.attributedOpEntries[null]!!

        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAG_TRUSTED_PROXY))
                .isIn(before..afterTrusted)
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(afterTrusted..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAG_TRUSTED_PROXY)).isIn(before..afterTrusted)
        assertThat(opEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(afterTrusted..after)

        // When asked for any flags, the second access overrides the first
        assertThat(attributionOpEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(afterTrusted..after)
        assertThat(opEntry.getLastAccessTime(OP_FLAGS_ALL)).isIn(afterTrusted..after)
    }

    @Test
    fun noteForTwoAttributionsCheckOpEntries() {
        val before = System.currentTimeMillis()
        appOpsManager.noteOp(OPSTR_WIFI_SCAN, myUid, myPackage, "firstAttribution", null)
        val afterFirst = System.currentTimeMillis()

        // Make sure timestamps are distinct
        sleep(1)

        // self note
        appOpsManager.noteOp(OPSTR_WIFI_SCAN, myUid, myPackage, "secondAttribution", null)
        val after = System.currentTimeMillis()

        val opEntry = getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!
        val firstAttributionOpEntry = opEntry.attributedOpEntries["firstAttribution"]!!
        val secondAttributionOpEntry = opEntry.attributedOpEntries["secondAttribution"]!!

        assertThat(firstAttributionOpEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(before..afterFirst)
        assertThat(secondAttributionOpEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(afterFirst..after)

        // When asked for any attribution, the second access overrides the first
        assertThat(opEntry.getLastAccessTime(OP_FLAG_SELF)).isIn(afterFirst..after)
    }

    @AppModeFull(reason = "instant apps cannot see other packages")
    @Test
    fun noteFromTwoProxiesAndVerifyProxyInfo() {
        // Find another app to blame
        val otherAppInfo = context.packageManager
                .resolveActivity(Intent(ACTION_INSTALL_PACKAGE).addCategory(Intent.CATEGORY_DEFAULT)
                        .setDataAndType(Uri.parse("content://com.example/foo.apk"),
                                "application/vnd.android.package-archive"), 0)
                ?.activityInfo?.applicationInfo

        assumeNotNull(otherAppInfo)

        val otherPkg = otherAppInfo!!.packageName
        val otherUid = otherAppInfo.uid

        // Using the shell identity causes a trusted proxy note
        runWithShellPermissionIdentity {
            context.createAttributionContext("firstProxyAttribution")
                    .getSystemService(AppOpsManager::class.java)
                    .noteProxyOp(OPSTR_WIFI_SCAN, otherPkg, otherUid, null, null)
        }

        // Make sure timestamps are distinct
        sleep(1)

        // untrusted proxy note
        context.createAttributionContext("secondProxyAttribution")
                .getSystemService(AppOpsManager::class.java)
                .noteProxyOp(OPSTR_WIFI_SCAN, otherPkg, otherUid, null, null)

        val opEntry = getOpEntry(otherUid, otherPkg, OPSTR_WIFI_SCAN)!!
        val attributionOpEntry = opEntry.attributedOpEntries[null]!!

        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_TRUSTED_PROXIED)?.packageName)
                .isEqualTo(myPackage)
        assertThat(opEntry.getLastProxyInfo(OP_FLAG_TRUSTED_PROXIED)?.packageName)
                .isEqualTo(myPackage)
        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_TRUSTED_PROXIED)?.uid)
                .isEqualTo(myUid)
        assertThat(opEntry.getLastProxyInfo(OP_FLAG_TRUSTED_PROXIED)?.uid).isEqualTo(myUid)

        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_UNTRUSTED_PROXIED)?.packageName)
                .isEqualTo(myPackage)
        assertThat(opEntry.getLastProxyInfo(OP_FLAG_UNTRUSTED_PROXIED)?.packageName)
                .isEqualTo(myPackage)
        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_UNTRUSTED_PROXIED)?.uid)
                .isEqualTo(myUid)
        assertThat(opEntry.getLastProxyInfo(OP_FLAG_UNTRUSTED_PROXIED)?.uid).isEqualTo(myUid)

        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_TRUSTED_PROXIED)?.attributionTag)
                .isEqualTo("firstProxyAttribution")
        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAG_UNTRUSTED_PROXIED)?.attributionTag)
                .isEqualTo("secondProxyAttribution")

        // If asked for all op-flags the second attribution overrides the first
        assertThat(attributionOpEntry.getLastProxyInfo(OP_FLAGS_ALL)?.attributionTag)
                .isEqualTo("secondProxyAttribution")
    }

    @Test
    fun startStopMultipleOpsAndVerifyIsRunning() {
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isTrue()
            attributedOpEntries[TEST_ATTRIBUTION_TAG]?.let { assertThat(it.isRunning).isFalse() }
            assertThat(isRunning).isTrue()
        }

        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isTrue()
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.isRunning).isTrue()
            assertThat(isRunning).isTrue()
        }

        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isTrue()
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.isRunning).isTrue()
            assertThat(isRunning).isTrue()
        }

        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isTrue()
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.isRunning).isTrue()
            assertThat(isRunning).isTrue()
        }

        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isTrue()
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.isRunning).isFalse()
            assertThat(isRunning).isTrue()
        }

        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, null)

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.isRunning).isFalse()
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.isRunning).isFalse()
            assertThat(isRunning).isFalse()
        }
    }

    @Test
    fun startStopMultipleOpsAndVerifyLastAccess() {
        val beforeNullAttributionStart = System.currentTimeMillis()
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)
        val afterNullAttributionStart = System.currentTimeMillis()

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeNullAttributionStart..afterNullAttributionStart)
            attributedOpEntries[TEST_ATTRIBUTION_TAG]?.let {
                assertThat(it.getLastAccessTime(OP_FLAGS_ALL)).isAtMost(beforeNullAttributionStart)
            }
            assertThat(getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeNullAttributionStart..afterNullAttributionStart)
        }

        val beforeFirstAttributionStart = System.currentTimeMillis()
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)
        val afterFirstAttributionStart = System.currentTimeMillis()

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeNullAttributionStart..afterNullAttributionStart)
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeFirstAttributionStart..afterFirstAttributionStart)
            assertThat(getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeFirstAttributionStart..afterFirstAttributionStart)
        }

        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)

        // Nested startOps do _not_ count as another access
        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeNullAttributionStart..afterNullAttributionStart)
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeFirstAttributionStart..afterFirstAttributionStart)
            assertThat(getLastAccessTime(OP_FLAGS_ALL))
                    .isIn(beforeFirstAttributionStart..afterFirstAttributionStart)
        }

        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)
        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)
        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, null)
    }

    @Test
    fun startStopMultipleOpsAndVerifyDuration() {
        val beforeNullAttributionStart = SystemClock.elapsedRealtime()
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, null, null)
        val afterNullAttributionStart = SystemClock.elapsedRealtime()

        run {
            val beforeGetOp = SystemClock.elapsedRealtime()
            with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
                val afterGetOp = SystemClock.elapsedRealtime()

                assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
                assertThat(getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
            }
        }

        val beforeAttributionStart = SystemClock.elapsedRealtime()
        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)
        val afterAttributionStart = SystemClock.elapsedRealtime()

        run {
            val beforeGetOp = SystemClock.elapsedRealtime()
            with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
                val afterGetOp = SystemClock.elapsedRealtime()

                assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
                assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!
                        .getLastDuration(OP_FLAGS_ALL)).isIn(beforeGetOp -
                                afterAttributionStart..afterGetOp - beforeAttributionStart)

                // The last duration is the duration of the last started attribution
                assertThat(getLastDuration(OP_FLAGS_ALL)).isIn(beforeGetOp -
                        afterAttributionStart..afterGetOp - beforeAttributionStart)
            }
        }

        appOpsManager.startOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG, null)

        // Nested startOps do _not_ start another duration counting, hence the nested
        // startOp and finishOp calls have no affect
        run {
            val beforeGetOp = SystemClock.elapsedRealtime()
            with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
                val afterGetOp = SystemClock.elapsedRealtime()

                assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
                assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!
                        .getLastDuration(OP_FLAGS_ALL)).isIn(beforeGetOp -
                        afterAttributionStart..afterGetOp - beforeAttributionStart)
                assertThat(getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp -
                                afterAttributionStart..afterGetOp - beforeAttributionStart)
            }
        }

        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)

        run {
            val beforeGetOp = SystemClock.elapsedRealtime()
            with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
                val afterGetOp = SystemClock.elapsedRealtime()

                assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
                assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!
                        .getLastDuration(OP_FLAGS_ALL)).isIn(beforeGetOp -
                                afterAttributionStart..afterGetOp - beforeAttributionStart)
                assertThat(getLastDuration(OP_FLAGS_ALL)).isIn(beforeGetOp -
                                afterAttributionStart..afterGetOp - beforeAttributionStart)
            }
        }

        val beforeAttributionStop = SystemClock.elapsedRealtime()
        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, TEST_ATTRIBUTION_TAG)
        val afterAttributionStop = SystemClock.elapsedRealtime()

        run {
            val beforeGetOp = SystemClock.elapsedRealtime()
            with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
                val afterGetOp = SystemClock.elapsedRealtime()

                assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeGetOp - afterNullAttributionStart
                                ..afterGetOp - beforeNullAttributionStart)
                assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!
                        .getLastDuration(OP_FLAGS_ALL)).isIn(
                        beforeAttributionStop - afterAttributionStart
                                ..afterAttributionStop - beforeAttributionStart)
                assertThat(getLastDuration(OP_FLAGS_ALL))
                        .isIn(beforeAttributionStop - afterAttributionStart
                                ..afterAttributionStop - beforeAttributionStart)
            }
        }

        val beforeNullAttributionStop = SystemClock.elapsedRealtime()
        appOpsManager.finishOp(OPSTR_WIFI_SCAN, myUid, myPackage, null)
        val afterNullAttributionStop = SystemClock.elapsedRealtime()

        with(getOpEntry(myUid, myPackage, OPSTR_WIFI_SCAN)!!) {
            assertThat(attributedOpEntries[null]!!.getLastDuration(OP_FLAGS_ALL))
                    .isIn(beforeNullAttributionStop - afterNullAttributionStart
                            ..afterNullAttributionStop - beforeNullAttributionStart)
            assertThat(attributedOpEntries[TEST_ATTRIBUTION_TAG]!!.getLastDuration(OP_FLAGS_ALL))
                    .isIn(beforeAttributionStop - afterAttributionStart
                            ..afterAttributionStop - beforeAttributionStart)
            assertThat(getLastDuration(OP_FLAGS_ALL))
                    .isIn(beforeAttributionStop - afterAttributionStart
                            ..afterAttributionStop - beforeAttributionStart)
        }
    }
}
