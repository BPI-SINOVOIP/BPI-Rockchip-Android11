/*
 * Copyright 2019 The Android Open Source Project
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
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import android.media.tv.tuner.DemuxCapabilities;
import android.media.tv.tuner.Descrambler;
import android.media.tv.tuner.LnbCallback;
import android.media.tv.tuner.Lnb;
import android.media.tv.tuner.Tuner;
import android.media.tv.tuner.dvr.DvrPlayback;
import android.media.tv.tuner.dvr.DvrRecorder;
import android.media.tv.tuner.dvr.OnPlaybackStatusChangedListener;
import android.media.tv.tuner.dvr.OnRecordStatusChangedListener;

import android.media.tv.tuner.filter.AudioDescriptor;
import android.media.tv.tuner.filter.DownloadEvent;
import android.media.tv.tuner.filter.FilterCallback;
import android.media.tv.tuner.filter.FilterConfiguration;
import android.media.tv.tuner.filter.FilterEvent;
import android.media.tv.tuner.filter.Filter;
import android.media.tv.tuner.filter.IpPayloadEvent;
import android.media.tv.tuner.filter.MediaEvent;
import android.media.tv.tuner.filter.MmtpRecordEvent;
import android.media.tv.tuner.filter.PesEvent;
import android.media.tv.tuner.filter.SectionEvent;
import android.media.tv.tuner.filter.SectionSettingsWithTableInfo;
import android.media.tv.tuner.filter.Settings;
import android.media.tv.tuner.filter.TsFilterConfiguration;
import android.media.tv.tuner.filter.TemiEvent;
import android.media.tv.tuner.filter.TimeFilter;
import android.media.tv.tuner.filter.TsRecordEvent;

import android.media.tv.tuner.frontend.AnalogFrontendCapabilities;
import android.media.tv.tuner.frontend.AnalogFrontendSettings;
import android.media.tv.tuner.frontend.Atsc3FrontendCapabilities;
import android.media.tv.tuner.frontend.Atsc3FrontendSettings;
import android.media.tv.tuner.frontend.Atsc3PlpInfo;
import android.media.tv.tuner.frontend.Atsc3PlpSettings;
import android.media.tv.tuner.frontend.AtscFrontendCapabilities;
import android.media.tv.tuner.frontend.AtscFrontendSettings;
import android.media.tv.tuner.frontend.DvbcFrontendCapabilities;
import android.media.tv.tuner.frontend.DvbcFrontendSettings;
import android.media.tv.tuner.frontend.DvbsCodeRate;
import android.media.tv.tuner.frontend.DvbsFrontendCapabilities;
import android.media.tv.tuner.frontend.DvbsFrontendSettings;
import android.media.tv.tuner.frontend.DvbtFrontendCapabilities;
import android.media.tv.tuner.frontend.DvbtFrontendSettings;
import android.media.tv.tuner.frontend.FrontendCapabilities;
import android.media.tv.tuner.frontend.FrontendInfo;
import android.media.tv.tuner.frontend.FrontendSettings;
import android.media.tv.tuner.frontend.FrontendStatus.Atsc3PlpTuningInfo;
import android.media.tv.tuner.frontend.FrontendStatus;
import android.media.tv.tuner.frontend.Isdbs3FrontendCapabilities;
import android.media.tv.tuner.frontend.Isdbs3FrontendSettings;
import android.media.tv.tuner.frontend.IsdbsFrontendCapabilities;
import android.media.tv.tuner.frontend.IsdbsFrontendSettings;
import android.media.tv.tuner.frontend.IsdbtFrontendCapabilities;
import android.media.tv.tuner.frontend.IsdbtFrontendSettings;
import android.media.tv.tuner.frontend.OnTuneEventListener;
import android.media.tv.tuner.frontend.ScanCallback;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class TunerTest {
    private static final String TAG = "MediaTunerTest";

    private static final int TIMEOUT_MS = 10000;

    private Context mContext;
    private Tuner mTuner;
    private CountDownLatch mLockLatch = new CountDownLatch(1);

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
    public void testTunerConstructor() throws Exception {
        if (!hasTuner()) return;
        assertNotNull(mTuner);
    }

    @Test
    public void testTuning() throws Exception {
        if (!hasTuner()) return;
        List<Integer> ids = mTuner.getFrontendIds();
        assertFalse(ids.isEmpty());

        FrontendInfo info = mTuner.getFrontendInfoById(ids.get(0));
        int res = mTuner.tune(createFrontendSettings(info));
        assertEquals(Tuner.RESULT_SUCCESS, res);
        assertEquals(Tuner.RESULT_SUCCESS, mTuner.setLnaEnabled(false));
        res = mTuner.cancelTuning();
        assertEquals(Tuner.RESULT_SUCCESS, res);
    }

    @Test
    public void testScanning() throws Exception {
        if (!hasTuner()) return;
        List<Integer> ids = mTuner.getFrontendIds();
        assertFalse(ids.isEmpty());
        for (int id : ids) {
            FrontendInfo info = mTuner.getFrontendInfoById(id);
            if (info != null && info.getType() == FrontendSettings.TYPE_ATSC) {
                mLockLatch = new CountDownLatch(1);
                int res = mTuner.scan(
                        createFrontendSettings(info),
                        Tuner.SCAN_TYPE_AUTO,
                        getExecutor(),
                        getScanCallback());
               assertEquals(Tuner.RESULT_SUCCESS, res);
               assertTrue(mLockLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
               res = mTuner.cancelScanning();
               assertEquals(Tuner.RESULT_SUCCESS, res);
            }
        }
        mLockLatch = null;
    }

    @Test
    public void testFrontendStatus() throws Exception {
        if (!hasTuner()) return;
        List<Integer> ids = mTuner.getFrontendIds();
        assertFalse(ids.isEmpty());

        FrontendInfo info = mTuner.getFrontendInfoById(ids.get(0));
        int res = mTuner.tune(createFrontendSettings(info));
        FrontendStatus status = mTuner.getFrontendStatus(
                new int[] {
                        FrontendStatus.FRONTEND_STATUS_TYPE_DEMOD_LOCK,
                        FrontendStatus.FRONTEND_STATUS_TYPE_SNR,
                        FrontendStatus.FRONTEND_STATUS_TYPE_BER,
                        FrontendStatus.FRONTEND_STATUS_TYPE_PER,
                        FrontendStatus.FRONTEND_STATUS_TYPE_PRE_BER,
                        FrontendStatus.FRONTEND_STATUS_TYPE_SIGNAL_QUALITY,
                        FrontendStatus.FRONTEND_STATUS_TYPE_SIGNAL_STRENGTH,
                        FrontendStatus.FRONTEND_STATUS_TYPE_SYMBOL_RATE,
                        FrontendStatus.FRONTEND_STATUS_TYPE_FEC,
                        FrontendStatus.FRONTEND_STATUS_TYPE_MODULATION,
                        FrontendStatus.FRONTEND_STATUS_TYPE_SPECTRAL,
                        FrontendStatus.FRONTEND_STATUS_TYPE_LNB_VOLTAGE,
                        FrontendStatus.FRONTEND_STATUS_TYPE_PLP_ID,
                        FrontendStatus.FRONTEND_STATUS_TYPE_EWBS,
                        FrontendStatus.FRONTEND_STATUS_TYPE_AGC,
                        FrontendStatus.FRONTEND_STATUS_TYPE_LNA,
                        FrontendStatus.FRONTEND_STATUS_TYPE_LAYER_ERROR,
                        FrontendStatus.FRONTEND_STATUS_TYPE_MER,
                        FrontendStatus.FRONTEND_STATUS_TYPE_FREQ_OFFSET,
                        FrontendStatus.FRONTEND_STATUS_TYPE_HIERARCHY,
                        FrontendStatus.FRONTEND_STATUS_TYPE_RF_LOCK,
                        FrontendStatus.FRONTEND_STATUS_TYPE_ATSC3_PLP_INFO
                });
        assertNotNull(status);

        status.isDemodLocked();
        status.getSnr();
        status.getBer();
        status.getPer();
        status.getPerBer();
        status.getSignalQuality();
        status.getSignalStrength();
        status.getSymbolRate();
        status.getInnerFec();
        status.getModulation();
        status.getSpectralInversion();
        status.getLnbVoltage();
        status.getPlpId();
        status.isEwbs();
        status.getAgc();
        status.isLnaOn();
        status.getLayerErrors();
        status.getMer();
        status.getFreqOffset();
        status.getHierarchy();
        status.isRfLocked();
        Atsc3PlpTuningInfo[] tuningInfos = status.getAtsc3PlpTuningInfo();
        if (tuningInfos != null) {
            for (Atsc3PlpTuningInfo tuningInfo : tuningInfos) {
                tuningInfo.getPlpId();
                tuningInfo.isLocked();
                tuningInfo.getUec();
            }
        }
    }

    @Test
    public void testLnb() throws Exception {
        if (!hasTuner()) return;
        Lnb lnb = mTuner.openLnb(getExecutor(), getLnbCallback());
        if (lnb == null) return;
        assertEquals(lnb.setVoltage(Lnb.VOLTAGE_5V), Tuner.RESULT_SUCCESS);
        assertEquals(lnb.setTone(Lnb.TONE_NONE), Tuner.RESULT_SUCCESS);
        assertEquals(
                lnb.setSatellitePosition(Lnb.POSITION_A), Tuner.RESULT_SUCCESS);
        lnb.sendDiseqcMessage(new byte[] {1, 2});
        lnb.close();
    }

    @Test
    public void testOpenLnbByname() throws Exception {
        if (!hasTuner()) return;
        Lnb lnb = mTuner.openLnbByName("default", getExecutor(), getLnbCallback());
        if (lnb != null) {
            lnb.close();
        }
    }

    @Test
    public void testCiCam() throws Exception {
        if (!hasTuner()) return;
        // open filter to get demux resource
        mTuner.openFilter(
                Filter.TYPE_TS, Filter.SUBTYPE_SECTION, 1000, getExecutor(), getFilterCallback());

        mTuner.connectCiCam(1);
        mTuner.disconnectCiCam();
    }

    @Test
    public void testAvSyncId() throws Exception {
        if (!hasTuner()) return;
        // open filter to get demux resource
        Filter f = mTuner.openFilter(
                Filter.TYPE_TS, Filter.SUBTYPE_AUDIO, 1000, getExecutor(), getFilterCallback());
        int id = mTuner.getAvSyncHwId(f);
        if (id != Tuner.INVALID_AV_SYNC_ID) {
            assertNotEquals(Tuner.INVALID_TIMESTAMP, mTuner.getAvSyncTime(id));
        }
    }

    @Test
    public void testReadFilter() throws Exception {
        if (!hasTuner()) return;
        Filter f = mTuner.openFilter(
                Filter.TYPE_TS, Filter.SUBTYPE_SECTION, 1000, getExecutor(), getFilterCallback());
        assertNotNull(f);
        assertNotEquals(Tuner.INVALID_FILTER_ID, f.getId());

        Settings settings = SectionSettingsWithTableInfo
                .builder(Filter.TYPE_TS)
                .setTableId(2)
                .setVersion(1)
                .setCrcEnabled(true)
                .setRaw(false)
                .setRepeat(false)
                .build();
        FilterConfiguration config = TsFilterConfiguration
                .builder()
                .setTpid(10)
                .setSettings(settings)
                .build();
        f.configure(config);
        f.start();
        f.flush();
        f.read(new byte[3], 0, 3);
        f.stop();
        f.close();
    }

    @Test
    public void testTimeFilter() throws Exception {
        if (!hasTuner()) return;
        if (!mTuner.getDemuxCapabilities().isTimeFilterSupported()) return;
        TimeFilter f = mTuner.openTimeFilter();
        assertNotNull(f);
        f.setCurrentTimestamp(0);
        assertNotEquals(Tuner.INVALID_TIMESTAMP, f.getTimeStamp());
        assertNotEquals(Tuner.INVALID_TIMESTAMP, f.getSourceTime());
        f.clearTimestamp();
        f.close();
    }

    @Test
    public void testDescrambler() throws Exception {
        if (!hasTuner()) return;
        Descrambler d = mTuner.openDescrambler();
        assertNotNull(d);
        Filter f = mTuner.openFilter(
                Filter.TYPE_TS, Filter.SUBTYPE_SECTION, 1000, getExecutor(), getFilterCallback());
        d.setKeyToken(new byte[] {1, 3, 2});
        d.addPid(Descrambler.PID_TYPE_T, 1, f);
        d.removePid(Descrambler.PID_TYPE_T, 1, f);
        f.close();
        d.close();
    }

    @Test
    public void testOpenDvrRecorder() throws Exception {
        if (!hasTuner()) return;
        DvrRecorder d = mTuner.openDvrRecorder(100, getExecutor(), getRecordListener());
        assertNotNull(d);
    }

    @Test
    public void testOpenDvPlayback() throws Exception {
        if (!hasTuner()) return;
        DvrPlayback d = mTuner.openDvrPlayback(100, getExecutor(), getPlaybackListener());
        assertNotNull(d);
    }

    @Test
    public void testDemuxCapabilities() throws Exception {
        if (!hasTuner()) return;
        DemuxCapabilities d = mTuner.getDemuxCapabilities();
        assertNotNull(d);

        d.getDemuxCount();
        d.getRecordCount();
        d.getPlaybackCount();
        d.getTsFilterCount();
        d.getSectionFilterCount();
        d.getAudioFilterCount();
        d.getVideoFilterCount();
        d.getPesFilterCount();
        d.getPcrFilterCount();
        d.getSectionFilterLength();
        d.getFilterCapabilities();
        d.getLinkCapabilities();
        d.isTimeFilterSupported();
    }

    @Test
    public void testResourceLostListener() throws Exception {
        if (!hasTuner()) return;
        mTuner.setResourceLostListener(getExecutor(), new Tuner.OnResourceLostListener() {
            @Override
            public void onResourceLost(Tuner tuner) {}
        });
        mTuner.clearResourceLostListener();
    }

    @Test
    public void testOnTuneEventListener() throws Exception {
        if (!hasTuner()) return;
        mTuner.setOnTuneEventListener(getExecutor(), new OnTuneEventListener() {
            @Override
            public void onTuneEvent(int tuneEvent) {}
        });
        mTuner.clearOnTuneEventListener();
    }

    @Test
    public void testUpdateResourcePriority() throws Exception {
        if (!hasTuner()) return;
        mTuner.updateResourcePriority(100, 20);
    }

    @Test
    public void testShareFrontendFromTuner() throws Exception {
        if (!hasTuner()) return;
        Tuner other = new Tuner(mContext, null, 100);
        List<Integer> ids = other.getFrontendIds();
        assertFalse(ids.isEmpty());
        FrontendInfo info = other.getFrontendInfoById(ids.get(0));

	// call tune() to open frontend resource
        int res = other.tune(createFrontendSettings(info));
        assertEquals(Tuner.RESULT_SUCCESS, res);
        assertNotNull(other.getFrontendInfo());
        mTuner.shareFrontendFromTuner(other);
        other.close();
    }

    private boolean hasTuner() {
        return mContext.getPackageManager().hasSystemFeature("android.hardware.tv.tuner");
    }

    private Executor getExecutor() {
        return Runnable::run;
    }

    private LnbCallback getLnbCallback() {
        return new LnbCallback() {
            @Override
            public void onEvent(int lnbEventType) {}
            @Override
            public void onDiseqcMessage(byte[] diseqcMessage) {}
        };
    }

    private FilterCallback getFilterCallback() {
        return new FilterCallback() {
            @Override
            public void onFilterEvent(Filter filter, FilterEvent[] events) {
                for (FilterEvent e : events) {
                    if (e instanceof DownloadEvent) {
                        testDownloadEvent(filter, (DownloadEvent) e);
                    } else if (e instanceof IpPayloadEvent) {
                        testIpPayloadEvent(filter, (IpPayloadEvent) e);
                    } else if (e instanceof MediaEvent) {
                        testMediaEvent(filter, (MediaEvent) e);
                    } else if (e instanceof MmtpRecordEvent) {
                        testMmtpRecordEvent(filter, (MmtpRecordEvent) e);
                    } else if (e instanceof PesEvent) {
                        testPesEvent(filter, (PesEvent) e);
                    } else if (e instanceof SectionEvent) {
                        testSectionEvent(filter, (SectionEvent) e);
                    } else if (e instanceof TemiEvent) {
                        testTemiEvent(filter, (TemiEvent) e);
                    } else if (e instanceof TsRecordEvent) {
                        testTsRecordEvent(filter, (TsRecordEvent) e);
                    }
                }
            }
            @Override
            public void onFilterStatusChanged(Filter filter, int status) {}
        };
    }

    private void testDownloadEvent(Filter filter, DownloadEvent e) {
        e.getItemId();
        e.getMpuSequenceNumber();
        e.getItemFragmentIndex();
        e.getLastItemFragmentIndex();
        long length = e.getDataLength();
        if (length > 0) {
            byte[] buffer = new byte[(int) length];
            assertNotEquals(0, filter.read(buffer, 0, length));
        }
    }

    private void testIpPayloadEvent(Filter filter, IpPayloadEvent e) {
        long length = e.getDataLength();
        if (length > 0) {
            byte[] buffer = new byte[(int) length];
            assertNotEquals(0, filter.read(buffer, 0, length));
        }
    }

    private void testMediaEvent(Filter filter, MediaEvent e) {
        e.getStreamId();
        e.isPtsPresent();
        e.getPts();
        e.getDataLength();
        e.getOffset();
        e.getLinearBlock();
        e.isSecureMemory();
        e.getAvDataId();
        e.getAudioHandle();
        e.getMpuSequenceNumber();
        e.isPrivateData();
        AudioDescriptor ad = e.getExtraMetaData();
        if (ad != null) {
            ad.getAdFade();
            ad.getAdPan();
            ad.getAdVersionTextTag();
            ad.getAdGainCenter();
            ad.getAdGainFront();
            ad.getAdGainSurround();
        }
    }

    private void testMmtpRecordEvent(Filter filter, MmtpRecordEvent e) {
        e.getScHevcIndexMask();
        e.getDataLength();
    }

    private void testPesEvent(Filter filter, PesEvent e) {
        e.getStreamId();
        e.getMpuSequenceNumber();
        long length = e.getDataLength();
        if (length > 0) {
            byte[] buffer = new byte[(int) length];
            assertNotEquals(0, filter.read(buffer, 0, length));
        }
    }

    private void testSectionEvent(Filter filter, SectionEvent e) {
        e.getTableId();
        e.getVersion();
        e.getSectionNumber();
        long length = e.getDataLength();
        if (length > 0) {
            byte[] buffer = new byte[(int) length];
            assertNotEquals(0, filter.read(buffer, 0, length));
        }
    }

    private void testTemiEvent(Filter filter, TemiEvent e) {
        e.getPts();
        e.getDescriptorTag();
        e.getDescriptorData();
    }

    private void testTsRecordEvent(Filter filter, TsRecordEvent e) {
        e.getPacketId();
        e.getTsIndexMask();
        e.getScIndexMask();
        e.getDataLength();
    }

    private OnRecordStatusChangedListener getRecordListener() {
        return new OnRecordStatusChangedListener() {
            @Override
            public void onRecordStatusChanged(int status) {}
        };
    }

    private OnPlaybackStatusChangedListener getPlaybackListener() {
        return new OnPlaybackStatusChangedListener() {
            @Override
            public void onPlaybackStatusChanged(int status) {}
        };
    }

    private FrontendSettings createFrontendSettings(FrontendInfo info) {
            FrontendCapabilities caps = info.getFrontendCapabilities();
            int minFreq = info.getFrequencyRange().getLower();
            FrontendCapabilities feCaps = info.getFrontendCapabilities();
            switch(info.getType()) {
                case FrontendSettings.TYPE_ANALOG: {
                    AnalogFrontendCapabilities analogCaps = (AnalogFrontendCapabilities) caps;
                    int signalType = getFirstCapable(analogCaps.getSignalTypeCapability());
                    int sif = getFirstCapable(analogCaps.getSifStandardCapability());
                    return AnalogFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setSignalType(signalType)
                            .setSifStandard(sif)
                            .build();
                }
                case FrontendSettings.TYPE_ATSC3: {
                    Atsc3FrontendCapabilities atsc3Caps = (Atsc3FrontendCapabilities) caps;
                    int bandwidth = getFirstCapable(atsc3Caps.getBandwidthCapability());
                    int demod = getFirstCapable(atsc3Caps.getDemodOutputFormatCapability());
                    return Atsc3FrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setBandwidth(bandwidth)
                            .setDemodOutputFormat(demod)
                            .build();
                }
                case FrontendSettings.TYPE_ATSC: {
                    AtscFrontendCapabilities atscCaps = (AtscFrontendCapabilities) caps;
                    int modulation = getFirstCapable(atscCaps.getModulationCapability());
                    return AtscFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .build();
                }
                case FrontendSettings.TYPE_DVBC: {
                    DvbcFrontendCapabilities dvbcCaps = (DvbcFrontendCapabilities) caps;
                    int modulation = getFirstCapable(dvbcCaps.getModulationCapability());
                    int fec = getFirstCapable(dvbcCaps.getFecCapability());
                    int annex = getFirstCapable(dvbcCaps.getAnnexCapability());
                    return DvbcFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .setInnerFec(fec)
                            .setAnnex(annex)
                            .build();
                }
                case FrontendSettings.TYPE_DVBS: {
                    DvbsFrontendCapabilities dvbsCaps = (DvbsFrontendCapabilities) caps;
                    int modulation = getFirstCapable(dvbsCaps.getModulationCapability());
                    int standard = getFirstCapable(dvbsCaps.getStandardCapability());
                    return DvbsFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .setStandard(standard)
                            .build();
                }
                case FrontendSettings.TYPE_DVBT: {
                    DvbtFrontendCapabilities dvbtCaps = (DvbtFrontendCapabilities) caps;
                    int transmission = getFirstCapable(dvbtCaps.getTransmissionModeCapability());
                    int bandwidth = getFirstCapable(dvbtCaps.getBandwidthCapability());
                    int constellation = getFirstCapable(dvbtCaps.getConstellationCapability());
                    int codeRate = getFirstCapable(dvbtCaps.getCodeRateCapability());
                    int hierarchy = getFirstCapable(dvbtCaps.getHierarchyCapability());
                    int guardInterval = getFirstCapable(dvbtCaps.getGuardIntervalCapability());
                    return DvbtFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setTransmissionMode(transmission)
                            .setBandwidth(bandwidth)
                            .setConstellation(constellation)
                            .setHierarchy(hierarchy)
                            .setHighPriorityCodeRate(codeRate)
                            .setLowPriorityCodeRate(codeRate)
                            .setGuardInterval(guardInterval)
                            .setStandard(DvbtFrontendSettings.STANDARD_T)
                            .setMiso(false)
                            .build();
                }
                case FrontendSettings.TYPE_ISDBS3: {
                    Isdbs3FrontendCapabilities isdbs3Caps = (Isdbs3FrontendCapabilities) caps;
                    int modulation = getFirstCapable(isdbs3Caps.getModulationCapability());
                    int codeRate = getFirstCapable(isdbs3Caps.getCodeRateCapability());
                    return Isdbs3FrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .setCodeRate(codeRate)
                            .build();
                }
                case FrontendSettings.TYPE_ISDBS: {
                    IsdbsFrontendCapabilities isdbsCaps = (IsdbsFrontendCapabilities) caps;
                    int modulation = getFirstCapable(isdbsCaps.getModulationCapability());
                    int codeRate = getFirstCapable(isdbsCaps.getCodeRateCapability());
                    return IsdbsFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .setCodeRate(codeRate)
                            .build();
                }
                case FrontendSettings.TYPE_ISDBT: {
                    IsdbtFrontendCapabilities isdbtCaps = (IsdbtFrontendCapabilities) caps;
                    int mode = getFirstCapable(isdbtCaps.getModeCapability());
                    int bandwidth = getFirstCapable(isdbtCaps.getBandwidthCapability());
                    int modulation = getFirstCapable(isdbtCaps.getModulationCapability());
                    int codeRate = getFirstCapable(isdbtCaps.getCodeRateCapability());
                    int guardInterval = getFirstCapable(isdbtCaps.getGuardIntervalCapability());
                    return IsdbtFrontendSettings
                            .builder()
                            .setFrequency(minFreq)
                            .setModulation(modulation)
                            .setBandwidth(bandwidth)
                            .setMode(mode)
                            .setCodeRate(codeRate)
                            .setGuardInterval(guardInterval)
                            .build();
                }
                default:
                    break;
            }
        return null;
    }

    private int getFirstCapable(int caps) {
        if (caps == 0) return 0;
        int mask = 1;
        while ((mask & caps) == 0) {
            mask = mask << 1;
        }
        return (mask & caps);
    }

    private long getFirstCapable(long caps) {
        if (caps == 0) return 0;
        long mask = 1;
        while ((mask & caps) == 0) {
            mask = mask << 1;
        }
        return (mask & caps);
    }

    private ScanCallback getScanCallback() {
        return new ScanCallback() {
            @Override
            public void onLocked() {
                if (mLockLatch != null) {
                    mLockLatch.countDown();
                }
            }

            @Override
            public void onScanStopped() {}

            @Override
            public void onProgress(int percent) {}

            @Override
            public void onFrequenciesReported(int[] frequency) {}

            @Override
            public void onSymbolRatesReported(int[] rate) {}

            @Override
            public void onPlpIdsReported(int[] plpIds) {}

            @Override
            public void onGroupIdsReported(int[] groupIds) {}

            @Override
            public void onInputStreamIdsReported(int[] inputStreamIds) {}

            @Override
            public void onDvbsStandardReported(int dvbsStandard) {}

            @Override
            public void onDvbtStandardReported(int dvbtStandard) {}

            @Override
            public void onAnalogSifStandardReported(int sif) {}

            @Override
            public void onAtsc3PlpInfosReported(Atsc3PlpInfo[] atsc3PlpInfos) {
                for (Atsc3PlpInfo info : atsc3PlpInfos) {
                    if (info != null) {
                        info.getPlpId();
                        info.getLlsFlag();
                    }
                }
            }

            @Override
            public void onHierarchyReported(int hierarchy) {}

            @Override
            public void onSignalTypeReported(int signalType) {}
        };
    }
}
