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

package com.android.car.storagemonitoring;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.car.storagemonitoring.IoStats;
import android.car.storagemonitoring.IoStatsEntry;
import android.car.storagemonitoring.LifetimeWriteInfo;
import android.car.storagemonitoring.UidIoRecord;
import android.car.storagemonitoring.WearEstimate;
import android.car.storagemonitoring.WearEstimateChange;
import android.hardware.health.V2_0.IHealth;
import android.hardware.health.V2_0.IHealth.getStorageInfoCallback;
import android.hardware.health.V2_0.Result;
import android.hardware.health.V2_0.StorageInfo;
import android.os.Parcel;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.JsonReader;
import android.util.JsonWriter;
import android.util.SparseArray;

import com.android.car.test.utils.TemporaryDirectory;
import com.android.car.test.utils.TemporaryFile;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.FileWriter;
import java.io.StringReader;
import java.io.StringWriter;
import java.nio.file.Files;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests the storage monitoring API in CarService.
 */
@RunWith(MockitoJUnitRunner.class)
@MediumTest
public class CarStorageMonitoringTest {
    static final String TAG = CarStorageMonitoringTest.class.getSimpleName();

    @Mock private IHealth mMockedHal;
    @Mock private HealthServiceWearInfoProvider.IHealthSupplier mHealthServiceSupplier;

    @Test
    public void testEMmcWearInformationProvider() throws Exception {
        try (TemporaryFile lifetimeFile = new TemporaryFile(TAG)) {
            try (TemporaryFile eolFile = new TemporaryFile(TAG)) {
                lifetimeFile.write("0x05 0x00");
                eolFile.write("01");

                EMmcWearInformationProvider wearInfoProvider = new EMmcWearInformationProvider(
                        lifetimeFile.getFile(), eolFile.getFile());

                WearInformation wearInformation = wearInfoProvider.load();

                assertThat(wearInformation).isNotNull();
                assertThat(wearInformation.lifetimeEstimateA).isEqualTo(40);
                assertThat(wearInformation.lifetimeEstimateB)
                    .isEqualTo(WearInformation.UNKNOWN_LIFETIME_ESTIMATE);
                assertThat(wearInformation.preEolInfo)
                    .isEqualTo(WearInformation.PRE_EOL_INFO_NORMAL);
            }
        }
    }

    @Test
    public void testUfsWearInformationProvider() throws Exception {
        try (TemporaryFile lifetimeFile = new TemporaryFile(TAG)) {
            lifetimeFile.write("ufs version: 1.0\n" +
                    "Health Descriptor[Byte offset 0x2]: bPreEOLInfo = 0x2\n" +
                    "Health Descriptor[Byte offset 0x1]: bDescriptionIDN = 0x1\n" +
                    "Health Descriptor[Byte offset 0x3]: bDeviceLifeTimeEstA = 0x0\n" +
                    "Health Descriptor[Byte offset 0x5]: VendorPropInfo = somedatahere\n" +
                    "Health Descriptor[Byte offset 0x4]: bDeviceLifeTimeEstB = 0xA\n");

            UfsWearInformationProvider wearInfoProvider = new UfsWearInformationProvider(
                lifetimeFile.getFile());

            WearInformation wearInformation = wearInfoProvider.load();

            assertThat(wearInformation).isNotNull();
            assertThat(wearInformation.lifetimeEstimateB).isEqualTo(90);
            assertThat(wearInformation.preEolInfo).isEqualTo(WearInformation.PRE_EOL_INFO_WARNING);
            assertThat(wearInformation.lifetimeEstimateA)
                .isEqualTo(WearInformation.UNKNOWN_LIFETIME_ESTIMATE);
        }
    }

