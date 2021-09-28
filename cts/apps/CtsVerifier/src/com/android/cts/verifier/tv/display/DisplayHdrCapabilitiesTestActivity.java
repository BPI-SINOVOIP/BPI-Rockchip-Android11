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

package com.android.cts.verifier.tv.display;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.util.Log;
import android.view.Display;

import androidx.annotation.StringRes;

import com.android.cts.verifier.R;
import com.android.cts.verifier.tv.TestSequence;
import com.android.cts.verifier.tv.TestStepBase;
import com.android.cts.verifier.tv.TvAppVerifierActivity;
import com.android.cts.verifier.tv.TvUtil;

import com.google.common.base.Throwables;
import com.google.common.collect.Range;

import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Test to verify the HDR Capabilities API is correctly implemented.
 *
 * <p>This test checks if <a
 * href="https://developer.android.com/reference/android/view/Display.html#isHdr()">Display.isHdr()</a>
 * and <a
 * href="https://developer.android.com/reference/android/view/Display.html#getHdrCapabilities()">Display.getHdrCapabilities()</a>
 * return correct results when 1. HDR Display is connected, 2. non-HDR Display is connected and 3.
 * no display is connected.
 */
public class DisplayHdrCapabilitiesTestActivity extends TvAppVerifierActivity {
    private static final String LOG_TAG = "HdrCapabilitiesTest";
    private static final float MAX_EXPECTED_LUMINANCE = 10_000f;
    private static final int DISPLAY_DISCONNECT_WAIT_TIME_SECONDS = 5;

    private TestSequence mTestSequence;

    @Override
    protected void setInfoResources() {
        setInfoResources(
                R.string.tv_hdr_capabilities_test, R.string.tv_hdr_capabilities_test_info, -1);
    }

    @Override
    public String getTestDetails() {
        return mTestSequence.getFailureDetails();
    }

    @Override
    protected void createTestItems() {
        List<TestStepBase> testSteps = new ArrayList<>();
        testSteps.add(new TvPanelReportedTypesAreSupportedTestStep(this));
        testSteps.add(new TvPanelSupportedTypesAreReportedTestStep(this));
        mTestSequence = new TestSequence(this, testSteps);
        mTestSequence.init();
    }

    private static class TvPanelReportedTypesAreSupportedTestStep extends YesNoTestStep {
        public TvPanelReportedTypesAreSupportedTestStep(TvAppVerifierActivity context) {
            super(context, getInstructionText(context), R.string.tv_yes, R.string.tv_no);
        }

        private static String getInstructionText(Context context) {
            DisplayManager displayManager = context.getSystemService(DisplayManager.class);
            Display display = displayManager.getDisplay(Display.DEFAULT_DISPLAY);

            int[] hdrTypes = display.getHdrCapabilities().getSupportedHdrTypes();
            String hdrTypesString;
            if (hdrTypes.length == 0) {
                hdrTypesString = context.getString(R.string.tv_none);
            } else {
                hdrTypesString =
                        Arrays.stream(hdrTypes)
                                .mapToObj(DisplayHdrCapabilitiesTestActivity::hdrTypeToString)
                                .collect(Collectors.joining(", "));
            }

            return context.getString(
                    R.string.tv_panel_hdr_types_reported_are_supported, hdrTypesString);
        }
    }

    private static class TvPanelSupportedTypesAreReportedTestStep extends YesNoTestStep {
        public TvPanelSupportedTypesAreReportedTestStep(TvAppVerifierActivity context) {
            super(context, getInstructionText(context), R.string.tv_no, R.string.tv_yes);
        }

        private static String getInstructionText(Context context) {
            return context.getString(R.string.tv_panel_hdr_types_supported_are_reported);
        }
    }

    private static String hdrTypeToString(@Display.HdrCapabilities.HdrType int type) {
        switch (type) {
            case Display.HdrCapabilities.HDR_TYPE_DOLBY_VISION:
                return "DOLBY_VISION";
            case Display.HdrCapabilities.HDR_TYPE_HDR10:
                return "HDR10";
            case Display.HdrCapabilities.HDR_TYPE_HLG:
                return "HLG";
            case Display.HdrCapabilities.HDR_TYPE_HDR10_PLUS:
                return "HDR10_PLUS";
            default:
                Log.e(LOG_TAG, "Unknown HDR type " + type);
                return "UNKNOWN";
        }
    }
}
