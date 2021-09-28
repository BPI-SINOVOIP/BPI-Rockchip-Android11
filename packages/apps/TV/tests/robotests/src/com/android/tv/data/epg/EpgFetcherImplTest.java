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
 * limitations under the License
 */

package com.android.tv.data.epg;

/** Tests for {@link EpgFetcher}. */
import static com.google.common.truth.Truth.assertThat;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.sqlite.SQLiteDatabase;
import android.media.tv.TvContract;

import androidx.tvprovider.media.tv.Channel;

import com.android.tv.common.CommonPreferences;
import com.android.tv.common.buildtype.HasBuildType.BuildType;
import com.android.tv.common.flags.impl.DefaultBackendKnobsFlags;
import com.android.tv.common.flags.impl.SettableFlagsModule;
import com.android.tv.common.util.PostalCodeUtils;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.features.TvFeatures;
import com.android.tv.perf.PerformanceMonitor;
import com.android.tv.perf.stub.StubPerformanceMonitor;
import com.android.tv.testing.DbTestingUtils;
import com.android.tv.testing.EpgTestData;
import com.android.tv.testing.FakeEpgReader;
import com.android.tv.testing.FakeTvInputManagerHelper;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.fakes.FakeClock;
import com.android.tv.testing.fakes.FakeTvProvider;
import com.android.tv.testing.robo.ContentProviders;

import com.google.android.tv.livechannels.epg.provider.EpgContentProvider;
import com.google.android.tv.partner.support.EpgContract;
import com.google.common.collect.ImmutableList;
import com.android.tv.common.flags.proto.TypedFeatures.StringListParam;

import dagger.Component;
import dagger.Module;
import dagger.android.AndroidInjectionModule;
import dagger.android.AndroidInjector;
import dagger.android.DispatchingAndroidInjector;
import dagger.android.HasAndroidInjector;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

import javax.inject.Inject;