    @Test
    public void testHealthServiceWearInformationProvider() throws Exception {
        StorageInfo storageInfo = new StorageInfo();
        storageInfo.eol = WearInformation.PRE_EOL_INFO_NORMAL;
        storageInfo.lifetimeA = 3;
        storageInfo.lifetimeB = WearInformation.UNKNOWN_LIFETIME_ESTIMATE;
        storageInfo.attr.isInternal = true;
        HealthServiceWearInfoProvider wearInfoProvider = new HealthServiceWearInfoProvider();
        wearInfoProvider.setHealthSupplier(mHealthServiceSupplier);

        doReturn(mMockedHal)
            .when(mHealthServiceSupplier).get(anyString());
        doAnswer((invocation) -> {
            ArrayList<StorageInfo> list = new ArrayList<StorageInfo>();
            list.add(storageInfo);
            ((IHealth.getStorageInfoCallback) invocation.getArguments()[0])
                .onValues(Result.SUCCESS, list);
            return null;
        }).when(mMockedHal).getStorageInfo(any(getStorageInfoCallback.class));
        WearInformation wearInformation = wearInfoProvider.load();

        assertThat(wearInformation).isNotNull();
        assertThat(wearInformation.lifetimeEstimateA).isEqualTo(storageInfo.lifetimeA);
        assertThat(wearInformation.lifetimeEstimateB).isEqualTo(storageInfo.lifetimeB);
        assertThat(wearInformation.preEolInfo).isEqualTo(storageInfo.eol);
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    // TODO: use EqualsTester to check equality with itself,
    // Remove @SuppressWarnings("TruthSelfEquals") at other places too
    public void testWearEstimateEquality() {
        WearEstimate wearEstimate1 = new WearEstimate(10, 20);
        WearEstimate wearEstimate2 = new WearEstimate(10, 20);
        WearEstimate wearEstimate3 = new WearEstimate(20, 30);
        assertThat(wearEstimate1).isEqualTo(wearEstimate1);
        assertThat(wearEstimate2).isEqualTo(wearEstimate1);
        assertThat(wearEstimate1).isNotSameAs(wearEstimate3);
    }

    @Test
    public void testWearEstimateParcel() throws Exception {
        WearEstimate originalWearEstimate = new WearEstimate(10, 20);
        Parcel p = Parcel.obtain();
        originalWearEstimate.writeToParcel(p, 0);
        p.setDataPosition(0);
        WearEstimate newWearEstimate = new WearEstimate(p);
        assertThat(newWearEstimate).isEqualTo(originalWearEstimate);
        p.recycle();
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testWearEstimateChangeEquality() {
        WearEstimateChange wearEstimateChange1 = new WearEstimateChange(
                new WearEstimate(10, 20),
                new WearEstimate(20, 30),
                5000L,
                Instant.now(),
                false);
        WearEstimateChange wearEstimateChange2 = new WearEstimateChange(
            new WearEstimate(10, 20),
            new WearEstimate(20, 30),
            5000L,
            wearEstimateChange1.dateAtChange,
            false);
        assertThat(wearEstimateChange1).isEqualTo(wearEstimateChange1);
        assertThat(wearEstimateChange2).isEqualTo(wearEstimateChange1);
        WearEstimateChange wearEstimateChange3 = new WearEstimateChange(
            new WearEstimate(10, 30),
            new WearEstimate(20, 30),
            3000L,
            Instant.now(),
            true);
        assertThat(wearEstimateChange1).isNotSameAs(wearEstimateChange3);
    }

    @Test
    public void testWearEstimateChangeParcel() throws Exception {
        WearEstimateChange originalWearEstimateChange = new WearEstimateChange(
                new WearEstimate(10, 0),
                new WearEstimate(20, 10),
                123456789L,
                Instant.now(),
                false);
        Parcel p = Parcel.obtain();
        originalWearEstimateChange.writeToParcel(p, 0);
        p.setDataPosition(0);
        WearEstimateChange newWearEstimateChange = new WearEstimateChange(p);
        assertThat(newWearEstimateChange).isEqualTo(originalWearEstimateChange);
        p.recycle();
    }

    @Test
    public void testWearEstimateJson() throws Exception {
        WearEstimate originalWearEstimate = new WearEstimate(20, 0);
        StringWriter stringWriter = new StringWriter(1024);
        JsonWriter jsonWriter = new JsonWriter(stringWriter);
        originalWearEstimate.writeToJson(jsonWriter);
        StringReader stringReader = new StringReader(stringWriter.toString());
        JsonReader jsonReader = new JsonReader(stringReader);
        WearEstimate newWearEstimate = new WearEstimate(jsonReader);
        assertThat(newWearEstimate).isEqualTo(originalWearEstimate);
    }

    @Test
    public void testWearEstimateRecordJson() throws Exception {
        try (TemporaryFile temporaryFile = new TemporaryFile(TAG)) {
            WearEstimateRecord originalWearEstimateRecord = new WearEstimateRecord(new WearEstimate(10, 20),
                new WearEstimate(10, 30), 5000, Instant.ofEpochMilli(1000));
            try (JsonWriter jsonWriter = new JsonWriter(new FileWriter(temporaryFile.getFile()))) {
                originalWearEstimateRecord.writeToJson(jsonWriter);
            }
            JSONObject jsonObject = new JSONObject(
                    new String(Files.readAllBytes(temporaryFile.getPath())));
            WearEstimateRecord newWearEstimateRecord = new WearEstimateRecord(jsonObject);
            assertThat(newWearEstimateRecord).isEqualTo(originalWearEstimateRecord);
        }
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testWearEstimateRecordEquality() throws Exception {
        WearEstimateRecord wearEstimateRecord1 = new WearEstimateRecord(WearEstimate.UNKNOWN_ESTIMATE,
                new WearEstimate(10, 20), 5000, Instant.ofEpochMilli(2000));
        WearEstimateRecord wearEstimateRecord2 = new WearEstimateRecord(WearEstimate.UNKNOWN_ESTIMATE,
            new WearEstimate(10, 20), 5000, Instant.ofEpochMilli(2000));
        WearEstimateRecord wearEstimateRecord3 = new WearEstimateRecord(WearEstimate.UNKNOWN_ESTIMATE,
            new WearEstimate(10, 40), 5000, Instant.ofEpochMilli(1000));

        assertThat(wearEstimateRecord1).isEqualTo(wearEstimateRecord1);
        assertThat(wearEstimateRecord2).isEqualTo(wearEstimateRecord1);
        assertThat(wearEstimateRecord1).isNotSameAs(wearEstimateRecord3);
    }

    @Test
    public void testWearHistoryJson() throws Exception {
        try (TemporaryFile temporaryFile = new TemporaryFile(TAG)) {
            WearEstimateRecord wearEstimateRecord1 = new WearEstimateRecord(
                WearEstimate.UNKNOWN_ESTIMATE,
                new WearEstimate(10, 20), 5000, Instant.ofEpochMilli(2000));
            WearEstimateRecord wearEstimateRecord2 = new WearEstimateRecord(
                wearEstimateRecord1.getOldWearEstimate(),
                new WearEstimate(10, 40), 9000, Instant.ofEpochMilli(16000));
            WearEstimateRecord wearEstimateRecord3 = new WearEstimateRecord(
                wearEstimateRecord2.getOldWearEstimate(),
                new WearEstimate(20, 40), 12000, Instant.ofEpochMilli(21000));
            WearHistory originalWearHistory = WearHistory.fromRecords(wearEstimateRecord1,
                wearEstimateRecord2, wearEstimateRecord3);
            try (JsonWriter jsonWriter = new JsonWriter(new FileWriter(temporaryFile.getFile()))) {
                originalWearHistory.writeToJson(jsonWriter);
            }
            JSONObject jsonObject = new JSONObject(
                new String(Files.readAllBytes(temporaryFile.getPath())));
            WearHistory newWearHistory = new WearHistory(jsonObject);
            assertThat(newWearHistory).isEqualTo(originalWearHistory);
        }
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testWearHistoryEquality() throws Exception {
        WearEstimateRecord wearEstimateRecord1 = new WearEstimateRecord(
            WearEstimate.UNKNOWN_ESTIMATE,
            new WearEstimate(10, 20), 5000, Instant.ofEpochMilli(2000));
        WearEstimateRecord wearEstimateRecord2 = new WearEstimateRecord(
            wearEstimateRecord1.getOldWearEstimate(),
            new WearEstimate(10, 40), 9000, Instant.ofEpochMilli(16000));
        WearEstimateRecord wearEstimateRecord3 = new WearEstimateRecord(
            wearEstimateRecord2.getOldWearEstimate(),
            new WearEstimate(20, 40), 12000, Instant.ofEpochMilli(21000));
        WearEstimateRecord wearEstimateRecord4 = new WearEstimateRecord(
            wearEstimateRecord3.getOldWearEstimate(),
            new WearEstimate(30, 50), 17000, Instant.ofEpochMilli(34000));
        WearEstimateRecord wearEstimateRecord5 = new WearEstimateRecord(
            wearEstimateRecord3.getOldWearEstimate(),
            new WearEstimate(20, 50), 15000, Instant.ofEpochMilli(34000));

        WearHistory wearHistory1 = WearHistory.fromRecords(wearEstimateRecord1,
            wearEstimateRecord2, wearEstimateRecord3, wearEstimateRecord4);
        WearHistory wearHistory2 = WearHistory.fromRecords(wearEstimateRecord4,
            wearEstimateRecord1, wearEstimateRecord2, wearEstimateRecord3);
        WearHistory wearHistory3 = WearHistory.fromRecords(wearEstimateRecord1,
            wearEstimateRecord2, wearEstimateRecord3, wearEstimateRecord5);

        assertThat(wearHistory1).isEqualTo(wearHistory1);
        assertThat(wearHistory2).isEqualTo(wearHistory1);
        assertThat(wearHistory1).isNotSameAs(wearHistory3);
    }

    @Test
    public void testWearHistoryToChanges() {
        WearEstimateRecord wearEstimateRecord1 = new WearEstimateRecord(
            WearEstimate.UNKNOWN_ESTIMATE,
            new WearEstimate(10, 20), 3600000, Instant.ofEpochMilli(2000));
        WearEstimateRecord wearEstimateRecord2 = new WearEstimateRecord(
            wearEstimateRecord1.getOldWearEstimate(),
            new WearEstimate(10, 40), 172000000, Instant.ofEpochMilli(16000));
        WearEstimateRecord wearEstimateRecord3 = new WearEstimateRecord(
            wearEstimateRecord2.getOldWearEstimate(),
            new WearEstimate(20, 40), 172000001, Instant.ofEpochMilli(21000));

        WearHistory wearHistory = WearHistory.fromRecords(wearEstimateRecord1,
            wearEstimateRecord2, wearEstimateRecord3);

        List<WearEstimateChange> wearEstimateChanges = wearHistory.toWearEstimateChanges(1);

        assertThat(wearEstimateChanges.size()).isEqualTo(3);
        WearEstimateChange unknownToOne = wearEstimateChanges.get(0);
        WearEstimateChange oneToTwo = wearEstimateChanges.get(1);
        WearEstimateChange twoToThree = wearEstimateChanges.get(2);

        assertThat(wearEstimateRecord1.getOldWearEstimate()).isEqualTo(unknownToOne.oldEstimate);
        assertThat(wearEstimateRecord1.getNewWearEstimate()).isEqualTo(unknownToOne.newEstimate);
        assertThat(wearEstimateRecord1.getTotalCarServiceUptime())
            .isEqualTo(unknownToOne.uptimeAtChange);
        assertThat(wearEstimateRecord1.getUnixTimestamp()).isEqualTo(unknownToOne.dateAtChange);
        assertThat(unknownToOne.isAcceptableDegradation).isTrue();

        assertThat(wearEstimateRecord2.getOldWearEstimate()).isEqualTo(oneToTwo.oldEstimate);
        assertThat(wearEstimateRecord2.getNewWearEstimate()).isEqualTo(oneToTwo.newEstimate);
        assertThat(wearEstimateRecord2.getTotalCarServiceUptime())
            .isEqualTo(oneToTwo.uptimeAtChange);
        assertThat(wearEstimateRecord2.getUnixTimestamp()).isEqualTo(oneToTwo.dateAtChange);
        assertThat(oneToTwo.isAcceptableDegradation).isTrue();

        assertThat(wearEstimateRecord3.getOldWearEstimate()).isEqualTo(twoToThree.oldEstimate);
        assertThat(wearEstimateRecord3.getNewWearEstimate()).isEqualTo(twoToThree.newEstimate);
        assertThat(wearEstimateRecord3.getTotalCarServiceUptime())
            .isEqualTo(twoToThree.uptimeAtChange);
        assertThat(wearEstimateRecord3.getUnixTimestamp()).isEqualTo(twoToThree.dateAtChange);
        assertThat(twoToThree.isAcceptableDegradation).isFalse();
    }

    @Test
    public void testUidIoStatEntry() throws Exception {
        try (TemporaryFile statsFile = new TemporaryFile(TAG)) {
            statsFile.write("0 256797495 181736102 362132480 947167232 0 0 0 0 250 0\n"
                + "1006 489007 196802 0 20480 51474 2048 1024 2048 1 1\n");

            ProcfsUidIoStatsProvider statsProvider = new ProcfsUidIoStatsProvider(
                    statsFile.getPath());

            SparseArray<UidIoRecord> entries = statsProvider.load();

            assertThat(entries).isNotNull();
            assertThat(entries.size()).isEqualTo(2);

            IoStatsEntry entry = new IoStatsEntry(entries.get(0), 1234);
            assertThat(entry).isNotNull();
            assertThat(entry.uid).isEqualTo(0);
            assertThat(entry.runtimeMillis).isEqualTo(1234);
            assertThat(entry.foreground.bytesRead).isEqualTo(256797495);
            assertThat(entry.foreground.bytesWritten).isEqualTo(181736102);
            assertThat(entry.foreground.bytesReadFromStorage).isEqualTo(362132480);
            assertThat(entry.foreground.bytesWrittenToStorage).isEqualTo(947167232);
            assertThat(entry.foreground.fsyncCalls).isEqualTo(250);
            assertThat(entry.background.bytesRead).isEqualTo(0);
            assertThat(entry.background.bytesWritten).isEqualTo(0);
            assertThat(entry.background.bytesReadFromStorage).isEqualTo(0);
            assertThat(entry.background.bytesWrittenToStorage).isEqualTo(0);
            assertThat(entry.background.fsyncCalls).isEqualTo(0);

            entry = new IoStatsEntry(entries.get(1006), 4321);
            assertThat(entry).isNotNull();
            assertThat(entry.uid).isEqualTo(1006);
            assertThat(entry.runtimeMillis).isEqualTo(4321);
            assertThat(entry.foreground.bytesRead).isEqualTo(489007);
            assertThat(entry.foreground.bytesWritten).isEqualTo(196802);
            assertThat(entry.foreground.bytesReadFromStorage).isEqualTo(0);
            assertThat(entry.foreground.bytesWrittenToStorage).isEqualTo(20480);
            assertThat(entry.foreground.fsyncCalls).isEqualTo(1);
            assertThat(entry.background.bytesRead).isEqualTo(51474);
            assertThat(entry.background.bytesWritten).isEqualTo(2048);
            assertThat(entry.background.bytesReadFromStorage).isEqualTo(1024);
            assertThat(entry.background.bytesWrittenToStorage).isEqualTo(2048);
            assertThat(entry.background.fsyncCalls).isEqualTo(1);
        }
    }

    @Test
    public void testUidIoStatEntryMissingFields() throws Exception {
        try (TemporaryFile statsFile = new TemporaryFile(TAG)) {
            statsFile.write("0 256797495 181736102 362132480 947167232 0 0 0 0 250 0\n"
                + "1 2 3 4 5 6 7 8 9\n");

            ProcfsUidIoStatsProvider statsProvider = new ProcfsUidIoStatsProvider(
                statsFile.getPath());

            SparseArray<UidIoRecord> entries = statsProvider.load();

            assertThat(entries).isNull();
        }
    }

    @Test
    public void testUidIoStatEntryNonNumericFields() throws Exception {
        try (TemporaryFile statsFile = new TemporaryFile(TAG)) {
            statsFile.write("0 256797495 181736102 362132480 947167232 0 0 0 0 250 0\n"
                + "notanumber 489007 196802 0 20480 51474 2048 1024 2048 1 1\n");

            ProcfsUidIoStatsProvider statsProvider = new ProcfsUidIoStatsProvider(
                statsFile.getPath());

            SparseArray<UidIoRecord> entries = statsProvider.load();

            assertThat(entries).isNull();
        }
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testUidIoStatEntryEquality() throws Exception {
        IoStatsEntry statEntry1 = new IoStatsEntry(10, 1234,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));
        IoStatsEntry statEntry2 = new IoStatsEntry(10, 1234,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));
        IoStatsEntry statEntry3 = new IoStatsEntry(30, 4567,
            new IoStatsEntry.Metrics(1, 20, 30, 42, 50),
            new IoStatsEntry.Metrics(10, 200, 300, 420, 500));
        IoStatsEntry statEntry4 = new IoStatsEntry(11, 6541,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));
        IoStatsEntry statEntry5 = new IoStatsEntry(10, 1234,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 0),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));

        assertThat(statEntry1).isEqualTo(statEntry1);
        assertThat(statEntry2).isEqualTo(statEntry1);
        assertThat(statEntry1).isNotSameAs(statEntry3);
        assertThat(statEntry1).isNotSameAs(statEntry4);
        assertThat(statEntry1).isNotSameAs(statEntry5);
    }

