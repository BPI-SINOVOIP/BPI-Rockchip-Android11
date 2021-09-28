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
package com.android.tv.guide;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyLong;

import com.android.tv.common.flags.impl.DefaultUiFlags;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.ChannelImpl;
import com.android.tv.data.GenreItems;
import com.android.tv.data.ProgramDataManager;
import com.android.tv.data.ProgramImpl;
import com.android.tv.data.api.Channel;
import com.android.tv.data.api.Program;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link ProgramTableAdapter}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestSingletonApp.class)
public class ProgramTableAdapterTest {

    @Mock private ProgramGuide mProgramGuide;
    @Mock private ChannelDataManager mChannelDataManager;
    @Mock private ProgramDataManager mProgramDataManager;
    private ProgramManager mProgramManager;

    //  Thursday, June 1, 2017 1:00:00 PM GMT-07:00
    private final long mTestStartTimeMs = 1496347200000L;
    // Thursday, June 1, 2017 8:00:00 PM GMT-07:00
    private final long mEightPM = 1496372400000L;
    private DefaultUiFlags mUiFlags;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        TestSingletonApp app = (TestSingletonApp) RuntimeEnvironment.application;
        app.fakeClock.setBootTimeMillis(mTestStartTimeMs + TimeUnit.HOURS.toMillis(-12));
        app.fakeClock.setCurrentTimeMillis(mTestStartTimeMs);
        mUiFlags = new DefaultUiFlags();
        mProgramManager =
                new ProgramManager(
                        app.getTvInputManagerHelper(),
                        mChannelDataManager,
                        mProgramDataManager,
                        null,
                        null);
    }

    @Test
    public void testOnTableEntryChanged() {
        Mockito.when(mProgramGuide.getProgramManager()).thenReturn(mProgramManager);
        Mockito.when(mProgramDataManager.getCurrentProgram(anyLong()))
                .thenAnswer(
                        invocation -> {
                            long id = (long) invocation.getArguments()[0];
                            return buildProgramForTesting(
                                    id, id, (int) id % GenreItems.getGenreCount());
                        });
        ProgramTableAdapter programTableAdapter =
                new ProgramTableAdapter(RuntimeEnvironment.application, mProgramGuide, mUiFlags);
        mProgramManager.setChannels(buildChannelForTesting(1, 2, 3));
        assertThat(mProgramManager.getChannelCount()).isEqualTo(3);

        // set genre ID to 1. Then channel 1 is in the filtered list but channel 2 is not.
        mProgramManager.resetChannelListWithGenre(1);
        assertThat(mProgramManager.getChannelCount()).isEqualTo(1);
        assertThat(mProgramManager.getChannelIndex(2)).isEqualTo(-1);

        // should be no exception when onTableEntryChanged() is called
        programTableAdapter.onTableEntryChanged(
                ProgramManager.createTableEntryForTest(
                        2,
                        mProgramDataManager.getCurrentProgram(2),
                        null,
                        mTestStartTimeMs,
                        mEightPM,
                        false));
    }

    private List<Channel> buildChannelForTesting(long... ids) {
        List<Channel> channels = new ArrayList<>();
        for (long id : ids) {
            channels.add(new ChannelImpl.Builder().setId(id).build());
        }
        return channels;
    }

    private Program buildProgramForTesting(long id, long channelId, int genreId) {
        return new ProgramImpl.Builder()
                .setId(id)
                .setChannelId(channelId)
                .setCanonicalGenres(GenreItems.getCanonicalGenre(genreId))
                .build();
    }
}