/** Tests for {@link EpgFetcherImpl}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = EpgFetcherImplTest.TestApp.class)
public class EpgFetcherImplTest {

    /** TestApp for {@link EpgFetcherImplTest} */
    public static class TestApp extends TestSingletonApp implements HasAndroidInjector {
        @Inject DispatchingAndroidInjector<Object> dispatchingAndroidInjector;

        @Override
        public void onCreate() {
            super.onCreate();
            DaggerEpgFetcherImplTest_TestAppComponent.builder()
                    .settableFlagsModule(flagsModule)
                    .build()
                    .inject(this);
        }

        @Override
        public AndroidInjector<Object> androidInjector() {
            return dispatchingAndroidInjector;
        }
    }

    /** Component for {@link EpgFetcherImplTest} */
    @Component(
            modules = {
                AndroidInjectionModule.class,
                TestModule.class,
                EpgContentProvider.Module.class
            })
    interface TestAppComponent extends AndroidInjector<TestApp> {}

    /** Module for {@link EpgFetcherImplTest} */
    @Module(includes = {SettableFlagsModule.class})
    public static class TestModule {}

    private static final String[] PROGRAM_COLUMNS = {
        TvContract.Programs.COLUMN_CHANNEL_ID,
        TvContract.Programs.COLUMN_TITLE,
        TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS,
        TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS
    };

    private static final String[] CHANNEL_COLUMNS = {
        TvContract.Channels.COLUMN_DISPLAY_NAME,
        TvContract.Channels.COLUMN_DISPLAY_NUMBER,
        TvContract.Channels.COLUMN_NETWORK_AFFILIATION
    };

    private FakeClock mFakeClock;
    private EpgFetcherImpl mEpgFetcher;
    private ChannelDataManager mChannelDataManager;
    private FakeEpgReader mEpgReader;
    private PerformanceMonitor mPerformanceMonitor = new StubPerformanceMonitor();
    private ContentResolver mContentResolver;
    private FakeTvProvider mTvProvider;
    private EpgContentProvider mEpgProvider;
    private EpgContentProvider.EpgDatabaseHelper mDatabaseHelper;
    private TestApp mTestApp;

    @Before
    public void setup() {

        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.enableForTest();
        mTestApp = (TestApp) RuntimeEnvironment.application;
        Shadows.shadowOf(RuntimeEnvironment.application)
                .grantPermissions("com.android.providers.tv.permission.ACCESS_ALL_EPG_DATA");
        mDatabaseHelper = new EpgContentProvider.EpgDatabaseHelper(RuntimeEnvironment.application);
        CommonPreferences.initialize(RuntimeEnvironment.application);
        PostalCodeUtils.setLastPostalCode(RuntimeEnvironment.application, "90210");
        EpgFetchHelper.setLastLineupId(RuntimeEnvironment.application, "test90210");
        mTvProvider = ContentProviders.register(FakeTvProvider.class, TvContract.AUTHORITY);
        mEpgProvider = ContentProviders.register(EpgContentProvider.class, EpgContract.AUTHORITY);
        mEpgProvider.setCallingPackage_("com.google.android.tv");
        mFakeClock = FakeClock.createWithCurrentTime();
        FakeTvInputManagerHelper fakeTvInputManagerHelper =
                new FakeTvInputManagerHelper(RuntimeEnvironment.application);
        mContentResolver = RuntimeEnvironment.application.getContentResolver();
        mChannelDataManager =
                new ChannelDataManager(
                        RuntimeEnvironment.application,
                        fakeTvInputManagerHelper,
                        new RoboExecutorService(),
                        mContentResolver);
        fakeTvInputManagerHelper.start();
        mChannelDataManager.start();
        mEpgReader = new FakeEpgReader(mFakeClock);
        mEpgFetcher =
                new EpgFetcherImpl(
                        RuntimeEnvironment.application,
                        new EpgInputWhiteList(
                                mTestApp.flagsModule.cloudEpgFlags,
                                mTestApp.flagsModule.legacyFlags),
                        mChannelDataManager,
                        mEpgReader,
                        mPerformanceMonitor,
                        mFakeClock,
                        new DefaultBackendKnobsFlags(),
                        BuildType.NO_JNI_TEST);
        EpgTestData.DATA_90210.loadData(mFakeClock, mEpgReader); // This also sets fake clock
        EpgFetchHelper.setLastEpgUpdatedTimestamp(
                RuntimeEnvironment.application,
                mFakeClock.currentTimeMillis() - TimeUnit.DAYS.toMillis(1));
    }

    @After
    public void after() {
        mChannelDataManager.stop();
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.resetForTests();
    }

    @Test
    public void fetchImmediately_nochannels() throws ExecutionException, InterruptedException {
        EpgFetcherImpl.FetchAsyncTask fetcherTask = mEpgFetcher.createFetchTask(null, null);
        fetcherTask.execute();

        assertThat(fetcherTask.get()).isEqualTo(EpgFetcherImpl.REASON_NO_BUILT_IN_CHANNELS);
        List<List<String>> rows =
                DbTestingUtils.toList(
                        mContentResolver.query(
                                TvContract.Programs.CONTENT_URI,
                                PROGRAM_COLUMNS,
                                null,
                                null,
                                null));
        assertThat(rows).isEmpty();
    }

    @Test
    public void fetchImmediately_testChannel() throws ExecutionException, InterruptedException {
        // The channels must be in the app package.
        // For this test the package is com.android.tv.data.epg
        insertTestChannels(
                "com.android.tv.data.epg/.tuner.TunerTvInputService", EpgTestData.CHANNEL_10);
        EpgFetcherImpl.FetchAsyncTask fetcherTask = mEpgFetcher.createFetchTask(null, null);
        fetcherTask.execute();

        assertThat(fetcherTask.get()).isNull();
        List<List<String>> rows =
                DbTestingUtils.toList(
                        mContentResolver.query(
                                TvContract.Programs.CONTENT_URI,
                                PROGRAM_COLUMNS,
                                null,
                                null,
                                null));
        assertThat(rows)
                .containsExactly(
                        ImmutableList.of("1", "Program 1", "1496358000000", "1496359800000"));
    }

    @Test
    public void fetchImmediately_epgChannel() throws ExecutionException, InterruptedException {
        mTestApp.flagsModule.cloudEpgFlags.setThirdPartyEpgInput(
                StringListParam.newBuilder().addElement("com.example/.Input").build());
        insertTestChannels("com.example/.Input", EpgTestData.CHANNEL_10, EpgTestData.CHANNEL_11);
        createTestEpgInput();
        EpgFetcherImpl.FetchAsyncTask fetcherTask = mEpgFetcher.createFetchTask(null, null);
        fetcherTask.execute();

        assertThat(fetcherTask.get()).isNull();
        List<List<String>> rows =
                DbTestingUtils.toList(
                        mContentResolver.query(
                                TvContract.Programs.CONTENT_URI,
                                PROGRAM_COLUMNS,
                                null,
                                null,
                                null));
        assertThat(rows)
                .containsExactly(
                        ImmutableList.of("1", "Program 1", "1496358000000", "1496359800000"),
                        ImmutableList.of("2", "Program 2", "1496359800000", "1496361600000"));
    }

    @Test
    public void testUpdateNetworkAffiliation() throws ExecutionException, InterruptedException {
        if (!TvFeatures.STORE_NETWORK_AFFILIATION.isEnabled(RuntimeEnvironment.application)) {
            return;
        }
        // set network affiliation to null so that it can be updated later
        Channel channel =
                new Channel.Builder(EpgTestData.CHANNEL_10).setNetworkAffiliation(null).build();
        // The channels must be in the app package.
        // For this test the package is com.android.tv.data.epg
        insertTestChannels("com.android.tv.data.epg/.tuner.TunerTvInputService", channel);

        List<List<String>> rows =
                DbTestingUtils.toList(
                        mContentResolver.query(
                                TvContract.Channels.CONTENT_URI,
                                CHANNEL_COLUMNS,
                                null,
                                null,
                                null));
        assertThat(rows).containsExactly(ImmutableList.of("Channel TEN", "10", "null"));
        EpgFetcherImpl.FetchAsyncTask fetcherTask = mEpgFetcher.createFetchTask(null, null);
        fetcherTask.execute();

        assertThat(fetcherTask.get()).isNull();
        rows =
                DbTestingUtils.toList(
                        mContentResolver.query(
                                TvContract.Channels.CONTENT_URI,
                                CHANNEL_COLUMNS,
                                null,
                                null,
                                null));
        // network affiliation should be updated
        assertThat(rows)
                .containsExactly(
                        ImmutableList.of("Channel TEN", "10", "Channel 10 Network Affiliation"));
    }

    protected void insertTestChannels(String inputId, Channel... channels) {

        for (Channel channel : channels) {
            ContentValues values =
                    new Channel.Builder(channel).setInputId(inputId).build().toContentValues();
            String packageName = inputId.substring(0, inputId.indexOf('/'));
            mTvProvider.setCallingPackage(packageName);
            mContentResolver.insert(TvContract.Channels.CONTENT_URI, values);
            mTvProvider.setCallingPackage("com.android.tv");
        }
    }

    private void createTestEpgInput() {
        // Use the database helper so we can set the package name.
        SQLiteDatabase db = mDatabaseHelper.getWritableDatabase();
        ContentValues values = new ContentValues();
        values.put(EpgContract.EpgInputs.COLUMN_ID, "1");
        values.put(EpgContract.EpgInputs.COLUMN_PACKAGE_NAME, "com.example");
        values.put(EpgContract.EpgInputs.COLUMN_INPUT_ID, "com.example/.Input");
        values.put(EpgContract.EpgInputs.COLUMN_LINEUP_ID, "lineup1");
        long rowId = db.insert("epg_input", null, values);
        assertThat(rowId).isEqualTo(1);
    }
}
