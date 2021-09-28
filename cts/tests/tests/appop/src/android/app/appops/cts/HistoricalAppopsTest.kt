/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.app.appops.cts

import android.app.AppOpsManager
import android.app.AppOpsManager.HistoricalOp
import android.app.AppOpsManager.HistoricalOps
import android.app.AppOpsManager.OPSTR_REQUEST_DELETE_PACKAGES
import android.app.AppOpsManager.OP_FLAGS_ALL
import android.os.Process
import android.os.SystemClock
import android.provider.DeviceConfig
import androidx.test.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.AndroidJUnit4
import androidx.test.uiautomator.UiDevice
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.time.Instant
import java.util.concurrent.TimeUnit
import java.util.concurrent.locks.ReentrantLock
import java.util.function.Consumer

const val PROPERTY_PERMISSIONS_HUB_ENABLED = "permissions_hub_enabled"

@RunWith(AndroidJUnit4::class)
class HistoricalAppopsTest {
    private val uid = Process.myUid()
    private lateinit var appOpsManager: AppOpsManager
    private lateinit var packageName: String

    private var wasPermissionsHubEnabled = false

    // Start an activity to make sure this app counts as being in the foreground
    @Rule @JvmField
    var activityRule = ActivityTestRule(UidStateForceActivity::class.java)

    @Before
    fun wakeScreenUp() {
        val uiDevice = UiDevice.getInstance(instrumentation)
        uiDevice.wakeUp()
        uiDevice.executeShellCommand("wm dismiss-keyguard")
    }

    @Before
    fun setUpTest() {
        appOpsManager = context.getSystemService(AppOpsManager::class.java)!!
        packageName = context.packageName!!
        runWithShellPermissionIdentity {
            wasPermissionsHubEnabled = DeviceConfig.getBoolean(DeviceConfig.NAMESPACE_PRIVACY,
                    PROPERTY_PERMISSIONS_HUB_ENABLED, false)
            DeviceConfig.setProperty(DeviceConfig.NAMESPACE_PRIVACY,
                    PROPERTY_PERMISSIONS_HUB_ENABLED, true.toString(), false)
            appOpsManager.clearHistory()
            appOpsManager.resetHistoryParameters()
        }
    }

    @After
    fun tearDownTest() {
        runWithShellPermissionIdentity {
            appOpsManager.clearHistory()
            appOpsManager.resetHistoryParameters()
            DeviceConfig.setProperty(DeviceConfig.NAMESPACE_PRIVACY,
                    PROPERTY_PERMISSIONS_HUB_ENABLED, wasPermissionsHubEnabled.toString(), false)
        }
    }

    @Test
    fun testGetHistoricalPackageOpsForegroundAccessInMemoryBucket() {
        testGetHistoricalPackageOpsForegroundAtDepth(0)
    }

    @Test
    fun testGetHistoricalPackageOpsForegroundAccessFirstOnDiskBucket() {
        testGetHistoricalPackageOpsForegroundAtDepth(1)
    }

    @Test
    fun testHistoricalAggregationOneLevelsDeep() {
        testHistoricalAggregationSomeLevelsDeep(0)
    }

    @Test
    fun testHistoricalAggregationTwoLevelsDeep() {
        testHistoricalAggregationSomeLevelsDeep(1)
    }

    @Test
    fun testRebootHistory() {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_PASSIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        // Add the data to the history
        val chunk = createDataChunk()
        val chunkCount = (INTERVAL_COMPRESSION_MULTIPLIER * 2) + 3
        for (i in 0 until chunkCount) {
            addHistoricalOps(chunk)
        }

        // Validate the data for the first interval
        val firstIntervalBeginMillis = computeIntervalBeginRawMillis(0)
        val firstIntervalEndMillis = computeIntervalBeginRawMillis(1)
        var firstOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertHasCounts(firstOps!!, 197)

        // Validate the data for the second interval
        val secondIntervalBeginMillis = computeIntervalBeginRawMillis(1)
        val secondIntervalEndMillis = computeIntervalBeginRawMillis(2)
        var secondOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)
        assertHasCounts(secondOps!!, 33)

