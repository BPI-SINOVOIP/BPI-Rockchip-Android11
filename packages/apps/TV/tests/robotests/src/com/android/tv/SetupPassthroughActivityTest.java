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

package com.android.tv;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.ServiceInfo;
import android.media.tv.TvInputInfo;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.support.annotation.Nullable;

import androidx.test.core.app.ApplicationProvider;

import com.android.tv.common.CommonConstants;
import com.android.tv.common.dagger.ApplicationModule;
import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.common.flags.impl.SettableFlagsModule;
import com.android.tv.common.util.CommonUtils;
import com.android.tv.data.ChannelDataManager;
import com.android.tv.data.epg.EpgFetcher;
import com.android.tv.features.TvFeatures;
import com.android.tv.modules.TvSingletonsModule;
import com.android.tv.testing.FakeTvInputManager;
import com.android.tv.testing.FakeTvInputManagerHelper;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.tunerinputcontroller.BuiltInTunerManager;
import com.android.tv.util.AsyncDbTask.DbExecutor;
import com.android.tv.util.SetupUtils;
import com.android.tv.util.TvInputManagerHelper;

import com.google.android.tv.partner.support.EpgContract;
import com.google.common.base.Optional;
import com.android.tv.common.flags.proto.TypedFeatures.StringListParam;

import dagger.Component;
import dagger.Module;
import dagger.Provides;
import dagger.android.AndroidInjectionModule;
import dagger.android.AndroidInjector;
import dagger.android.DispatchingAndroidInjector;
import dagger.android.HasAndroidInjector;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import java.util.concurrent.Executor;

import javax.inject.Inject;
import javax.inject.Singleton;

