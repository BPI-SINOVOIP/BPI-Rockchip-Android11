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
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.media.tv.tuner.Tuner;
import android.media.tv.tuner.frontend.AnalogFrontendCapabilities;
import android.media.tv.tuner.frontend.AnalogFrontendSettings;
import android.media.tv.tuner.frontend.Atsc3FrontendCapabilities;
import android.media.tv.tuner.frontend.Atsc3FrontendSettings;
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
import android.media.tv.tuner.frontend.Isdbs3FrontendCapabilities;
import android.media.tv.tuner.frontend.Isdbs3FrontendSettings;
import android.media.tv.tuner.frontend.IsdbsFrontendCapabilities;
import android.media.tv.tuner.frontend.IsdbsFrontendSettings;
import android.media.tv.tuner.frontend.IsdbtFrontendCapabilities;
import android.media.tv.tuner.frontend.IsdbtFrontendSettings;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import java.util.List;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class TunerFrontendTest {
    private static final String TAG = "MediaTunerFrontendTest";

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
    public void testAnalogFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        AnalogFrontendSettings settings =
                AnalogFrontendSettings
                        .builder()
                        .setFrequency(1)
                        .setSignalType(AnalogFrontendSettings.SIGNAL_TYPE_NTSC)
                        .setSifStandard(AnalogFrontendSettings.SIF_BG_NICAM)
                        .build();

        assertEquals(FrontendSettings.TYPE_ANALOG, settings.getType());
        assertEquals(1, settings.getFrequency());
        assertEquals(AnalogFrontendSettings.SIGNAL_TYPE_NTSC, settings.getSignalType());
        assertEquals(AnalogFrontendSettings.SIF_BG_NICAM, settings.getSifStandard());
    }

    @Test
    public void testAtsc3FrontendSettings() throws Exception {
        if (!hasTuner()) return;

        Atsc3PlpSettings plp1 =
                Atsc3PlpSettings
                        .builder()
                        .setPlpId(1)
                        .setModulation(Atsc3FrontendSettings.MODULATION_MOD_QPSK)
                        .setInterleaveMode(Atsc3FrontendSettings.TIME_INTERLEAVE_MODE_AUTO)
                        .setCodeRate(Atsc3FrontendSettings.CODERATE_6_15)
                        .setFec(Atsc3FrontendSettings.FEC_BCH_LDPC_64K)
                        .build();

        Atsc3PlpSettings plp2 =
                Atsc3PlpSettings
                        .builder()
                        .setPlpId(2)
                        .setModulation(Atsc3FrontendSettings.MODULATION_MOD_QPSK)
                        .setInterleaveMode(Atsc3FrontendSettings.TIME_INTERLEAVE_MODE_HTI)
                        .setCodeRate(Atsc3FrontendSettings.CODERATE_UNDEFINED)
                        .setFec(Atsc3FrontendSettings.FEC_LDPC_16K)
                        .build();

        Atsc3FrontendSettings settings =
                Atsc3FrontendSettings
                        .builder()
                        .setFrequency(2)
                        .setBandwidth(Atsc3FrontendSettings.BANDWIDTH_BANDWIDTH_6MHZ)
                        .setDemodOutputFormat(Atsc3FrontendSettings.MODULATION_MOD_QPSK)
                        .setPlpSettings(new Atsc3PlpSettings[] {plp1, plp2})
                        .build();

        assertEquals(FrontendSettings.TYPE_ATSC3, settings.getType());
        assertEquals(2, settings.getFrequency());
        assertEquals(Atsc3FrontendSettings.BANDWIDTH_BANDWIDTH_6MHZ, settings.getBandwidth());
        assertEquals(Atsc3FrontendSettings.MODULATION_MOD_QPSK, settings.getDemodOutputFormat());

        Atsc3PlpSettings[] plps = settings.getPlpSettings();
        assertEquals(2, plps.length);

        assertEquals(1, plps[0].getPlpId());
        assertEquals(Atsc3FrontendSettings.MODULATION_MOD_QPSK, plps[0].getModulation());
        assertEquals(Atsc3FrontendSettings.TIME_INTERLEAVE_MODE_AUTO, plps[0].getInterleaveMode());
        assertEquals(Atsc3FrontendSettings.CODERATE_6_15, plps[0].getCodeRate());
        assertEquals(Atsc3FrontendSettings.FEC_BCH_LDPC_64K, plps[0].getFec());

        assertEquals(2, plps[1].getPlpId());
        assertEquals(Atsc3FrontendSettings.MODULATION_MOD_QPSK, plps[1].getModulation());
        assertEquals(Atsc3FrontendSettings.TIME_INTERLEAVE_MODE_HTI, plps[1].getInterleaveMode());
        assertEquals(Atsc3FrontendSettings.CODERATE_UNDEFINED, plps[1].getCodeRate());
        assertEquals(Atsc3FrontendSettings.FEC_LDPC_16K, plps[1].getFec());
    }

    @Test
    public void testAtscFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        AtscFrontendSettings settings =
                AtscFrontendSettings
                        .builder()
                        .setFrequency(3)
                        .setModulation(AtscFrontendSettings.MODULATION_MOD_8VSB)
                        .build();

        assertEquals(FrontendSettings.TYPE_ATSC, settings.getType());
        assertEquals(3, settings.getFrequency());
        assertEquals(AtscFrontendSettings.MODULATION_MOD_8VSB, settings.getModulation());
    }

    @Test
    public void testDvbcFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        DvbcFrontendSettings settings =
                DvbcFrontendSettings
                        .builder()
                        .setFrequency(4)
                        .setModulation(DvbcFrontendSettings.MODULATION_MOD_32QAM)
                        .setInnerFec(FrontendSettings.FEC_8_15)
                        .setSymbolRate(3)
                        .setOuterFec(DvbcFrontendSettings.OUTER_FEC_OUTER_FEC_RS)
                        .setAnnex(DvbcFrontendSettings.ANNEX_B)
                        .setSpectralInversion(DvbcFrontendSettings.SPECTRAL_INVERSION_NORMAL)
                        .build();

        assertEquals(FrontendSettings.TYPE_DVBC, settings.getType());
        assertEquals(4, settings.getFrequency());
        assertEquals(DvbcFrontendSettings.MODULATION_MOD_32QAM, settings.getModulation());
        assertEquals(FrontendSettings.FEC_8_15, settings.getInnerFec());
        assertEquals(3, settings.getSymbolRate());
        assertEquals(DvbcFrontendSettings.OUTER_FEC_OUTER_FEC_RS, settings.getOuterFec());
        assertEquals(DvbcFrontendSettings.ANNEX_B, settings.getAnnex());
        assertEquals(
                DvbcFrontendSettings.SPECTRAL_INVERSION_NORMAL, settings.getSpectralInversion());
    }

    @Test
    public void testDvbsFrontendSettings() throws Exception {
        if (!hasTuner()) return;

        DvbsCodeRate codeRate =
                DvbsCodeRate
                        .builder()
                        .setInnerFec(FrontendSettings.FEC_9_10)
                        .setLinear(true)
                        .setShortFrameEnabled(false)
                        .setBitsPer1000Symbol(55)
                        .build();

        DvbsFrontendSettings settings =
                DvbsFrontendSettings
                        .builder()
                        .setFrequency(5)
                        .setModulation(DvbsFrontendSettings.MODULATION_MOD_ACM)
                        .setCodeRate(codeRate)
                        .setSymbolRate(3)
                        .setRolloff(DvbsFrontendSettings.ROLLOFF_0_15)
                        .setPilot(DvbsFrontendSettings.PILOT_OFF)
                        .setInputStreamId(1)
                        .setStandard(DvbsFrontendSettings.STANDARD_S2)
                        .setVcmMode(DvbsFrontendSettings.VCM_MODE_MANUAL)
                        .build();

        assertEquals(FrontendSettings.TYPE_DVBS, settings.getType());
        assertEquals(5, settings.getFrequency());
        assertEquals(DvbsFrontendSettings.MODULATION_MOD_ACM, settings.getModulation());
        assertEquals(3, settings.getSymbolRate());
        assertEquals(DvbsFrontendSettings.ROLLOFF_0_15, settings.getRolloff());
        assertEquals(DvbsFrontendSettings.PILOT_OFF, settings.getPilot());
        assertEquals(1, settings.getInputStreamId());
        assertEquals(DvbsFrontendSettings.STANDARD_S2, settings.getStandard());
        assertEquals(DvbsFrontendSettings.VCM_MODE_MANUAL, settings.getVcmMode());

        DvbsCodeRate cr = settings.getCodeRate();
        assertNotNull(cr);
        assertEquals(FrontendSettings.FEC_9_10, cr.getInnerFec());
        assertEquals(true, cr.isLinear());
        assertEquals(false, cr.isShortFrameEnabled());
        assertEquals(55, cr.getBitsPer1000Symbol());
    }

    @Test
    public void testDvbtFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        DvbtFrontendSettings settings =
                DvbtFrontendSettings
                        .builder()
                        .setFrequency(6)
                        .setTransmissionMode(DvbtFrontendSettings.TRANSMISSION_MODE_8K)
                        .setBandwidth(DvbtFrontendSettings.BANDWIDTH_1_7MHZ)
                        .setConstellation(DvbtFrontendSettings.CONSTELLATION_256QAM)
                        .setHierarchy(DvbtFrontendSettings.HIERARCHY_4_NATIVE)
                        .setHighPriorityCodeRate(DvbtFrontendSettings.CODERATE_6_7)
                        .setLowPriorityCodeRate(DvbtFrontendSettings.CODERATE_2_3)
                        .setGuardInterval(DvbtFrontendSettings.GUARD_INTERVAL_19_128)
                        .setHighPriority(true)
                        .setStandard(DvbtFrontendSettings.STANDARD_T2)
                        .setMiso(false)
                        .setPlpMode(DvbtFrontendSettings.PLP_MODE_MANUAL)
                        .setPlpId(333)
                        .setPlpGroupId(777)
                        .build();

        assertEquals(FrontendSettings.TYPE_DVBT, settings.getType());
        assertEquals(6, settings.getFrequency());
        assertEquals(DvbtFrontendSettings.TRANSMISSION_MODE_8K, settings.getTransmissionMode());
        assertEquals(DvbtFrontendSettings.BANDWIDTH_1_7MHZ, settings.getBandwidth());
        assertEquals(DvbtFrontendSettings.CONSTELLATION_256QAM, settings.getConstellation());
        assertEquals(DvbtFrontendSettings.HIERARCHY_4_NATIVE, settings.getHierarchy());
        assertEquals(DvbtFrontendSettings.CODERATE_6_7, settings.getHighPriorityCodeRate());
        assertEquals(DvbtFrontendSettings.CODERATE_2_3, settings.getLowPriorityCodeRate());
        assertEquals(DvbtFrontendSettings.GUARD_INTERVAL_19_128, settings.getGuardInterval());
        assertEquals(true, settings.isHighPriority());
        assertEquals(DvbtFrontendSettings.STANDARD_T2, settings.getStandard());
        assertEquals(false, settings.isMiso());
        assertEquals(DvbtFrontendSettings.PLP_MODE_MANUAL, settings.getPlpMode());
        assertEquals(333, settings.getPlpId());
        assertEquals(777, settings.getPlpGroupId());
    }

    @Test
    public void testIsdbs3FrontendSettings() throws Exception {
        if (!hasTuner()) return;
        Isdbs3FrontendSettings settings =
                Isdbs3FrontendSettings
                        .builder()
                        .setFrequency(7)
                        .setStreamId(2)
                        .setStreamIdType(IsdbsFrontendSettings.STREAM_ID_TYPE_ID)
                        .setModulation(Isdbs3FrontendSettings.MODULATION_MOD_BPSK)
                        .setCodeRate(Isdbs3FrontendSettings.CODERATE_1_3)
                        .setSymbolRate(555)
                        .setRolloff(Isdbs3FrontendSettings.ROLLOFF_0_03)
                        .build();

        assertEquals(FrontendSettings.TYPE_ISDBS3, settings.getType());
        assertEquals(7, settings.getFrequency());
        assertEquals(2, settings.getStreamId());
        assertEquals(IsdbsFrontendSettings.STREAM_ID_TYPE_ID, settings.getStreamIdType());
        assertEquals(Isdbs3FrontendSettings.MODULATION_MOD_BPSK, settings.getModulation());
        assertEquals(Isdbs3FrontendSettings.CODERATE_1_3, settings.getCodeRate());
        assertEquals(555, settings.getSymbolRate());
        assertEquals(Isdbs3FrontendSettings.ROLLOFF_0_03, settings.getRolloff());
    }

    @Test
    public void testIsdbsFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        IsdbsFrontendSettings settings =
                IsdbsFrontendSettings
                        .builder()
                        .setFrequency(8)
                        .setStreamId(3)
                        .setStreamIdType(IsdbsFrontendSettings.STREAM_ID_TYPE_RELATIVE_NUMBER)
                        .setModulation(IsdbsFrontendSettings.MODULATION_AUTO)
                        .setCodeRate(IsdbsFrontendSettings.CODERATE_3_4)
                        .setSymbolRate(667)
                        .setRolloff(IsdbsFrontendSettings.ROLLOFF_0_35)
                        .build();

        assertEquals(FrontendSettings.TYPE_ISDBS, settings.getType());
        assertEquals(8, settings.getFrequency());
        assertEquals(3, settings.getStreamId());
        assertEquals(
                IsdbsFrontendSettings.STREAM_ID_TYPE_RELATIVE_NUMBER, settings.getStreamIdType());
        assertEquals(IsdbsFrontendSettings.MODULATION_AUTO, settings.getModulation());
        assertEquals(IsdbsFrontendSettings.CODERATE_3_4, settings.getCodeRate());
        assertEquals(667, settings.getSymbolRate());
        assertEquals(IsdbsFrontendSettings.ROLLOFF_0_35, settings.getRolloff());
    }

    @Test
    public void testIsdbtFrontendSettings() throws Exception {
        if (!hasTuner()) return;
        IsdbtFrontendSettings settings =
                IsdbtFrontendSettings
                        .builder()
                        .setFrequency(9)
                        .setModulation(IsdbtFrontendSettings.MODULATION_MOD_64QAM)
                        .setBandwidth(IsdbtFrontendSettings.BANDWIDTH_8MHZ)
                        .setMode(IsdbtFrontendSettings.MODE_2)
                        .setCodeRate(DvbtFrontendSettings.CODERATE_7_8)
                        .setGuardInterval(DvbtFrontendSettings.GUARD_INTERVAL_1_4)
                        .setServiceAreaId(10)
                        .build();

        assertEquals(FrontendSettings.TYPE_ISDBT, settings.getType());
        assertEquals(9, settings.getFrequency());
        assertEquals(IsdbtFrontendSettings.MODULATION_MOD_64QAM, settings.getModulation());
        assertEquals(IsdbtFrontendSettings.BANDWIDTH_8MHZ, settings.getBandwidth());
        assertEquals(IsdbtFrontendSettings.MODE_2, settings.getMode());
        assertEquals(DvbtFrontendSettings.CODERATE_7_8, settings.getCodeRate());
        assertEquals(DvbtFrontendSettings.GUARD_INTERVAL_1_4, settings.getGuardInterval());
        assertEquals(10, settings.getServiceAreaId());
    }

    @Test
    public void testFrontendInfo() throws Exception {
        if (!hasTuner()) return;
        List<Integer> ids = mTuner.getFrontendIds();
        for (int id : ids) {
            FrontendInfo info = mTuner.getFrontendInfoById(id);
            assertNotNull(info);
            assertEquals(id, info.getId());
            assertTrue(info.getFrequencyRange().getLower() > 0);
            assertTrue(info.getSymbolRateRange().getLower() >= 0);
            assertTrue(info.getAcquireRange() > 0);
            info.getExclusiveGroupId();
            info.getStatusCapabilities();

            FrontendCapabilities caps = info.getFrontendCapabilities();
            assertNotNull(caps);
            switch(info.getType()) {
                case FrontendSettings.TYPE_ANALOG:
                    testAnalogFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_ATSC3:
                    testAtsc3FrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_ATSC:
                    testAtscFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_DVBC:
                    testDvbcFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_DVBS:
                    testDvbsFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_DVBT:
                    testDvbtFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_ISDBS3:
                    testIsdbs3FrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_ISDBS:
                    testIsdbsFrontendCapabilities(caps);
                    break;
                case FrontendSettings.TYPE_ISDBT:
                    testIsdbtFrontendCapabilities(caps);
                    break;
                default:
                    break;
            }
        }
    }

    private void testAnalogFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof AnalogFrontendCapabilities);
        AnalogFrontendCapabilities analogCaps = (AnalogFrontendCapabilities) caps;
        analogCaps.getSignalTypeCapability();
        analogCaps.getSifStandardCapability();
    }

    private void testAtsc3FrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof Atsc3FrontendCapabilities);
        Atsc3FrontendCapabilities atsc3Caps = (Atsc3FrontendCapabilities) caps;
        atsc3Caps.getBandwidthCapability();
        atsc3Caps.getModulationCapability();
        atsc3Caps.getTimeInterleaveModeCapability();
        atsc3Caps.getPlpCodeRateCapability();
        atsc3Caps.getFecCapability();
        atsc3Caps.getDemodOutputFormatCapability();
    }

    private void testAtscFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof AtscFrontendCapabilities);
        AtscFrontendCapabilities atscCaps = (AtscFrontendCapabilities) caps;
        atscCaps.getModulationCapability();
    }

    private void testDvbcFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof DvbcFrontendCapabilities);
        DvbcFrontendCapabilities dvbcCaps = (DvbcFrontendCapabilities) caps;
        dvbcCaps.getModulationCapability();
        dvbcCaps.getFecCapability();
        dvbcCaps.getAnnexCapability();
    }

    private void testDvbsFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof DvbsFrontendCapabilities);
        DvbsFrontendCapabilities dvbsCaps = (DvbsFrontendCapabilities) caps;
        dvbsCaps.getModulationCapability();
        dvbsCaps.getInnerFecCapability();
        dvbsCaps.getStandardCapability();
    }

    private void testDvbtFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof DvbtFrontendCapabilities);
        DvbtFrontendCapabilities dvbtCaps = (DvbtFrontendCapabilities) caps;
        dvbtCaps.getTransmissionModeCapability();
        dvbtCaps.getBandwidthCapability();
        dvbtCaps.getConstellationCapability();
        dvbtCaps.getCodeRateCapability();
        dvbtCaps.getHierarchyCapability();
        dvbtCaps.getGuardIntervalCapability();
        dvbtCaps.isT2Supported();
        dvbtCaps.isMisoSupported();
    }

    private void testIsdbs3FrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof Isdbs3FrontendCapabilities);
        Isdbs3FrontendCapabilities isdbs3Caps = (Isdbs3FrontendCapabilities) caps;
        isdbs3Caps.getModulationCapability();
        isdbs3Caps.getCodeRateCapability();
    }

    private void testIsdbsFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof IsdbsFrontendCapabilities);
        IsdbsFrontendCapabilities isdbsCaps = (IsdbsFrontendCapabilities) caps;
        isdbsCaps.getModulationCapability();
        isdbsCaps.getCodeRateCapability();
    }

    private void testIsdbtFrontendCapabilities(FrontendCapabilities caps) throws Exception {
        assertTrue(caps instanceof IsdbtFrontendCapabilities);
        IsdbtFrontendCapabilities isdbtCaps = (IsdbtFrontendCapabilities) caps;
        isdbtCaps.getModeCapability();
        isdbtCaps.getBandwidthCapability();
        isdbtCaps.getModulationCapability();
        isdbtCaps.getCodeRateCapability();
        isdbtCaps.getGuardIntervalCapability();
    }

    private boolean hasTuner() {
        return mContext.getPackageManager().hasSystemFeature("android.hardware.tv.tuner");
    }
}