        // Validate the data for all intervals
        val everythingIntervalBeginMillis = Instant.EPOCH.toEpochMilli()
        val everythingIntervalEndMillis = Long.MAX_VALUE
        var allOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                everythingIntervalBeginMillis, everythingIntervalEndMillis)
        assertHasCounts(allOps!!, 230)

        // Now reboot the history
        runWithShellPermissionIdentity {
            appOpsManager.rebootHistory(firstIntervalEndMillis)
        }

        // Validate the data for the first interval
        firstOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertHasCounts(firstOps!!, 0)

        // Validate the data for the second interval
        secondOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)
        assertHasCounts(secondOps!!, 230)

        // Validate the data for all intervals
        allOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                everythingIntervalBeginMillis, everythingIntervalEndMillis)
        assertHasCounts(allOps!!, 230)

        // Write some more ops to the first interval
        for (i in 0 until chunkCount) {
            addHistoricalOps(chunk)
        }

        // Validate the data for the first interval
        firstOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertHasCounts(firstOps!!, 197)

        // Validate the data for the second interval
        secondOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)
        assertHasCounts(secondOps!!, 263)

        // Validate the data for all intervals
        allOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                everythingIntervalBeginMillis, everythingIntervalEndMillis)
        assertHasCounts(allOps!!, 460)
    }

    @Test
    fun testHistoricalAggregationOverflow() {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_PASSIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        // Add the data to the history
        val chunk = createDataChunk()
        val chunkCount = (INTERVAL_COMPRESSION_MULTIPLIER * 2) + 3
        for (i in 0 until chunkCount) {
            addHistoricalOps(chunk)
        }

        // Validate the data for the first interval
        val firstIntervalBeginMillis = computeIntervalBeginRawMillis(0)
        val firstIntervalEndMillis = computeIntervalBeginRawMillis(1)
        val firstOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertHasCounts(firstOps!!, 197)

        // Validate the data for the second interval
        val secondIntervalBeginMillis = computeIntervalBeginRawMillis(1)
        val secondIntervalEndMillis = computeIntervalBeginRawMillis(2)
        val secondOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)
        assertHasCounts(secondOps!!, 33)

        // Validate the data for both intervals
        val thirdIntervalBeginMillis = firstIntervalEndMillis - SNAPSHOT_INTERVAL_MILLIS
        val thirdIntervalEndMillis = secondIntervalBeginMillis + SNAPSHOT_INTERVAL_MILLIS
        val thirdOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                thirdIntervalBeginMillis, thirdIntervalEndMillis)
        assertHasCounts(thirdOps!!, 33)
    }

    @Test
    fun testHistoryTimeTravel() {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_PASSIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        // Fill the first two intervals with data
        val chunk = createDataChunk()
        val chunkCount = computeSlotCount(2) * SNAPSHOT_INTERVAL_MILLIS / chunk.endTimeMillis
        for (i in 0 until chunkCount) {
            addHistoricalOps(chunk)
        }

        // Move history in past with the first interval duration
        val firstIntervalDurationMillis = computeIntervalDurationMillis(0)
        runWithShellPermissionIdentity {
            appOpsManager.offsetHistory(firstIntervalDurationMillis)
        }

        // Validate the data for the first interval
        val firstIntervalBeginMillis = computeIntervalBeginRawMillis(0)
        val firstIntervalEndMillis = firstIntervalBeginMillis + firstIntervalDurationMillis
        val firstOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertThat(firstOps).isNotNull()
        assertThat(firstOps!!.uidCount).isEqualTo(0)

        // Validate the data for the second interval
        val secondIntervalBeginMillis = computeIntervalBeginRawMillis(1)
        val secondIntervalDurationMillis = computeIntervalDurationMillis(1)
        val secondIntervalEndMillis = secondIntervalBeginMillis + secondIntervalDurationMillis
        val secondOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)
        val secondChunkCount = ((computeSlotCount(2) - computeSlotCount(1))
            .times(SNAPSHOT_INTERVAL_MILLIS) / chunk.endTimeMillis)
        assertHasCounts(secondOps!!, 10 * secondChunkCount)

        // Validate the data for the third interval
        val thirdIntervalBeginMillis = computeIntervalBeginRawMillis(2)
        val thirdIntervalDurationMillis = computeIntervalDurationMillis(2)
        val thirdIntervalEndMillis = thirdIntervalBeginMillis + thirdIntervalDurationMillis
        val thirdOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                thirdIntervalBeginMillis, thirdIntervalEndMillis)
        val thirdChunkCount = secondChunkCount / INTERVAL_COMPRESSION_MULTIPLIER
        assertHasCounts(thirdOps!!, 10 * thirdChunkCount)

        // Move history in future with the first interval duration
        runWithShellPermissionIdentity {
            appOpsManager.offsetHistory(-(2.5f * firstIntervalDurationMillis).toLong())
        }

        // Validate the data for the first interval
        val fourthOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                firstIntervalBeginMillis, firstIntervalEndMillis)
        assertHasCounts(fourthOps!!, 194)

        // Validate the data for the second interval
        val fifthOps = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                secondIntervalBeginMillis, secondIntervalEndMillis)

        assertThat(fifthOps).isNotNull()
        assertHasCounts(fifthOps!!, 1703)
    }

    @Test
    fun testGetHistoricalAggregationOverAttributions() {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_ACTIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        setUidMode(OPSTR_REQUEST_DELETE_PACKAGES, uid, AppOpsManager.MODE_ALLOWED)

        UidStateForceActivity.waitForResumed()

        appOpsManager.noteOp(OPSTR_REQUEST_DELETE_PACKAGES, uid, packageName, "firstAttribution",
                null)
        appOpsManager.noteOp(OPSTR_REQUEST_DELETE_PACKAGES, uid, packageName, "secondAttribution",
                null)
        var memOps: AppOpsManager.HistoricalOps? = null
        eventually(SNAPSHOT_INTERVAL_MILLIS / 2) {
            memOps = getHistoricalOps(appOpsManager, uid = uid)!!

            assertThat(memOps!!.getUidOpsAt(0).getPackageOpsAt(0)
                    .getOp(OPSTR_REQUEST_DELETE_PACKAGES)!!.getForegroundAccessCount(OP_FLAGS_ALL))
                    .isEqualTo(2)
            assertThat(memOps!!.getUidOpsAt(0).getPackageOpsAt(0)
                    .getAttributedOps("firstAttribution")!!.getOp(OPSTR_REQUEST_DELETE_PACKAGES)!!
                    .getForegroundAccessCount(OP_FLAGS_ALL)).isEqualTo(1)
            assertThat(memOps!!.getUidOpsAt(0).getPackageOpsAt(0)
                    .getAttributedOps("secondAttribution")!!.getOp(OPSTR_REQUEST_DELETE_PACKAGES)!!
                    .getForegroundAccessCount(OP_FLAGS_ALL)).isEqualTo(1)
        }

        // Wait until data is on disk and verify no entry got lost
        Thread.sleep(SNAPSHOT_INTERVAL_MILLIS)

        val diskOps = getHistoricalOps(appOpsManager, uid = uid)!!
        assertThat(diskOps.getUidOpsAt(0)).isEqualTo(memOps?.getUidOpsAt(0))
    }

    private fun testHistoricalAggregationSomeLevelsDeep(depth: Int) {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_PASSIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        // Add the data to the history
        val chunk = createDataChunk()
        val chunkCount = (computeSlotCount(depth + 1)
            .times(SNAPSHOT_INTERVAL_MILLIS) / chunk.endTimeMillis)
        for (i in 0 until chunkCount) {
            addHistoricalOps(chunk)
        }

        // Validate the data for the full interval
        val intervalBeginMillis = computeIntervalBeginRawMillis(depth)
        val intervalEndMillis = computeIntervalBeginRawMillis(depth + 1)
        val ops = getHistoricalOpsFromDiskRaw(uid, packageName, null /*opNames*/,
                intervalBeginMillis, intervalEndMillis)
        val expectedOpCount = ((computeSlotCount(depth + 1) - computeSlotCount(depth))
            .times(SNAPSHOT_INTERVAL_MILLIS) / chunk.endTimeMillis) * 10
        assertHasCounts(ops!!, expectedOpCount)
    }

    private fun testGetHistoricalPackageOpsForegroundAtDepth(depth: Int) {
        // Configure historical registry behavior.
        setHistoryParameters(
                AppOpsManager.HISTORICAL_MODE_ENABLED_ACTIVE,
                SNAPSHOT_INTERVAL_MILLIS,
                INTERVAL_COMPRESSION_MULTIPLIER)

        setUidMode(AppOpsManager.OPSTR_START_FOREGROUND, uid,
                AppOpsManager.MODE_ALLOWED)
        setUidMode(AppOpsManager.OPSTR_START_FOREGROUND, 2000,
                AppOpsManager.MODE_ALLOWED)

        UidStateForceActivity.waitForResumed()

        try {
            val noteCount = 5

            var beginTimeMillis = 0L
            var endTimeMillis = 0L

            // Note ops such that we have data at all levels
            for (d in depth downTo 0) {
                for (i in 0 until noteCount) {
                    appOpsManager.noteOp(AppOpsManager.OPSTR_START_FOREGROUND, uid, packageName)
                }

                if (d > 0) {
                    val previousIntervalDuration = computeIntervalDurationMillis(d - 2)
                    val currentIntervalDuration = computeIntervalDurationMillis(d - 1)

                    endTimeMillis -= previousIntervalDuration
                    beginTimeMillis -= currentIntervalDuration

                    val sleepDurationMillis = currentIntervalDuration / 2
                    SystemClock.sleep(sleepDurationMillis)
                }
            }

            val nowMillis = System.currentTimeMillis()
            if (depth > 0) {
                beginTimeMillis += nowMillis
                endTimeMillis += nowMillis
            } else {
                beginTimeMillis = nowMillis - SNAPSHOT_INTERVAL_MILLIS
                endTimeMillis = Long.MAX_VALUE
            }

            // Get all ops for the package
            val allOps = getHistoricalOps(appOpsManager, uid, packageName,
                    null, beginTimeMillis, endTimeMillis)

            assertThat(allOps).isNotNull()
            assertThat(allOps!!.uidCount).isEqualTo(1)
            assertThat(allOps.beginTimeMillis).isEqualTo(beginTimeMillis)
            assertThat(allOps.endTimeMillis).isGreaterThan(beginTimeMillis)

            val uidOps = allOps.getUidOpsAt(0)
            assertThat(uidOps).isNotNull()
            assertThat(uidOps.uid).isEqualTo(Process.myUid())
            assertThat(uidOps.packageCount).isEqualTo(1)

            val packageOps = uidOps.getPackageOpsAt(0)
            assertThat(packageOps).isNotNull()
            assertThat(packageOps.packageName).isEqualTo(packageName)
            assertThat(packageOps.opCount).isEqualTo(1)

            val op = packageOps.getOpAt(0)
            assertThat(op).isNotNull()
            assertThat(op.opName).isEqualTo(AppOpsManager.OPSTR_START_FOREGROUND)

            assertThat(op.getForegroundAccessCount(AppOpsManager.OP_FLAGS_ALL))
                    .isEqualTo(noteCount)
            assertThat(op.getBackgroundAccessCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_PERSISTENT)).isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_TOP)).isEqualTo(noteCount)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE_LOCATION))
                    .isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE))
                    .isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_FOREGROUND)).isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_BACKGROUND)).isEqualTo(0)
            assertThat(getAccessCount(op, AppOpsManager.UID_STATE_CACHED)).isEqualTo(0)

            assertThat(op.getForegroundAccessDuration(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(0)
            assertThat(op.getBackgroundAccessDuration(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(0)
            assertThat(op.getAccessDuration(AppOpsManager.UID_STATE_TOP,
                    AppOpsManager.UID_STATE_BACKGROUND, AppOpsManager.OP_FLAGS_ALL))
                    .isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_PERSISTENT)).isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_TOP)).isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE_LOCATION))
                    .isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE))
                    .isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_FOREGROUND)).isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_BACKGROUND)).isEqualTo(0)
            assertThat(getAccessDuration(op, AppOpsManager.UID_STATE_CACHED)).isEqualTo(0)

            assertThat(op.getForegroundRejectCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(0)
            assertThat(op.getBackgroundRejectCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(0)
            assertThat(op.getRejectCount(AppOpsManager.UID_STATE_TOP,
                    AppOpsManager.UID_STATE_BACKGROUND, AppOpsManager.OP_FLAGS_ALL))
                    .isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_PERSISTENT)).isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_TOP)).isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE_LOCATION))
                    .isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_FOREGROUND_SERVICE))
                    .isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_FOREGROUND)).isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_BACKGROUND)).isEqualTo(0)
            assertThat(getRejectCount(op, AppOpsManager.UID_STATE_CACHED)).isEqualTo(0)
        } finally {
            setUidMode(AppOpsManager.OPSTR_START_FOREGROUND, uid, AppOpsManager.MODE_FOREGROUND)
            setUidMode(AppOpsManager.OPSTR_START_FOREGROUND, 2000, AppOpsManager.MODE_FOREGROUND)
        }
    }

    private fun createDataChunk(): HistoricalOps {
        val chunk = HistoricalOps(SNAPSHOT_INTERVAL_MILLIS / 4,
                SNAPSHOT_INTERVAL_MILLIS / 2)
        chunk.increaseAccessCount(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_TOP, AppOpsManager.OP_FLAG_SELF, 10)
        chunk.increaseAccessCount(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_BACKGROUND, AppOpsManager.OP_FLAG_SELF, 10)
        chunk.increaseRejectCount(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_TOP, AppOpsManager.OP_FLAG_SELF, 10)
        chunk.increaseRejectCount(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_BACKGROUND, AppOpsManager.OP_FLAG_SELF, 10)
        chunk.increaseAccessDuration(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_TOP, AppOpsManager.OP_FLAG_SELF, 10)
        chunk.increaseAccessDuration(AppOpsManager.OP_START_FOREGROUND, uid, packageName, null,
                AppOpsManager.UID_STATE_BACKGROUND, AppOpsManager.OP_FLAG_SELF, 10)
        return chunk
    }

    private fun setHistoryParameters(
        mode: Int,
        baseSnapshotInterval: Long,
        compressionStep: Int
    ) {
        runWithShellPermissionIdentity {
            appOpsManager.setHistoryParameters(mode, baseSnapshotInterval, compressionStep)
        }
    }

    private fun setUidMode(appOp: String, uid: Int, mode: Int) {
        runWithShellPermissionIdentity {
            appOpsManager.setUidMode(appOp, uid, mode)
        }
    }

    private fun addHistoricalOps(ops: AppOpsManager.HistoricalOps) {
        runWithShellPermissionIdentity {
            appOpsManager.addHistoricalOps(ops)
        }
    }

    private fun getHistoricalOps(
        appOpsManager: AppOpsManager,
        uid: Int = Process.INVALID_UID,
        packageName: String? = null,
        opNames: List<String>? = null,
        beginTimeMillis: Long = 0,
        endTimeMillis: Long = Long.MAX_VALUE
    ): HistoricalOps? {
        uiAutomation.adoptShellPermissionIdentity()
        val array = arrayOfNulls<HistoricalOps>(1)
        val lock = ReentrantLock()
        val condition = lock.newCondition()
        try {
            lock.lock()
            val request = AppOpsManager.HistoricalOpsRequest.Builder(
                    beginTimeMillis, endTimeMillis)
                    .setUid(uid)
                    .setPackageName(packageName)
                    .setOpNames(opNames?.toList())
                    .build()
            appOpsManager.getHistoricalOps(request, context.mainExecutor, Consumer { ops ->
                array[0] = ops
                try {
                    lock.lock()
                    condition.signalAll()
                } finally {
                    lock.unlock()
                }
            })
            condition.await(5, TimeUnit.SECONDS)
            return array[0]
        } finally {
            lock.unlock()
            uiAutomation.dropShellPermissionIdentity()
        }
    }

    private fun assertHasCounts(ops: HistoricalOps, count: Long) {
        assertThat(ops).isNotNull()

        if (count <= 0) {
            assertThat(ops.uidCount).isEqualTo(0)
            return
        }

        assertThat(ops.uidCount).isEqualTo(1)

        val uidOps = ops.getUidOpsAt(0)
        assertThat(uidOps).isNotNull()

        val packageOps = uidOps.getPackageOpsAt(0)
        assertThat(packageOps).isNotNull()

        val op = packageOps.getOpAt(0)
        assertThat(op).isNotNull()

        assertThat(op.getForegroundAccessCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
        assertThat(op.getBackgroundAccessCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
        assertThat(op.getForegroundRejectCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
        assertThat(op.getBackgroundRejectCount(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
        assertThat(op.getForegroundAccessDuration(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
        assertThat(op.getBackgroundAccessDuration(AppOpsManager.OP_FLAGS_ALL)).isEqualTo(count)
    }

    private fun getAccessCount(op: HistoricalOp, uidState: Int): Long {
        return op.getAccessCount(uidState, uidState, AppOpsManager.OP_FLAGS_ALL)
    }

    private fun getRejectCount(op: HistoricalOp, uidState: Int): Long {
        return op.getRejectCount(uidState, uidState, AppOpsManager.OP_FLAGS_ALL)
    }

    private fun getAccessDuration(op: HistoricalOp, uidState: Int): Long {
        return op.getAccessDuration(uidState, uidState, AppOpsManager.OP_FLAGS_ALL)
    }

    private fun getHistoricalOpsFromDiskRaw(
        uid: Int,
        packageName: String,
        opNames: List<String>?,
        beginTimeMillis: Long,
        endTimeMillis: Long
    ): HistoricalOps? {
        uiAutomation.adoptShellPermissionIdentity()
        val array = arrayOfNulls<HistoricalOps>(1)
        val lock = ReentrantLock()
        val condition = lock.newCondition()
        try {
            lock.lock()
            val request = AppOpsManager.HistoricalOpsRequest.Builder(
                    beginTimeMillis, endTimeMillis)
                    .setUid(uid)
                    .setPackageName(packageName)
                    .setOpNames(opNames?.toList())
                    .build()
            appOpsManager.getHistoricalOpsFromDiskRaw(request, context.mainExecutor,
                    Consumer { ops ->
                        array[0] = ops
                        try {
                            lock.lock()
                            condition.signalAll()
                        } finally {
                            lock.unlock()
                        }
                    })
            condition.await(5, TimeUnit.SECONDS)
            return array[0]
        } finally {
            lock.unlock()
            uiAutomation.dropShellPermissionIdentity()
        }
    }

    companion object {
        const val INTERVAL_COMPRESSION_MULTIPLIER = 10
        const val SNAPSHOT_INTERVAL_MILLIS = 1000L

        val instrumentation get() = InstrumentationRegistry.getInstrumentation()
        val context get() = instrumentation.context
        val uiAutomation get() = instrumentation.uiAutomation

        private fun computeIntervalDurationMillis(depth: Int): Long {
            return Math.pow(INTERVAL_COMPRESSION_MULTIPLIER.toDouble(),
                    (depth + 1).toDouble()).toLong() * SNAPSHOT_INTERVAL_MILLIS
        }

        private fun computeSlotCount(depth: Int): Int {
            var count = 0
            for (i in 1..depth) {
                count += Math.pow(INTERVAL_COMPRESSION_MULTIPLIER.toDouble(), i.toDouble()).toInt()
            }
            return count
        }

        private fun computeIntervalBeginRawMillis(depth: Int): Long {
            var beginTimeMillis: Long = 0
            for (i in 0 until depth + 1) {
                beginTimeMillis += Math.pow(INTERVAL_COMPRESSION_MULTIPLIER.toDouble(),
                        i.toDouble()).toLong()
            }
            return beginTimeMillis * SNAPSHOT_INTERVAL_MILLIS
        }
    }
}
