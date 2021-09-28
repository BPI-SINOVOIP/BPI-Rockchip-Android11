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
 * limitations under the License.
 */

package com.android.tv.search;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.fail;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.SearchManager;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.database.Cursor;
import android.net.Uri;

import com.android.tv.common.flags.impl.SettableFlagsModule;
import com.android.tv.data.ProgramImpl;
import com.android.tv.data.api.Program;
import com.android.tv.perf.PerformanceMonitor;
import com.android.tv.perf.stub.StubPerformanceMonitor;
import com.android.tv.search.LocalSearchProvider.SearchResult;
import com.android.tv.search.LocalSearchProviderTest.TestAppComponent.TestAppModule;
import com.android.tv.testing.TestSingletonApp;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.robo.ContentProviders;

import dagger.Component;
import dagger.Module;
import dagger.Provides;
import dagger.android.AndroidInjectionModule;
import dagger.android.AndroidInjector;
import dagger.android.DispatchingAndroidInjector;
import dagger.android.HasAndroidInjector;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;
import javax.inject.Singleton;

/** Unit test for {@link LocalSearchProvider}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK, application = LocalSearchProviderTest.TestApp.class)
public class LocalSearchProviderTest {

    /** Test app for {@link LocalSearchProviderTest} */
    public static class TestApp extends TestSingletonApp implements HasAndroidInjector {
        @Inject DispatchingAndroidInjector<Object> mDispatchingAndroidProvider;

        @Override
        public void onCreate() {
            super.onCreate();
            DaggerLocalSearchProviderTest_TestAppComponent.builder()
                    .settableFlagsModule(flagsModule)
                    .build()
                    .inject(this);
        }

        @Override
        public AndroidInjector<Object> androidInjector() {
            return mDispatchingAndroidProvider;
        }
    }

    @Component(
            modules = {
                AndroidInjectionModule.class,
                SettableFlagsModule.class,
                LocalSearchProvider.Module.class,
                TestAppModule.class
            })
    @Singleton
    interface TestAppComponent extends AndroidInjector<TestApp> {
        @Module
        abstract static class TestAppModule {
            @Provides
            @Singleton
            static PerformanceMonitor providePerformanceMonitor() {
                return new StubPerformanceMonitor();
            }
        }
    }

    private final Program mProgram1 =
            new ProgramImpl.Builder()
                    .setTitle("Dummy program")
                    .setDescription("Dummy program season 2")
                    .setPosterArtUri("FakeUri")
                    .setStartTimeUtcMillis(1516674000000L)
                    .setEndTimeUtcMillis(1516677000000L)
                    .setChannelId(7)
                    .setVideoWidth(1080)
                    .setVideoHeight(960)
                    .build();

    private final String mAuthority = "com.android.tv.search";
    private final String mKeyword = "mKeyword";
    private final Uri mBaseSearchUri =
            Uri.parse(
                    "content://"
                            + mAuthority
                            + "/"
                            + SearchManager.SUGGEST_URI_PATH_QUERY
                            + "/"
                            + mKeyword);

    private final Uri mWrongSearchUri =
            Uri.parse("content://" + mAuthority + "/wrong_path/" + mKeyword);

    private LocalSearchProvider mProvider;
    private ContentResolver mContentResolver;

    @Mock private SearchInterface mMockSearchInterface;
    private final FakeSearchInterface mFakeSearchInterface = new FakeSearchInterface();

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mProvider = ContentProviders.register(LocalSearchProvider.class, mAuthority);
        mContentResolver = RuntimeEnvironment.application.getContentResolver();
    }

    @Test
    public void testQuery_normalUri() {
        verifyQueryWithArguments(null, null);
        verifyQueryWithArguments(1, null);
        verifyQueryWithArguments(null, 1);
        verifyQueryWithArguments(1, 1);
    }

    @Test
    public void testQuery_invalidUri() {
        try (Cursor c = mContentResolver.query(mWrongSearchUri, null, null, null, null)) {
            fail("Query with invalid URI should fail.");
        } catch (IllegalArgumentException e) {
            // Success.
        }
    }

    @Test
    public void testQuery_invalidLimit() {
        verifyQueryWithArguments(-1, null);
    }

    @Test
    public void testQuery_invalidAction() {
        verifyQueryWithArguments(null, SearchInterface.ACTION_TYPE_START - 1);
        verifyQueryWithArguments(null, SearchInterface.ACTION_TYPE_END + 1);
    }

    private void verifyQueryWithArguments(Integer limit, Integer action) {
        mProvider.setSearchInterface(mMockSearchInterface);
        Uri uri = mBaseSearchUri;
        if (limit != null || action != null) {
            Uri.Builder builder = uri.buildUpon();
            if (limit != null) {
                builder.appendQueryParameter(
                        SearchManager.SUGGEST_PARAMETER_LIMIT, limit.toString());
            }
            if (action != null) {
                builder.appendQueryParameter(
                        LocalSearchProvider.SUGGEST_PARAMETER_ACTION, action.toString());
            }
            uri = builder.build();
        }
        try (Cursor c = mContentResolver.query(uri, null, null, null, null)) {
            // Do nothing.
        }
        int expectedLimit =
                limit == null || limit <= 0 ? LocalSearchProvider.DEFAULT_SEARCH_LIMIT : limit;
        int expectedAction =
                (action == null
                                || action < SearchInterface.ACTION_TYPE_START
                                || action > SearchInterface.ACTION_TYPE_END)
                        ? LocalSearchProvider.DEFAULT_SEARCH_ACTION
                        : action;
        verify(mMockSearchInterface).search(mKeyword, expectedLimit, expectedAction);
        reset(mMockSearchInterface);
    }

    @Test
    public void testGetType() {
        assertThat(mProvider.getType(mBaseSearchUri)).isEqualTo(SearchManager.SUGGEST_MIME_TYPE);
    }

    @Test
    public void query_empty() {
        mProvider.setSearchInterface(mFakeSearchInterface);
        Cursor cursor = mContentResolver.query(mBaseSearchUri, null, null, null, null);
        assertThat(cursor.moveToNext()).isFalse();
    }

    @Test
    public void query_program1() {
        mProvider.setSearchInterface(mFakeSearchInterface);
        mFakeSearchInterface.mPrograms.add(mProgram1);
        Uri uri =
                Uri.parse(
                        "content://"
                                + mAuthority
                                + "/"
                                + SearchManager.SUGGEST_URI_PATH_QUERY
                                + "/"
                                + "Dummy");
        Cursor cursor = mContentResolver.query(uri, null, null, null, null);
        assertThat(fromCursor(cursor)).containsExactly(FakeSearchInterface.fromProgram(mProgram1));
    }

    private List<SearchResult> fromCursor(Cursor cursor) {
        List<SearchResult> results = new ArrayList<>();
        while (cursor.moveToNext()) {
            SearchResult.Builder result = SearchResult.builder();
            int i = 0;
            result.setTitle(cursor.getString(i++));
            result.setDescription(cursor.getString(i++));
            result.setImageUri(cursor.getString(i++));
            result.setIntentAction(cursor.getString(i++));
            String intentData = cursor.getString(i++);
            result.setIntentData(intentData);
            result.setIntentExtraData(cursor.getString(i++));
            result.setContentType(cursor.getString(i++));
            result.setIsLive(cursor.getString(i++).equals("1"));
            result.setVideoWidth(Integer.valueOf(cursor.getString(i++)));
            result.setVideoHeight(Integer.valueOf(cursor.getString(i++)));
            result.setDuration(Long.valueOf(cursor.getString(i++)));
            result.setProgressPercentage(Integer.valueOf(cursor.getString(i)));
            result.setChannelId(ContentUris.parseId(Uri.parse(intentData)));
            results.add(result.build());
        }
        return results;
    }
}
