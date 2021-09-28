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

package com.android.tv.util;

import static androidx.test.InstrumentationRegistry.getContext;

import android.content.pm.ResolveInfo;
import android.media.tv.TvInputInfo;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.tv.testing.ComparatorTester;
import com.android.tv.testing.utils.TestUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** Test for {@link TvInputManagerHelper} */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class TvInputManagerHelperTest {
    final HashMap<String, TvInputInfoWrapper> TEST_INPUT_MAP = new HashMap<>();

    @Test
    public void testComparatorInternal() {
        ResolveInfo resolveInfo = TestUtils.createResolveInfo("test", "test");

        List<TvInputInfo> inputs = new ArrayList<>();
        inputs.add(
                createTvInputInfo(
                        resolveInfo,
                        "2_partner_input",
                        null,
                        0,
                        false,
                        "2_partner_input",
                        null,
                        true));
        inputs.add(
                createTvInputInfo(
                        resolveInfo,
                        "3_partner_input",
                        null,
                        0,
                        false,
                        "3_partner_input",
                        null,
                        true));
        inputs.add(
                createTvInputInfo(
                        resolveInfo,
                        "1_3rd_party_input",
                        null,
                        0,
                        false,
                        "1_3rd_party_input",
                        null,
                        false));
        inputs.add(
                createTvInputInfo(
                        resolveInfo,
                        "4_3rd_party_input",
                        null,
                        0,
                        false,
                        "4_3rd_party_input",
                        null,
                        false));

        TvInputManagerHelper manager = createMockTvInputManager();

        ComparatorTester comparatorTester =
                new ComparatorTester(new TvInputManagerHelper.InputComparatorInternal(manager))
                        .permitInconsistencyWithEquals();
        for (TvInputInfo input : inputs) {
            comparatorTester.addEqualityGroup(input);
        }
        comparatorTester.testCompare();
    }

    @Test
    public void testHardwareInputComparatorHdmi() {
        ResolveInfo resolveInfo = TestUtils.createResolveInfo("test", "test");

        TvInputInfo hdmi1 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI1",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI1",
                        null,
                        false);
        TvInputInfo hdmi2 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI2",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI2",
                        "DVD",
                        false);
        TvInputInfo hdmi3 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI3",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI3",
                        "Cable",
                        false);
        TvInputInfo hdmi4 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI4",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI4",
                        null,
                        false);

        TvInputManagerHelper manager = createMockTvInputManager();

        ComparatorTester comparatorTester =
                new ComparatorTester(
                                new TvInputManagerHelper.HardwareInputComparator(
                                        getContext(), manager))
                        .permitInconsistencyWithEquals();
        comparatorTester
                .addEqualityGroup(hdmi3)
                .addEqualityGroup(hdmi2)
                .addEqualityGroup(hdmi1)
                .addEqualityGroup(hdmi4)
                .testCompare();
    }

    @Test
    public void testHardwareInputComparatorCec() {
        ResolveInfo resolveInfo = TestUtils.createResolveInfo("test", "test");

        TvInputInfo hdmi1 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI1",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI1",
                        null,
                        false);
        TvInputInfo hdmi2 =
                createTvInputInfo(
                        resolveInfo,
                        "HDMI2",
                        null,
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "HDMI2",
                        null,
                        false);

        TvInputInfo cec1 =
                createTvInputInfo(
                        resolveInfo,
                        "2_cec",
                        "HDMI1",
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "2_cec",
                        null,
                        false);
        TvInputInfo cec2 =
                createTvInputInfo(
                        resolveInfo,
                        "1_cec",
                        "HDMI2",
                        TvInputInfo.TYPE_HDMI,
                        true,
                        "1_cec",
                        null,
                        false);

        TvInputManagerHelper manager = createMockTvInputManager();

        ComparatorTester comparatorTester =
                new ComparatorTester(
                                new TvInputManagerHelper.HardwareInputComparator(
                                        getContext(), manager))
                        .permitInconsistencyWithEquals();
        comparatorTester.addEqualityGroup(cec1).addEqualityGroup(cec2).testCompare();
    }

    private TvInputManagerHelper createMockTvInputManager() {
        TvInputManagerHelper manager = Mockito.mock(TvInputManagerHelper.class);
        Mockito.doAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) throws Throwable {
                                TvInputInfo info = (TvInputInfo) invocation.getArguments()[0];
                                return TEST_INPUT_MAP.get(info.getId()).mIsPartnerInput;
                            }
                        })
                .when(manager)
                .isPartnerInput(ArgumentMatchers.<TvInputInfo>any());
        Mockito.doAnswer(
                        new Answer<String>() {
                            @Override
                            public String answer(InvocationOnMock invocation) throws Throwable {
                                TvInputInfo info = (TvInputInfo) invocation.getArguments()[0];
                                return TEST_INPUT_MAP.get(info.getId()).mLabel;
                            }
                        })
                .when(manager)
                .loadLabel(ArgumentMatchers.<TvInputInfo>any());
        Mockito.doAnswer(
                        new Answer<String>() {
                            @Override
                            public String answer(InvocationOnMock invocation) throws Throwable {
                                TvInputInfo info = (TvInputInfo) invocation.getArguments()[0];
                                return TEST_INPUT_MAP.get(info.getId()).mCustomLabel;
                            }
                        })
                .when(manager)
                .loadCustomLabel(ArgumentMatchers.<TvInputInfo>any());
        Mockito.doAnswer(
                        new Answer<TvInputInfo>() {
                            @Override
                            public TvInputInfo answer(InvocationOnMock invocation)
                                    throws Throwable {
                                String inputId = (String) invocation.getArguments()[0];
                                TvInputInfoWrapper inputWrapper = TEST_INPUT_MAP.get(inputId);
                                return inputWrapper == null ? null : inputWrapper.mInput;
                            }
                        })
                .when(manager)
                .getTvInputInfo(ArgumentMatchers.<String>any());
        return manager;
    }

    private TvInputInfo createTvInputInfo(
            ResolveInfo service,
            String id,
            String parentId,
            int type,
            boolean isHardwareInput,
            String label,
            String customLabel,
            boolean isPartnerInput) {
        TvInputInfoWrapper inputWrapper = new TvInputInfoWrapper();
        try {
            inputWrapper.mInput =
                    TestUtils.createTvInputInfo(service, id, parentId, type, isHardwareInput);
        } catch (Exception e) {
        }
        inputWrapper.mLabel = label;
        inputWrapper.mIsPartnerInput = isPartnerInput;
        inputWrapper.mCustomLabel = customLabel;
        TEST_INPUT_MAP.put(id, inputWrapper);
        return inputWrapper.mInput;
    }

    private static class TvInputInfoWrapper {
        TvInputInfo mInput;
        String mLabel;
        String mCustomLabel;
        boolean mIsPartnerInput;
    }
}
