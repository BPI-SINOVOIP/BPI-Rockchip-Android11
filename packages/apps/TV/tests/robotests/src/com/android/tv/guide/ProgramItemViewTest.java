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

import static com.google.android.libraries.testing.truth.TextViewSubject.assertThat;

import android.support.annotation.NonNull;
import android.view.LayoutInflater;

import com.android.tv.R;
import com.android.tv.common.flags.impl.SettableFlagsModule;
import com.android.tv.common.util.Clock;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.ProgramImpl;
import com.android.tv.dvr.DvrManager;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.guide.ProgramItemViewTest.TestApp;
import com.android.tv.guide.ProgramItemViewTest.TestModule.Contributes;
import com.android.tv.guide.ProgramManager.TableEntry;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.robo.RobotTestAppHelper;
import com.android.tv.testing.testdata.TestData;

import dagger.Component;
import dagger.Module;
import dagger.Provides;
import dagger.android.AndroidInjectionModule;
import dagger.android.AndroidInjector;
import dagger.android.ContributesAndroidInjector;
import dagger.android.DispatchingAndroidInjector;
import dagger.android.HasAndroidInjector;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.time.Duration;
import java.util.concurrent.TimeUnit;

import javax.inject.Inject;

/** Tests for {@link ProgramItemView}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = TestApp.class)
public class ProgramItemViewTest {

    /** TestApp for {@link ProgramItemViewTest} */
    public static class TestApp extends TestSingletonApp implements HasAndroidInjector {
        @Inject DispatchingAndroidInjector<Object> dispatchingAndroidInjector;

        @Override
        public void onCreate() {
            super.onCreate();
            DaggerProgramItemViewTest_TestAppComponent.builder()
                    .testModule(new TestModule(this))
                    .build()
                    .inject(this);
        }

        @Override
        public AndroidInjector<Object> androidInjector() {
            return dispatchingAndroidInjector;
        }
    }

    /** Component for {@link ProgramItemViewTest} */
    @Component(
            modules = {
                AndroidInjectionModule.class,
                TestModule.class,
            })
    interface TestAppComponent extends AndroidInjector<TestApp> {}

    /** Module for {@link ProgramItemViewTest} */
    @Module(includes = {Contributes.class, SettableFlagsModule.class})
    public static class TestModule {

        @Module()
        public abstract static class Contributes {
            @ContributesAndroidInjector
            abstract ProgramItemView contributesProgramItemView();
        }

        private final TestApp myTestApp;

        TestModule(TestApp test) {
            myTestApp = test;
        }

        @Provides
        ChannelDataManager providesChannelDataManager() {
            return myTestApp.getChannelDataManager();
        }

        @Provides
        Clock provideClock() {
            return myTestApp.getClock();
        }
    }

    //  Thursday, June 1, 2017 1:00:00 PM GMT-07:00
    private final long testStartTimeMs = 1496347200000L;

    // Thursday, June 1, 2017 8:00:00 PM GMT-07:00
    private final long eightPM = 1496372400000L;
    private final ProgramImpl baseProgram =
            new ProgramImpl.Builder()
                    .setChannelId(1)
                    .setStartTimeUtcMillis(eightPM)
                    .setEndTimeUtcMillis(eightPM + Duration.ofHours(1).toMillis())
                    .build();

    private ProgramItemView mPprogramItemView;

    @Mock DvrManager dvrManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        TestSingletonApp app = (TestSingletonApp) RuntimeEnvironment.application;
        app.dvrManager = dvrManager;
        app.fakeClock.setBootTimeMillis(testStartTimeMs + TimeUnit.HOURS.toMillis(-12));
        app.fakeClock.setCurrentTimeMillis(testStartTimeMs);
        RobotTestAppHelper.loadTestData(app, TestData.DEFAULT_10_CHANNELS);
        mPprogramItemView =
                (ProgramItemView)
                        LayoutInflater.from(RuntimeEnvironment.application)
                                .inflate(R.layout.program_guide_table_item, null);
        GuideUtils.setWidthPerHour(100);
    }

    @Test
    public void initialState() {
        assertThat(mPprogramItemView).hasEmptyText();
    }

    @Test
    public void setValue_noProgram() {
        TableEntry tableEntry = create30MinuteTableEntryFor(null, null, false);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("a gap");
        assertThat(mPprogramItemView).hasContentDescription("1 a gap 8:00 – 9:00 PM");
    }

    @Test
    public void setValue_programNoTitle() {
        ProgramImpl program = new ProgramImpl.Builder(baseProgram).build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, null, false);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("No information");
        assertThat(mPprogramItemView).hasContentDescription("1 No information 8:00 – 9:00 PM");
    }

    @Test
    public void setValue_programTitle() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram).setTitle("A good program").build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, null, false);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program");
        assertThat(mPprogramItemView).hasContentDescription("1 A good program 8:00 – 9:00 PM");
    }

    @Test
    public void setValue_programDescriptionBlocked() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram)
                        .setTitle("A good program")
                        .setDescription("Naughty")
                        .build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, null, true);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program");
        assertThat(mPprogramItemView)
                .hasContentDescription("1 A good program 8:00 – 9:00 PM This program is blocked");
    }

    @Test
    public void setValue_programEpisode() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram)
                        .setTitle("A good program")
                        .setEpisodeTitle("The one with an episode")
                        .build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, null, false);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program\n\u200DThe one with an episode");
        assertThat(mPprogramItemView)
                .hasContentDescription("1 A good program 8:00 – 9:00 PM The one with an episode");
    }

    @Test
    public void setValue_programEpisodeAndDescrition() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram)
                        .setTitle("A good program")
                        .setEpisodeTitle("The one with an episode")
                        .setDescription("Jack and Jill go up a hill")
                        .build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, null, false);
        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program\n\u200DThe one with an episode");
        assertThat(mPprogramItemView)
                .hasContentDescription(
                        "1 A good program 8:00 – 9:00 PM The one with an episode"
                                + " Jack and Jill go up a hill");
    }

    @Test
    public void setValue_scheduledConflict() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram).setTitle("A good program").build();
        ScheduledRecording scheduledRecording =
                ScheduledRecording.builder("input1", program).build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, scheduledRecording, false);
        Mockito.when(dvrManager.isConflicting(scheduledRecording)).thenReturn(true);

        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program");
        assertThat(mPprogramItemView)
                .hasContentDescription("1 A good program 8:00 – 9:00 PM Recording conflict");
    }

    @Test
    public void setValue_scheduled() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram).setTitle("A good program").build();
        ScheduledRecording scheduledRecording =
                ScheduledRecording.builder("input1", program)
                        .setState(ScheduledRecording.STATE_RECORDING_NOT_STARTED)
                        .build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, scheduledRecording, false);
        Mockito.when(dvrManager.isConflicting(scheduledRecording)).thenReturn(false);

        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program");
        assertThat(mPprogramItemView)
                .hasContentDescription("1 A good program 8:00 – 9:00 PM Recording scheduled");
    }

    @Test
    public void setValue_recordingInProgress() {
        ProgramImpl program =
                new ProgramImpl.Builder(baseProgram).setTitle("A good program").build();
        ScheduledRecording scheduledRecording =
                ScheduledRecording.builder("input1", program)
                        .setState(ScheduledRecording.STATE_RECORDING_IN_PROGRESS)
                        .build();
        TableEntry tableEntry = create30MinuteTableEntryFor(program, scheduledRecording, false);
        Mockito.when(dvrManager.isConflicting(scheduledRecording)).thenReturn(false);

        mPprogramItemView.setValues(null, tableEntry, 0, 0, 0, "a gap");
        assertThat(mPprogramItemView).hasText("A good program");
        assertThat(mPprogramItemView)
                .hasContentDescription("1 A good program 8:00 – 9:00 PM Recording");
    }

    @NonNull
    private TableEntry create30MinuteTableEntryFor(
            ProgramImpl program, ScheduledRecording scheduledRecording, boolean isBlocked) {
        return ProgramManager.createTableEntryForTest(
                1,
                program,
                scheduledRecording,
                eightPM,
                eightPM + Duration.ofHours(1).toMillis(),
                isBlocked);
    }
}
