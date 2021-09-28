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

package com.android.tv.data;

import android.content.pm.ResolveInfo;
import android.media.tv.TvInputInfo;
import android.util.Pair;

import com.android.tv.testing.ComparatorTester;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.utils.TestUtils;
import com.android.tv.util.SetupUtils;
import com.android.tv.util.TvInputManagerHelper;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.Comparator;
import java.util.LinkedHashMap;

/** Test for {@link TvInputNewComparator} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvInputNewComparatorTest {
    @Test
    public void testComparator() throws Exception {
        LinkedHashMap<String, Pair<Boolean, Boolean>> inputIdToNewInput = new LinkedHashMap<>();
        inputIdToNewInput.put("2_new_input", Pair.create(true, false));
        inputIdToNewInput.put("4_new_input", Pair.create(true, false));
        inputIdToNewInput.put("4_old_input", Pair.create(false, false));
        inputIdToNewInput.put("0_old_input", Pair.create(false, true));
        inputIdToNewInput.put("1_old_input", Pair.create(false, true));
        inputIdToNewInput.put("3_old_input", Pair.create(false, true));

        SetupUtils setupUtils = Mockito.mock(SetupUtils.class);
        Mockito.when(setupUtils.isNewInput(ArgumentMatchers.anyString()))
                .thenAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) throws Throwable {
                                String inputId = (String) invocation.getArguments()[0];
                                return inputIdToNewInput.get(inputId).first;
                            }
                        });
        Mockito.when(setupUtils.isSetupDone(ArgumentMatchers.anyString()))
                .thenAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) throws Throwable {
                                String inputId = (String) invocation.getArguments()[0];
                                return inputIdToNewInput.get(inputId).second;
                            }
                        });
        TvInputManagerHelper inputManager = Mockito.mock(TvInputManagerHelper.class);
        Mockito.when(inputManager.getDefaultTvInputInfoComparator())
                .thenReturn(
                        new Comparator<TvInputInfo>() {
                            @Override
                            public int compare(TvInputInfo lhs, TvInputInfo rhs) {
                                return lhs.getId().compareTo(rhs.getId());
                            }
                        });
        TvInputNewComparator comparator = new TvInputNewComparator(setupUtils, inputManager);
        ComparatorTester comparatorTester =
                new ComparatorTester(comparator).permitInconsistencyWithEquals();
        ResolveInfo resolveInfo = TestUtils.createResolveInfo("test", "test");
        for (String id : inputIdToNewInput.keySet()) {
            // Put mock resolveInfo to prevent NPE in {@link TvInputInfo#toString}
            TvInputInfo info1 =
                    TestUtils.createTvInputInfo(
                            resolveInfo, id, "test1", TvInputInfo.TYPE_TUNER, false);
            TvInputInfo info2 =
                    TestUtils.createTvInputInfo(
                            resolveInfo, id, "test2", TvInputInfo.TYPE_DISPLAY_PORT, true);
            TvInputInfo info3 =
                    TestUtils.createTvInputInfo(
                            resolveInfo, id, "test", TvInputInfo.TYPE_HDMI, true);
            comparatorTester.addEqualityGroup(info1, info2, info3);
        }
        comparatorTester.testCompare();
    }
}