/** Tests for {@link SetupPassthroughActivity}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = SetupPassthroughActivityTest.MyTestApp.class)
public class SetupPassthroughActivityTest {

    private static final int REQUEST_START_SETUP_ACTIVITY = 200;

    private MyTestApp testSingletonApp;

    private TvInputInfo testInput;

    @Before
    public void setup() {
        testInput = createMockInput("com.example/.Input");
        testSingletonApp = (MyTestApp) ApplicationProvider.getApplicationContext();
        testSingletonApp.flagsModule.legacyFlags =
                DefaultLegacyFlags.builder().enableQaFeatures(true).build();
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager();
    }

    @After
    public void after() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.resetForTests();
    }

    @Test
    public void create_emptyIntent() {
        SetupPassthroughActivity activity =
                Robolectric.buildActivity(SetupPassthroughActivity.class, new Intent())
                        .create()
                        .get();
        ShadowActivity.IntentForResult shadowIntent =
                shadowOf(activity).getNextStartedActivityForResult();
        // Since there is no inputs, the next activity should not be started.
        assertThat(shadowIntent).isNull();
        assertThat(activity.isFinishing()).isTrue();
    }

    @Test
    public void create_noInputs() {
        SetupPassthroughActivity activity = createSetupActivityFor("com.example/.Input");
        ShadowActivity.IntentForResult shadowIntent =
                shadowOf(activity).getNextStartedActivityForResult();
        // Since there is no inputs, the next activity should not be started.
        assertThat(shadowIntent).isNull();
        assertThat(activity.isFinishing()).isTrue();
    }

    @Test
    public void create_inputNotFound() {
        testSingletonApp.tvInputManagerHelper = new FakeTvInputManagerHelper(testSingletonApp);
        testSingletonApp.tvInputManagerHelper.start();
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        SetupPassthroughActivity activity = createSetupActivityFor("com.example/.Other");
        ShadowActivity.IntentForResult shadowIntent =
                shadowOf(activity).getNextStartedActivityForResult();
        // Since the input is not found, the next activity should not be started.
        assertThat(shadowIntent).isNull();
        assertThat(activity.isFinishing()).isTrue();
    }

    @Test
    public void create_validInput() {
        testSingletonApp.tvInputManagerHelper.start();
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());

        ShadowActivity.IntentForResult shadowIntent =
                shadowOf(activity).getNextStartedActivityForResult();
        assertThat(shadowIntent).isNotNull();
        assertThat(shadowIntent.options).isNull();
        assertThat(shadowIntent.intent.getExtras()).isNotNull();
        assertThat(shadowIntent.requestCode).isEqualTo(REQUEST_START_SETUP_ACTIVITY);
        assertThat(activity.isFinishing()).isFalse();
    }

    @Test
    public void create_trustedCallingPackage() {
        testSingletonApp.tvInputManagerHelper.start();
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);

        ActivityController<SetupPassthroughActivity> activityController =
                Robolectric.buildActivity(
                        SetupPassthroughActivity.class,
                        CommonUtils.createSetupIntent(new Intent(), testInput.getId()));
        SetupPassthroughActivity activity = activityController.get();
        ShadowActivity shadowActivity = shadowOf(activity);
        shadowActivity.setCallingActivity(
                new ComponentName(CommonConstants.BASE_PACKAGE, "com.example.MyClass"));
        activityController.create();

        ShadowActivity.IntentForResult shadowIntent =
                shadowActivity.getNextStartedActivityForResult();
        assertThat(shadowIntent).isNotNull();
        assertThat(shadowIntent.options).isNull();
        assertThat(shadowIntent.intent.getExtras()).isNotNull();
        assertThat(shadowIntent.requestCode).isEqualTo(REQUEST_START_SETUP_ACTIVITY);
        assertThat(activity.isFinishing()).isFalse();
    }

    @Test
    public void create_nonTrustedCallingPackage() {
        testSingletonApp.tvInputManagerHelper.start();
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);

        ActivityController<SetupPassthroughActivity> activityController =
                Robolectric.buildActivity(
                        SetupPassthroughActivity.class,
                        CommonUtils.createSetupIntent(new Intent(), testInput.getId()));
        SetupPassthroughActivity activity = activityController.get();
        ShadowActivity shadowActivity = shadowOf(activity);
        shadowActivity.setCallingActivity(
                new ComponentName("com.notTrusted", "com.notTrusted.MyClass"));
        activityController.create();

        ShadowActivity.IntentForResult shadowIntent =
                shadowActivity.getNextStartedActivityForResult();
        // Since the calling activity is not trusted, the next activity should not be started.
        assertThat(shadowIntent).isNull();
        assertThat(activity.isFinishing()).isTrue();
    }

    @Test
    public void onActivityResult_canceled() {
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());

        activity.onActivityResult(0, Activity.RESULT_CANCELED, null);
        assertThat(activity.isFinishing()).isTrue();
        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_CANCELED);
    }

    @Test
    public void onActivityResult_ok() {
        TestSetupUtils setupUtils = new TestSetupUtils(RuntimeEnvironment.application);
        testSingletonApp.setupUtils = setupUtils;
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());
        activity.onActivityResult(REQUEST_START_SETUP_ACTIVITY, Activity.RESULT_OK, null);

        assertThat(testSingletonApp.epgFetcher.fetchStarted).isFalse();
        assertThat(setupUtils.finishedId).isEqualTo("com.example/.Input");
        assertThat(activity.isFinishing()).isFalse();

        setupUtils.finishedRunnable.run();
        assertThat(activity.isFinishing()).isTrue();
        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_OK);
    }

    @Test
    public void onActivityResult_3rdPartyEpg_ok() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.enableForTest();
        TestSetupUtils setupUtils = new TestSetupUtils(RuntimeEnvironment.application);
        testSingletonApp.setupUtils = setupUtils;
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        testSingletonApp
                .getCloudEpgFlags()
                .setThirdPartyEpgInput(
                        StringListParam.newBuilder().addElement(testInput.getId()).build());
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());
        Intent data = new Intent();
        data.putExtra(EpgContract.EXTRA_USE_CLOUD_EPG, true);
        data.putExtra(TvInputInfo.EXTRA_INPUT_ID, testInput.getId());
        activity.onActivityResult(REQUEST_START_SETUP_ACTIVITY, Activity.RESULT_OK, data);

        assertThat(testSingletonApp.epgFetcher.fetchStarted).isTrue();
        assertThat(setupUtils.finishedId).isEqualTo("com.example/.Input");
        assertThat(activity.isFinishing()).isFalse();

        setupUtils.finishedRunnable.run();
        assertThat(activity.isFinishing()).isTrue();
        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_OK);
    }

    @Test
    public void onActivityResult_3rdPartyEpg_notWhiteListed() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.enableForTest();
        TestSetupUtils setupUtils = new TestSetupUtils(RuntimeEnvironment.application);
        testSingletonApp.setupUtils = setupUtils;
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());
        Intent data = new Intent();
        data.putExtra(EpgContract.EXTRA_USE_CLOUD_EPG, true);
        data.putExtra(TvInputInfo.EXTRA_INPUT_ID, testInput.getId());
        activity.onActivityResult(REQUEST_START_SETUP_ACTIVITY, Activity.RESULT_OK, data);

        assertThat(testSingletonApp.epgFetcher.fetchStarted).isFalse();
        assertThat(setupUtils.finishedId).isEqualTo("com.example/.Input");
        assertThat(activity.isFinishing()).isFalse();

        setupUtils.finishedRunnable.run();
        assertThat(activity.isFinishing()).isTrue();
        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_OK);
    }

    @Test
    public void onActivityResult_3rdPartyEpg_disabled() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.disableForTests();
        TestSetupUtils setupUtils = new TestSetupUtils(RuntimeEnvironment.application);
        testSingletonApp.setupUtils = setupUtils;
        testSingletonApp.tvInputManagerHelper.getFakeTvInputManager().add(testInput, -1);
        testSingletonApp
                .getCloudEpgFlags()
                .setThirdPartyEpgInput(
                        StringListParam.newBuilder().addElement(testInput.getId()).build());
        testSingletonApp.dbExecutor = new RoboExecutorService();
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());
        Intent data = new Intent();
        data.putExtra(EpgContract.EXTRA_USE_CLOUD_EPG, true);
        data.putExtra(TvInputInfo.EXTRA_INPUT_ID, testInput.getId());
        activity.onActivityResult(REQUEST_START_SETUP_ACTIVITY, Activity.RESULT_OK, data);

        assertThat(testSingletonApp.epgFetcher.fetchStarted).isFalse();
        assertThat(setupUtils.finishedId).isEqualTo("com.example/.Input");
        assertThat(activity.isFinishing()).isFalse();

        setupUtils.finishedRunnable.run();
        assertThat(activity.isFinishing()).isTrue();
        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_OK);
    }

    @Test
    public void onActivityResult_ok_tvInputInfo_null() {
        TestSetupUtils setupUtils = new TestSetupUtils(RuntimeEnvironment.application);
        testSingletonApp.setupUtils = setupUtils;
        FakeTvInputManager tvInputManager =
                testSingletonApp.tvInputManagerHelper.getFakeTvInputManager();
        SetupPassthroughActivity activity = createSetupActivityFor(testInput.getId());
        activity.onActivityResult(REQUEST_START_SETUP_ACTIVITY, Activity.RESULT_OK, null);

        assertThat(tvInputManager.getTvInputInfo(testInput.getId())).isEqualTo(null);
        assertThat(testSingletonApp.epgFetcher.fetchStarted).isFalse();
        assertThat(activity.isFinishing()).isTrue();

        assertThat(shadowOf(activity).getResultCode()).isEqualTo(Activity.RESULT_OK);
    }

    private SetupPassthroughActivity createSetupActivityFor(String inputId) {
        return Robolectric.buildActivity(
                        SetupPassthroughActivity.class,
                        CommonUtils.createSetupIntent(new Intent(), inputId))
                .create()
                .get();
    }

    private TvInputInfo createMockInput(String inputId) {
        TvInputInfo tvInputInfo = Mockito.mock(TvInputInfo.class);
        ServiceInfo serviceInfo = new ServiceInfo();
        ApplicationInfo applicationInfo = new ApplicationInfo();
        Mockito.when(tvInputInfo.getId()).thenReturn(inputId);
        serviceInfo.packageName = inputId.substring(0, inputId.indexOf('/'));
        serviceInfo.applicationInfo = applicationInfo;
        applicationInfo.flags = 0;
        Mockito.when(tvInputInfo.getServiceInfo()).thenReturn(serviceInfo);
        Mockito.when(tvInputInfo.loadLabel(ArgumentMatchers.any())).thenReturn("testLabel");
        if (VERSION.SDK_INT >= VERSION_CODES.N) {
            Mockito.when(tvInputInfo.loadCustomLabel(ArgumentMatchers.any()))
                    .thenReturn("testCustomLabel");
        }
        return tvInputInfo;
    }

    /**
     * Test SetupUtils.
     *
     * <p>SetupUtils has lots of DB and threading interactions, that make it hard to test. This
     * bypasses all of that.
     */
    private static class TestSetupUtils extends SetupUtils {
        public String finishedId;
        public Runnable finishedRunnable;

        private TestSetupUtils(Context context) {
            super(context, Optional.absent());
        }

        @Override
        public void onTvInputSetupFinished(String inputId, @Nullable Runnable postRunnable) {
            finishedId = inputId;
            finishedRunnable = postRunnable;
        }
    }

    /** Test app for {@link SetupPassthroughActivityTest} */
    public static class MyTestApp extends TestSingletonApp implements HasAndroidInjector {

        @Inject DispatchingAndroidInjector<Object> dispatchingAndroidInjector;

        @Override
        public void onCreate() {

            super.onCreate();
            // Inject afterwards so we can use objects created in super.
            // Note TestSingletonApp does not do injection so it is safe
            applicationInjector().inject(this);
        }

        @Override
        public AndroidInjector<Object> androidInjector() {
            return dispatchingAndroidInjector;
        }

        protected AndroidInjector<MyTestApp> applicationInjector() {

            return DaggerSetupPassthroughActivityTest_TestComponent.builder()
                    .applicationModule(new ApplicationModule(this))
                    .tvSingletonsModule(new TvSingletonsModule(this))
                    .testModule(new TestModule(this))
                    .settableFlagsModule(flagsModule)
                    .build();
        }
    }

    /** Dagger component for {@link SetupPassthroughActivityTest}. */
    @Singleton
    @Component(
            modules = {
                AndroidInjectionModule.class,
                TestModule.class,
            })
    interface TestComponent extends AndroidInjector<MyTestApp> {}

    @Module(
            includes = {
                SetupPassthroughActivity.Module.class,
                ApplicationModule.class,
                TvSingletonsModule.class,
                SettableFlagsModule.class,
            })
    /** Module for {@link MyTestApp} */
    static class TestModule {
        private final MyTestApp myTestApp;

        TestModule(MyTestApp test) {
            myTestApp = test;
        }

        @Provides
        Optional<BuiltInTunerManager> providesBuiltInTunerManager() {
            return Optional.absent();
        }

        @Provides
        TvInputManagerHelper providesTvInputManagerHelper() {
            return myTestApp.tvInputManagerHelper;
        }

        @Provides
        SetupUtils providesTestSetupUtils() {
            return myTestApp.setupUtils;
        }

        @Provides
        @DbExecutor
        Executor providesDbExecutor() {
            return myTestApp.dbExecutor;
        }

        @Provides
        ChannelDataManager providesChannelDataManager() {
            return myTestApp.getChannelDataManager();
        }

        @Provides
        EpgFetcher providesEpgFetcher() {
            return myTestApp.epgFetcher;
        }
    }
}
