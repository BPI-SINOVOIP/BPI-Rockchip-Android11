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
import android.app.AppOpsManager.OPSTR_WIFI_SCAN
import android.app.AppOpsManager.OP_FLAGS_ALL
import android.platform.test.annotations.AppModeFull
import androidx.test.platform.app.InstrumentationRegistry
import com.google.common.truth.Truth.assertThat
import org.junit.Before
import org.junit.Test
import java.lang.AssertionError
import java.lang.Thread.sleep

private const val APK_PATH = "/data/local/tmp/cts/appops/"

private const val APP_PKG = "android.app.appops.cts.apptoblame"

private const val ATTRIBUTION_1 = "attribution1"
private const val ATTRIBUTION_2 = "attribution2"
private const val ATTRIBUTION_3 = "attribution3"
private const val ATTRIBUTION_4 = "attribution4"
private const val ATTRIBUTION_5 = "attribution5"
private const val ATTRIBUTION_6 = "attribution6"
private const val ATTRIBUTION_7 = "attribution7"

@AppModeFull(reason = "Test relies on seeing other apps. Instant apps can't see other apps")
class AttributionTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val appOpsManager = context.getSystemService(AppOpsManager::class.java)
    private val appUid by lazy { context.packageManager.getPackageUid(APP_PKG, 0) }

    private fun installApk(apk: String) {
        val result = runCommand("pm install -r --force-queryable $APK_PATH$apk")
        assertThat(result.trim()).isEqualTo("Success")
    }

    @Before
    fun resetTestApp() {
        runCommand("pm uninstall $APP_PKG")
        installApk("CtsAppToBlame1.apk")
    }

    private fun noteForAttribution(attribution: String) {
        // Make sure note times as distinct
        sleep(1)

        runWithShellPermissionIdentity {
            appOpsManager.noteOpNoThrow(OPSTR_WIFI_SCAN, appUid, APP_PKG, attribution, null)
        }
    }

    @Test
    fun inheritNotedAppOpsOnUpgrade() {
        noteForAttribution(ATTRIBUTION_1)
        noteForAttribution(ATTRIBUTION_2)
        noteForAttribution(ATTRIBUTION_3)
        noteForAttribution(ATTRIBUTION_4)
        noteForAttribution(ATTRIBUTION_5)

        val beforeUpdate = getOpEntry(appUid, APP_PKG, OPSTR_WIFI_SCAN)!!
        installApk("CtsAppToBlame2.apk")

        eventually {
            val afterUpdate = getOpEntry(appUid, APP_PKG, OPSTR_WIFI_SCAN)!!

            // Attribution 1 is unchanged
            assertThat(afterUpdate.attributedOpEntries[ATTRIBUTION_1]!!
                    .getLastAccessTime(OP_FLAGS_ALL))
                    .isEqualTo(beforeUpdate.attributedOpEntries[ATTRIBUTION_1]!!
                            .getLastAccessTime(OP_FLAGS_ALL))

            // Attribution 3 disappeared (i.e. was added into "null" attribution)
            assertThat(afterUpdate.attributedOpEntries[null]!!.getLastAccessTime(OP_FLAGS_ALL))
                    .isEqualTo(beforeUpdate.attributedOpEntries[ATTRIBUTION_3]!!
                            .getLastAccessTime(OP_FLAGS_ALL))

            // Attribution 6 inherits from attribution 2
            assertThat(afterUpdate.attributedOpEntries[ATTRIBUTION_6]!!
                    .getLastAccessTime(OP_FLAGS_ALL))
                    .isEqualTo(beforeUpdate.attributedOpEntries[ATTRIBUTION_2]!!
                            .getLastAccessTime(OP_FLAGS_ALL))

            // Attribution 7 inherits from attribution 4 and 5. 5 was noted after 4, hence 4 is
            // removed
            assertThat(afterUpdate.attributedOpEntries[ATTRIBUTION_7]!!
                    .getLastAccessTime(OP_FLAGS_ALL))
                    .isEqualTo(beforeUpdate.attributedOpEntries[ATTRIBUTION_5]!!
                            .getLastAccessTime(OP_FLAGS_ALL))
        }
    }

    @Test(expected = AssertionError::class)
    fun cannotInheritFromSelf() {
        installApk("AppWithAttributionInheritingFromSelf.apk")
    }

    @Test(expected = AssertionError::class)
    fun noDuplicateAttributions() {
        installApk("AppWithDuplicateAttribution.apk")
    }

    @Test(expected = AssertionError::class)
    fun cannotInheritFromExisting() {
        installApk("AppWithAttributionInheritingFromExisting.apk")
    }

    @Test(expected = AssertionError::class)
    fun cannotInheritFromSameAsOther() {
        installApk("AppWithAttributionInheritingFromSameAsOther.apk")
    }

    @Test(expected = AssertionError::class)
    fun cannotUseVeryLongAttributionTags() {
        installApk("AppWithLongAttributionTag.apk")
    }

    @Test(expected = AssertionError::class)
    fun cannotUseTooManyAttributions() {
        installApk("AppWithTooManyAttributions.apk")
    }
}