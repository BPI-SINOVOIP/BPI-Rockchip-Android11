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

package android.app.appops.cts

import org.junit.Assert.fail
import android.app.AppOpsManager
import android.platform.test.annotations.AppModeFull
import androidx.test.platform.app.InstrumentationRegistry
import com.android.internal.util.FrameworkStatsLog.RUNTIME_APP_OP_ACCESS__SAMPLING_STRATEGY__UNIFORM
import com.google.common.truth.Truth.assertThat
import org.junit.Before
import org.junit.Test
import java.lang.Thread.sleep

private const val APK_PATH = "/data/local/tmp/cts/appops/"

private const val APP_PKG = "android.app.appops.cts.apptocollect"

@AppModeFull(reason = "Test relies on seeing other apps. Instant apps can't see other apps")
class RuntimeMessageCollectionTest {
    private val TIMEOUT_MILLIS = 5000L
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val appOpsManager = context.getSystemService(AppOpsManager::class.java)
    private var appUid = -1

    private fun installApk(apk: String) {
        val result = runCommand("pm install -r -g --force-queryable $APK_PATH$apk")
        assertThat(result.trim()).isEqualTo("Success")
        appUid = context.packageManager.getPackageUid(APP_PKG, 0)
    }

    @Before
    fun resetTestApp() {
        runCommand("pm uninstall $APP_PKG")
        installApk("CtsAppToCollect.apk")
    }

    @Test
    fun collectAsyncStackTrace() {
        for (attempt in 0..12) {
            installApk("CtsAppToCollect.apk")
            val start = System.currentTimeMillis()
            runWithShellPermissionIdentity {
                appOpsManager.noteOp(AppOpsManager.OPSTR_READ_CONTACTS, appUid, APP_PKG,
                        TEST_ATTRIBUTION_TAG, null)
            }
            while (System.currentTimeMillis() - start < TIMEOUT_MILLIS) {
                sleep(200)

                runWithShellPermissionIdentity {
                    val message = appOpsManager.collectRuntimeAppOpAccessMessage()
                    if (message != null && message.packageName.equals(APP_PKG) &&
                            message.samplingStrategy !=
                            RUNTIME_APP_OP_ACCESS__SAMPLING_STRATEGY__UNIFORM) {
                        assertThat(message.op).isEqualTo(AppOpsManager.OPSTR_READ_CONTACTS)
                        assertThat(message.uid).isEqualTo(appUid)
                        assertThat(message.attributionTag).isEqualTo(TEST_ATTRIBUTION_TAG)
                        assertThat(message.message).isNotNull()
                        return
                    }
                }
            }
        }
        fail("App Op use message was not collected")
    }
}