    @Test
    public void testUidIoStatEntryParcel() throws Exception {
        IoStatsEntry statEntry = new IoStatsEntry(10, 5000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));
        Parcel p = Parcel.obtain();
        statEntry.writeToParcel(p, 0);
        p.setDataPosition(0);
        IoStatsEntry other = new IoStatsEntry(p);
        assertThat(statEntry).isEqualTo(other);
    }

    @Test
    public void testUidIoStatEntryJson() throws Exception {
        try (TemporaryFile temporaryFile = new TemporaryFile(TAG)) {
            IoStatsEntry statEntry = new IoStatsEntry(10, 1200,
                new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
                new IoStatsEntry.Metrics(100, 200, 300, 400, 500));
            try (JsonWriter jsonWriter = new JsonWriter(new FileWriter(temporaryFile.getFile()))) {
                statEntry.writeToJson(jsonWriter);
            }
            JSONObject jsonObject = new JSONObject(
                new String(Files.readAllBytes(temporaryFile.getPath())));
            IoStatsEntry other = new IoStatsEntry(jsonObject);
            assertThat(other).isEqualTo(statEntry);
        }
    }


    @Test
    public void testUidIoStatEntryDelta() throws Exception {
        IoStatsEntry statEntry1 = new IoStatsEntry(10, 1000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(60, 70, 80, 90, 100));

        IoStatsEntry statEntry2 = new IoStatsEntry(10,2000,
            new IoStatsEntry.Metrics(110, 120, 130, 140, 150),
            new IoStatsEntry.Metrics(260, 370, 480, 500, 110));

        IoStatsEntry statEntry3 = new IoStatsEntry(30, 3000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(100, 200, 300, 400, 500));


        IoStatsEntry delta21 = statEntry2.delta(statEntry1);
        assertThat(delta21).isNotNull();
        assertThat(delta21.uid).isEqualTo(statEntry1.uid);

        assertThat(delta21.runtimeMillis).isEqualTo(1000);
        assertThat(delta21.foreground.bytesRead).isEqualTo(100);
        assertThat(delta21.foreground.bytesWritten).isEqualTo(100);
        assertThat(delta21.foreground.bytesReadFromStorage).isEqualTo(100);
        assertThat(delta21.foreground.bytesWrittenToStorage).isEqualTo(100);
        assertThat(delta21.foreground.fsyncCalls).isEqualTo(100);

        assertThat(delta21.background.bytesRead).isEqualTo(200);
        assertThat(delta21.background.bytesWritten).isEqualTo(300);
        assertThat(delta21.background.bytesReadFromStorage).isEqualTo(400);
        assertThat(delta21.background.bytesWrittenToStorage).isEqualTo(410);
        assertThat(delta21.background.fsyncCalls).isEqualTo(10);

        try {
            IoStatsEntry delta31 = statEntry3.delta(statEntry1);
            fail("delta only allowed on stats for matching user ID");
        } catch (IllegalArgumentException e) {
            // test passed
        }
    }

    @Test
    public void testUidIoStatsRecordDelta() throws Exception {
        IoStatsEntry statEntry = new IoStatsEntry(10, 1000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(60, 70, 80, 90, 100));

        UidIoRecord statRecord = new UidIoRecord(10,
            20, 20, 30, 50, 70,
            80, 70, 80, 100, 110);

        UidIoRecord delta = statRecord.delta(statEntry);

        assertThat(delta).isNotNull();
        assertThat(delta.uid).isEqualTo(statRecord.uid);

        assertThat(delta.foreground_rchar).isEqualTo(10);
        assertThat(delta.foreground_wchar).isEqualTo(0);
        assertThat(delta.foreground_read_bytes).isEqualTo(0);
        assertThat(delta.foreground_write_bytes).isEqualTo(10);
        assertThat(delta.foreground_fsync).isEqualTo(20);

        assertThat(delta.background_rchar).isEqualTo(20);
        assertThat(delta.background_wchar).isEqualTo(0);
        assertThat(delta.background_read_bytes).isEqualTo(0);
        assertThat(delta.background_write_bytes).isEqualTo(10);
        assertThat(delta.background_fsync).isEqualTo(10);

        statRecord = new UidIoRecord(30,
            20, 20, 30, 50, 70,
            80, 70, 80, 100, 110);

        try {
            statRecord.delta(statEntry);
            fail("delta only allowed on records for matching user ID");
        } catch (IllegalArgumentException e) {
            // test passed
        }
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testUidIoStatsDelta() throws Exception {
        IoStatsEntry entry10 = new IoStatsEntry(10, 1000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(60, 70, 80, 90, 100));

        IoStatsEntry entry20 = new IoStatsEntry(20, 2000,
            new IoStatsEntry.Metrics(200, 60, 100, 30, 40),
            new IoStatsEntry.Metrics(20, 10, 20, 0, 0));

        IoStatsEntry entry30 = new IoStatsEntry(30, 2000,
            new IoStatsEntry.Metrics(50, 100, 100, 30, 40),
            new IoStatsEntry.Metrics(30, 0, 0, 0, 0));

        ArrayList<IoStatsEntry> statsEntries1 = new ArrayList<IoStatsEntry>() {{
            add(entry10);
            add(entry20);
        }};

        ArrayList<IoStatsEntry> statsEntries2 = new ArrayList<IoStatsEntry>() {{
            add(entry20);
            add(entry30);
        }};

        IoStats delta1 = new IoStats(statsEntries1, 5000);
        IoStats delta2 = new IoStats(statsEntries1, 5000);
        IoStats delta3 = new IoStats(statsEntries2, 3000);
        IoStats delta4 = new IoStats(statsEntries1, 5000);

        assertThat(delta1).isEqualTo(delta1);
        assertThat(delta2).isEqualTo(delta1);
        assertThat(delta1).isNotSameAs(delta3);
        assertThat(delta3).isNotSameAs(delta4);
    }

    @Test
    public void testUidIoStatsTotals() throws Exception {
        IoStatsEntry entry10 = new IoStatsEntry(10, 1000,
            new IoStatsEntry.Metrics(20, 0, 10, 0, 0),
            new IoStatsEntry.Metrics(10, 50, 0, 20, 2));

        IoStatsEntry entry20 = new IoStatsEntry(20, 1000,
            new IoStatsEntry.Metrics(100, 200, 50, 200, 1),
            new IoStatsEntry.Metrics(0, 30, 10, 0, 1));

        ArrayList<IoStatsEntry> statsEntries = new ArrayList<IoStatsEntry>() {{
            add(entry10);
            add(entry20);
        }};

        IoStats delta = new IoStats(statsEntries, 5000);

        IoStatsEntry.Metrics foregroundTotals = delta.getForegroundTotals();
        IoStatsEntry.Metrics backgroundTotals = delta.getBackgroundTotals();
        IoStatsEntry.Metrics overallTotals = delta.getTotals();

        assertThat(foregroundTotals.bytesRead).isEqualTo(120);
        assertThat(foregroundTotals.bytesWritten).isEqualTo(200);
        assertThat(foregroundTotals.bytesReadFromStorage).isEqualTo(60);
        assertThat(foregroundTotals.bytesWrittenToStorage).isEqualTo(200);
        assertThat(foregroundTotals.fsyncCalls).isEqualTo(1);


        assertThat(backgroundTotals.bytesRead).isEqualTo(10);
        assertThat(backgroundTotals.bytesWritten).isEqualTo(80);
        assertThat(backgroundTotals.bytesReadFromStorage).isEqualTo(10);
        assertThat(backgroundTotals.bytesWrittenToStorage).isEqualTo(20);
        assertThat(backgroundTotals.fsyncCalls).isEqualTo(3);

        assertThat(overallTotals.bytesRead).isEqualTo(130);
        assertThat(overallTotals.bytesWritten).isEqualTo(280);
        assertThat(overallTotals.bytesReadFromStorage).isEqualTo(70);
        assertThat(overallTotals.bytesWrittenToStorage).isEqualTo(220);
        assertThat(overallTotals.fsyncCalls).isEqualTo(4);
    }

    @Test
    public void testUidIoStatsDeltaParcel() throws Exception {
        IoStatsEntry entry10 = new IoStatsEntry(10, 1000,
            new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
            new IoStatsEntry.Metrics(60, 70, 80, 90, 100));

        IoStatsEntry entry20 = new IoStatsEntry(20, 2000,
            new IoStatsEntry.Metrics(200, 60, 100, 30, 40),
            new IoStatsEntry.Metrics(20, 10, 20, 0, 0));

        ArrayList<IoStatsEntry> statsEntries = new ArrayList<IoStatsEntry>() {{
            add(entry10);
            add(entry20);
        }};

        IoStats statsDelta = new IoStats(statsEntries, 5000);

        Parcel p = Parcel.obtain();
        statsDelta.writeToParcel(p, 0);
        p.setDataPosition(0);

        IoStats parceledStatsDelta = new IoStats(p);

        assertThat(parceledStatsDelta.getTimestamp()).isEqualTo(statsDelta.getTimestamp());

        assertEquals(2, parceledStatsDelta.getStats().stream()
                .filter(e -> e.equals(entry10) || e.equals(entry20))
                .count());
    }

    @Test
    public void testUidIoStatsDeltaJson() throws Exception {
        try (TemporaryFile temporaryFile = new TemporaryFile(TAG)) {
            IoStatsEntry entry10 = new IoStatsEntry(10, 1000,
                new IoStatsEntry.Metrics(10, 20, 30, 40, 50),
                new IoStatsEntry.Metrics(60, 70, 80, 90, 100));

            IoStatsEntry entry20 = new IoStatsEntry(20, 2000,
                new IoStatsEntry.Metrics(200, 60, 100, 30, 40),
                new IoStatsEntry.Metrics(20, 10, 20, 0, 0));

            ArrayList<IoStatsEntry> statsEntries = new ArrayList<IoStatsEntry>() {{
                add(entry10);
                add(entry20);
            }};

            IoStats statsDelta = new IoStats(statsEntries, 5000);
            try (JsonWriter jsonWriter = new JsonWriter(new FileWriter(temporaryFile.getFile()))) {
                statsDelta.writeToJson(jsonWriter);
            }
            JSONObject jsonObject = new JSONObject(
                new String(Files.readAllBytes(temporaryFile.getPath())));
            IoStats other = new IoStats(jsonObject);
            assertThat(other).isEqualTo(statsDelta);
        }
    }

    @Test
    public void testLifetimeWriteInfo() throws Exception {
        try (TemporaryDirectory temporaryDirectory = new TemporaryDirectory(TAG)) {
            try (TemporaryDirectory ext4 = temporaryDirectory.getSubdirectory("ext4");
                 TemporaryDirectory f2fs = temporaryDirectory.getSubdirectory("f2fs")) {
                try(TemporaryDirectory ext4_part1 = ext4.getSubdirectory("part1");
                    TemporaryDirectory f2fs_part1 = f2fs.getSubdirectory("part1");
                    TemporaryDirectory ext4_part2 = ext4.getSubdirectory("part2");
                    TemporaryDirectory f2f2_notpart = f2fs.getSubdirectory("nopart")) {
                    Files.write(ext4_part1.getPath().resolve("lifetime_write_kbytes"),
                        Collections.singleton("123"));
                    Files.write(f2fs_part1.getPath().resolve("lifetime_write_kbytes"),
                        Collections.singleton("250"));
                    Files.write(ext4_part2.getPath().resolve("lifetime_write_kbytes"),
                        Collections.singleton("2147483660"));

                    LifetimeWriteInfo expected_ext4_part1 =
                        new LifetimeWriteInfo("part1", "ext4", 123*1024);
                    LifetimeWriteInfo expected_f2fs_part1 =
                        new LifetimeWriteInfo("part1", "f2fs", 250*1024);
                    LifetimeWriteInfo expected_ext4_part2 =
                        new LifetimeWriteInfo("part2", "ext4", 2147483660L*1024);

                    SysfsLifetimeWriteInfoProvider sysfsLifetimeWriteInfoProvider =
                        new SysfsLifetimeWriteInfoProvider(temporaryDirectory.getDirectory());

                    LifetimeWriteInfo[] writeInfos = sysfsLifetimeWriteInfoProvider.load();

                    assertThat(writeInfos).isNotNull();
                    assertThat(writeInfos.length).isEqualTo(3);
                    assertTrue(Arrays.stream(writeInfos).anyMatch(
                            li -> li.equals(expected_ext4_part1)));
                    assertTrue(Arrays.stream(writeInfos).anyMatch(
                            li -> li.equals(expected_ext4_part2)));
                    assertTrue(Arrays.stream(writeInfos).anyMatch(
                            li -> li.equals(expected_f2fs_part1)));
                }
            }
        }
    }

    @Test
    @SuppressWarnings("TruthSelfEquals")
    public void testLifetimeWriteInfoEquality() throws Exception {
        LifetimeWriteInfo writeInfo = new LifetimeWriteInfo("part1", "ext4", 123);
        LifetimeWriteInfo writeInfoEq = new LifetimeWriteInfo("part1", "ext4", 123);

        LifetimeWriteInfo writeInfoNeq1 = new LifetimeWriteInfo("part2", "ext4", 123);
        LifetimeWriteInfo writeInfoNeq2 = new LifetimeWriteInfo("part1", "f2fs", 123);
        LifetimeWriteInfo writeInfoNeq3 = new LifetimeWriteInfo("part1", "ext4", 100);

        assertThat(writeInfo).isEqualTo(writeInfo);
        assertThat(writeInfoEq).isEqualTo(writeInfo);
        assertThat(writeInfo).isNotSameAs(writeInfoNeq1);
        assertThat(writeInfo).isNotSameAs(writeInfoNeq2);
        assertThat(writeInfo).isNotSameAs(writeInfoNeq3);
    }

    @Test
    public void testLifetimeWriteInfoParcel() throws Exception {
        LifetimeWriteInfo lifetimeWriteInfo = new LifetimeWriteInfo("part1", "ext4", 1024);

        Parcel p = Parcel.obtain();
        lifetimeWriteInfo.writeToParcel(p, 0);
        p.setDataPosition(0);

        LifetimeWriteInfo parceled = new LifetimeWriteInfo(p);

        assertThat(parceled).isEqualTo(lifetimeWriteInfo);
    }

    @Test
    public void testLifetimeWriteInfoJson() throws Exception {
        try (TemporaryFile temporaryFile = new TemporaryFile(TAG)) {
            LifetimeWriteInfo lifetimeWriteInfo = new LifetimeWriteInfo("part1", "ext4", 1024);

            try (JsonWriter jsonWriter = new JsonWriter(new FileWriter(temporaryFile.getFile()))) {
                lifetimeWriteInfo.writeToJson(jsonWriter);
            }
            JSONObject jsonObject = new JSONObject(
                new String(Files.readAllBytes(temporaryFile.getPath())));
            LifetimeWriteInfo other = new LifetimeWriteInfo(jsonObject);
            assertThat(other).isEqualTo(lifetimeWriteInfo);
        }
    }
}
