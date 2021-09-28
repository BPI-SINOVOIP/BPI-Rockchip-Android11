/*
 * Copyright 2020 The Android Open Source Project
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

package android.media.tv.tuner.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.media.tv.tuner.Tuner;
import android.media.tv.tuner.filter.AlpFilterConfiguration;
import android.media.tv.tuner.filter.AvSettings;
import android.media.tv.tuner.filter.DownloadSettings;
import android.media.tv.tuner.filter.Filter;
import android.media.tv.tuner.filter.IpFilterConfiguration;
import android.media.tv.tuner.filter.MmtpFilterConfiguration;
import android.media.tv.tuner.filter.PesSettings;
import android.media.tv.tuner.filter.RecordSettings;
import android.media.tv.tuner.filter.SectionSettingsWithSectionBits;
import android.media.tv.tuner.filter.SectionSettingsWithTableInfo;
import android.media.tv.tuner.filter.TlvFilterConfiguration;
import android.media.tv.tuner.filter.TsFilterConfiguration;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class TunerFilterTest {
    private static final String TAG = "MediaTunerFilterTest";

    private Context mContext;
    private Tuner mTuner;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        InstrumentationRegistry
                .getInstrumentation().getUiAutomation().adoptShellPermissionIdentity();
        if (!hasTuner()) return;
        mTuner = new Tuner(mContext, null, 100);
    }

    @After
    public void tearDown() {
        if (mTuner != null) {
          mTuner.close();
          mTuner = null;
        }
    }

    @Test
    public void testAvSettings() throws Exception {
        if (!hasTuner()) return;
        AvSettings settings =
                AvSettings
                        .builder(Filter.TYPE_TS, true)
                        .setPassthrough(false)
                        .build();

        assertFalse(settings.isPassthrough());
    }

    @Test
    public void testDownloadSettings() throws Exception {
        if (!hasTuner()) return;
        DownloadSettings settings =
                DownloadSettings
                        .builder(Filter.TYPE_MMTP)
                        .setDownloadId(2)
                        .build();

        assertEquals(2, settings.getDownloadId());
    }

    @Test
    public void testPesSettings() throws Exception {
        if (!hasTuner()) return;
        PesSettings settings =
                PesSettings
                        .builder(Filter.TYPE_TS)
                        .setStreamId(2)
                        .setRaw(true)
                        .build();

        assertEquals(2, settings.getStreamId());
        assertTrue(settings.isRaw());
    }

    @Test
    public void testRecordSettings() throws Exception {
        if (!hasTuner()) return;
        RecordSettings settings =
                RecordSettings
                        .builder(Filter.TYPE_TS)
                        .setTsIndexMask(
                                RecordSettings.TS_INDEX_FIRST_PACKET
                                        | RecordSettings.TS_INDEX_PRIVATE_DATA)
                        .setScIndexType(RecordSettings.INDEX_TYPE_SC)
                        .setScIndexMask(RecordSettings.SC_INDEX_I_FRAME)
                        .build();

        assertEquals(
                RecordSettings.TS_INDEX_FIRST_PACKET | RecordSettings.TS_INDEX_PRIVATE_DATA,
                settings.getTsIndexMask());
        assertEquals(RecordSettings.INDEX_TYPE_SC, settings.getScIndexType());
        assertEquals(RecordSettings.SC_INDEX_I_FRAME, settings.getScIndexMask());
    }

    @Test
    public void testSectionSettingsWithSectionBits() throws Exception {
        if (!hasTuner()) return;
        SectionSettingsWithSectionBits settings =
                SectionSettingsWithSectionBits
                        .builder(Filter.TYPE_TS)
                        .setCrcEnabled(true)
                        .setRepeat(false)
                        .setRaw(false)
                        .setFilter(new byte[] {2, 3, 4})
                        .setMask(new byte[] {7, 6, 5, 4})
                        .setMode(new byte[] {22, 55, 33})
                        .build();

        assertTrue(settings.isCrcEnabled());
        assertFalse(settings.isRepeat());
        assertFalse(settings.isRaw());
        Assert.assertArrayEquals(new byte[] {2, 3, 4}, settings.getFilterBytes());
        Assert.assertArrayEquals(new byte[] {7, 6, 5, 4}, settings.getMask());
        Assert.assertArrayEquals(new byte[] {22, 55, 33}, settings.getMode());
    }

    @Test
    public void testSectionSettingsWithTableInfo() throws Exception {
        if (!hasTuner()) return;
        SectionSettingsWithTableInfo settings =
                SectionSettingsWithTableInfo
                        .builder(Filter.TYPE_TS)
                        .setTableId(11)
                        .setVersion(2)
                        .setCrcEnabled(false)
                        .setRepeat(true)
                        .setRaw(true)
                        .build();

        assertEquals(11, settings.getTableId());
        assertEquals(2, settings.getVersion());
        assertFalse(settings.isCrcEnabled());
        assertTrue(settings.isRepeat());
        assertTrue(settings.isRaw());
    }

    @Test
    public void testAlpFilterConfiguration() throws Exception {
        if (!hasTuner()) return;
        AlpFilterConfiguration config =
                AlpFilterConfiguration
                        .builder()
                        .setPacketType(AlpFilterConfiguration.PACKET_TYPE_COMPRESSED)
                        .setLengthType(AlpFilterConfiguration.LENGTH_TYPE_WITH_ADDITIONAL_HEADER)
                        .setSettings(null)
                        .build();

        assertEquals(Filter.TYPE_ALP, config.getType());
        assertEquals(AlpFilterConfiguration.PACKET_TYPE_COMPRESSED, config.getPacketType());
        assertEquals(
                AlpFilterConfiguration.LENGTH_TYPE_WITH_ADDITIONAL_HEADER, config.getLengthType());
        assertEquals(null, config.getSettings());
    }

    @Test
    public void testIpFilterConfiguration() throws Exception {
        if (!hasTuner()) return;
        IpFilterConfiguration config =
                IpFilterConfiguration
                        .builder()
                        .setSrcIpAddress(new byte[] {(byte) 0xC0, (byte) 0xA8, 0, 1})
                        .setDstIpAddress(new byte[] {(byte) 0xC0, (byte) 0xA8, 3, 4})
                        .setSrcPort(33)
                        .setDstPort(23)
                        .setPassthrough(false)
                        .setSettings(null)
                        .build();

        assertEquals(Filter.TYPE_IP, config.getType());
        Assert.assertArrayEquals(
                new byte[] {(byte) 0xC0, (byte) 0xA8, 0, 1}, config.getSrcIpAddress());
        Assert.assertArrayEquals(
                new byte[] {(byte) 0xC0, (byte) 0xA8, 3, 4}, config.getDstIpAddress());
        assertEquals(33, config.getSrcPort());
        assertEquals(23, config.getDstPort());
        assertFalse(config.isPassthrough());
        assertEquals(null, config.getSettings());
    }

    @Test
    public void testMmtpFilterConfiguration() throws Exception {
        if (!hasTuner()) return;
        MmtpFilterConfiguration config =
                MmtpFilterConfiguration
                        .builder()
                        .setMmtpPacketId(3)
                        .setSettings(null)
                        .build();

        assertEquals(Filter.TYPE_MMTP, config.getType());
        assertEquals(3, config.getMmtpPacketId());
        assertEquals(null, config.getSettings());
    }

    @Test
    public void testTlvFilterConfiguration() throws Exception {
        if (!hasTuner()) return;
        TlvFilterConfiguration config =
                TlvFilterConfiguration
                        .builder()
                        .setPacketType(TlvFilterConfiguration.PACKET_TYPE_IPV4)
                        .setCompressedIpPacket(true)
                        .setPassthrough(false)
                        .setSettings(null)
                        .build();

        assertEquals(Filter.TYPE_TLV, config.getType());
        assertEquals(TlvFilterConfiguration.PACKET_TYPE_IPV4, config.getPacketType());
        assertTrue(config.isCompressedIpPacket());
        assertFalse(config.isPassthrough());
        assertEquals(null, config.getSettings());
    }

    @Test
    public void testTsFilterConfiguration() throws Exception {
        if (!hasTuner()) return;

        PesSettings settings =
                PesSettings
                        .builder(Filter.TYPE_TS)
                        .setStreamId(3)
                        .setRaw(false)
                        .build();

        TsFilterConfiguration config =
                TsFilterConfiguration
                        .builder()
                        .setTpid(521)
                        .setSettings(settings)
                        .build();

        assertEquals(Filter.TYPE_TS, config.getType());
        assertEquals(521, config.getTpid());

        assertTrue(config.getSettings() instanceof PesSettings);
        PesSettings pes = (PesSettings) config.getSettings();
        assertEquals(3, pes.getStreamId());
        assertFalse(pes.isRaw());
    }


    private boolean hasTuner() {
        return mContext.getPackageManager().hasSystemFeature("android.hardware.tv.tuner");
    }
}
