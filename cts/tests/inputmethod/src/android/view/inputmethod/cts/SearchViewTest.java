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

package android.view.inputmethod.cts;

import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeInvisible;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeVisible;
import static android.view.inputmethod.cts.util.TestUtils.runOnMainSync;

import static com.android.cts.mockime.ImeEventStreamTestUtils.EventFilterMode.CHECK_EXIT_EVENT_ONLY;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import android.os.Process;
import android.text.InputType;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.widget.BaseAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SearchView;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsTouchUtils;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class SearchViewTest extends EndToEndImeTestBase {
    static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    static final long NOT_EXPECT_TIMEOUT = TimeUnit.SECONDS.toMillis(2);

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    public SearchView launchTestActivity(boolean requestFocus) {
        final AtomicReference<SearchView> searchViewRef = new AtomicReference<>();
        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final EditText initialFocusedEditText = new EditText(activity);
            initialFocusedEditText.setHint("initialFocusedTextView");

            final SearchView searchView = new SearchView(activity);
            searchViewRef.set(searchView);

            searchView.setQueryHint("hint");
            searchView.setIconifiedByDefault(false);
            searchView.setInputType(InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
            searchView.setImeOptions(EditorInfo.IME_ACTION_DONE);
            if (requestFocus) {
                searchView.requestFocus();
            }

            layout.addView(initialFocusedEditText);
            layout.addView(searchView);
            return layout;
        });
        return searchViewRef.get();
    }

    private SearchView launchTestActivityWithListView(boolean requestFocus) {
        final AtomicReference<SearchView> searchViewRef = new AtomicReference<>();
        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final ListView listView = new ListView(activity);
            layout.addView(listView);

            final SearchView searchView = new SearchView(activity);
            searchViewRef.set(searchView);
            listView.setAdapter(new SingleItemAdapter(searchView));

            searchView.setQueryHint("hint");
            searchView.setIconifiedByDefault(false);
            searchView.setInputType(InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
            searchView.setImeOptions(EditorInfo.IME_ACTION_DONE);
            if (requestFocus) {
                searchView.requestFocus();
            }

            return layout;
        });
        return searchViewRef.get();
    }

    @Test
    public void testTapThenSetQuery() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final SearchView searchView = launchTestActivity(false /* requestFocus */);

            // Emulate tap event on SearchView
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, searchView);

            // Expect input to bind since EditText is focused.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            // Wait until "showSoftInput" gets called with a real InputConnection
            expectEvent(stream, event ->
                    "showSoftInput".equals(event.getEventName())
                            && !event.getExitState().hasDummyInputConnection(),
                    CHECK_EXIT_EVENT_ONLY, TIMEOUT);

            expectImeVisible(TIMEOUT);

            // Make sure that "setQuery" triggers "hideSoftInput" in the IME side.
            runOnMainSync(() -> searchView.setQuery("test", true /* submit */));
            expectEvent(stream, event -> "hideSoftInput".equals(event.getEventName()), TIMEOUT);

            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testShowImeWithSearchViewFocus() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final SearchView searchView = launchTestActivity(true /* requestFocus */);

            // Expect input to bind since checkInputConnectionProxy() returns true.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            runOnMainSync(() -> searchView.getContext().getSystemService(InputMethodManager.class)
                    .showSoftInput(searchView, 0));

            // Wait until "showSoftInput" gets called on searchView's inner editor
            // (SearchAutoComplete) with real InputConnection.
            expectEvent(stream, event ->
                    "showSoftInput".equals(event.getEventName())
                            && !event.getExitState().hasDummyInputConnection(),
                    CHECK_EXIT_EVENT_ONLY, TIMEOUT);

            expectImeVisible(TIMEOUT);
        }
    }

    @Test
    public void testShowImeWhenSearchViewFocusInListView() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final SearchView searchView = launchTestActivityWithListView(true /* requestFocus */);

            // Emulate tap event on SearchView
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, searchView);

            // Expect input to bind since EditText is focused.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            // Wait until "showSoftInput" gets called with a real InputConnection
            expectEvent(stream, event ->
                            "showSoftInput".equals(event.getEventName())
                                    && !event.getExitState().hasDummyInputConnection(),
                    CHECK_EXIT_EVENT_ONLY, TIMEOUT);

            expectImeVisible(TIMEOUT);

            notExpectEvent(stream, event -> "hideSoftInput".equals(event.getEventName()),
                    NOT_EXPECT_TIMEOUT);
        }
    }

    static final class SingleItemAdapter extends BaseAdapter {
        private final SearchView mSearchView;

        SingleItemAdapter(SearchView searchView) {
            mSearchView = searchView;
        }

        @Override
        public int getCount() {
            return 1;
        }

        @Override
        public Object getItem(int i) {
            return mSearchView;
        }

        @Override
        public long getItemId(int i) {
            return 0;
        }

        @Override
        public View getView(int i, View view, ViewGroup viewGroup) {
            return mSearchView;
        }
    }
}
