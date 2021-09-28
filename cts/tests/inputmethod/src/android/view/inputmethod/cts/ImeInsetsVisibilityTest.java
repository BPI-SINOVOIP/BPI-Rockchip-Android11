/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static android.content.Intent.ACTION_CLOSE_SYSTEM_DIALOGS;
import static android.content.Intent.FLAG_RECEIVER_FOREGROUND;
import static android.view.WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeInvisible;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeVisible;

import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEventWithKeyValue;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.util.Pair;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.TestUtils;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.CtsTouchUtils;
import com.android.compatibility.common.util.PollingCheck;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ImeInsetsVisibilityTest extends EndToEndImeTestBase {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    private static final int NEW_KEYBOARD_HEIGHT = 400;

    private static final String TEST_MARKER_PREFIX =
            "android.view.inputmethod.cts.ImeInsetsVisibilityTest";

    private static String getTestMarker() {
        return TEST_MARKER_PREFIX + "/"  + SystemClock.elapsedRealtimeNanos();
    }

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    @Test
    public void testImeVisibilityWhenImeFocusableChildPopup() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final Pair<EditText, TestActivity> editTextTestActivityPair =
                    launchTestActivity(false, marker);
            final EditText editText = editTextTestActivityPair.first;
            final TestActivity activity = editTextTestActivityPair.second;

            notExpectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeInvisible(TIMEOUT);

            // Emulate tap event
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, editText);
            TestUtils.waitOnMainUntil(() -> editText.hasFocus(), TIMEOUT);
            WindowInsetsController controller = editText.getWindowInsetsController();

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            PollingCheck.check("Ime insets should be visible", TIMEOUT,
                    () -> editText.getRootWindowInsets().isVisible(WindowInsets.Type.ime()));
            expectImeVisible(TIMEOUT);

            final View[] childViewRoot = new View[1];
            TestUtils.runOnMainSync(() -> {
                childViewRoot[0] = addChildWindow(activity);
                childViewRoot[0].setVisibility(View.VISIBLE);
            });
            TestUtils.waitOnMainUntil(() -> childViewRoot[0] != null
                    && childViewRoot[0].getVisibility() == View.VISIBLE, TIMEOUT);

            PollingCheck.check("Ime insets should be visible", TIMEOUT,
                    () -> editText.getRootWindowInsets().isVisible(WindowInsets.Type.ime()));
            expectImeVisible(TIMEOUT);
        }
    }

    @AppModeFull(reason = "Instant apps cannot rely on ACTION_CLOSE_SYSTEM_DIALOGS")
    @Test
    public void testEditTextPositionAndPersistWhenAboveImeWindowShown() throws Exception {
        final InputMethodManager imm = InstrumentationRegistry.getInstrumentation().getContext()
                .getSystemService(InputMethodManager.class);

        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder().setInputViewHeight(NEW_KEYBOARD_HEIGHT))) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final Pair<EditText, TestActivity> editTextTestActivityPair =
                    launchTestActivity(true, marker);
            final EditText editText = editTextTestActivityPair.first;
            final TestActivity activity = editTextTestActivityPair.second;
            final WindowInsets[] insetsFromActivity = new WindowInsets[1];
            Point curEditPos = getLocationOnScreenForView(editText);

            TestUtils.runOnMainSync(() -> {
                activity.getWindow().getDecorView().setOnApplyWindowInsetsListener(
                        (v, insets) -> insetsFromActivity[0] = insets);
            });

            notExpectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeInvisible(TIMEOUT);

            // Emulate tap event to show soft-keyboard
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, editText);
            TestUtils.waitOnMainUntil(() -> editText.hasFocus(), TIMEOUT);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            expectImeVisible(TIMEOUT);

            Point lastEditTextPos = new Point(curEditPos);
            curEditPos = getLocationOnScreenForView(editText);
            assertTrue("Insets should visible and EditText position should be adjusted",
                    isInsetsVisible(insetsFromActivity[0], WindowInsets.Type.ime())
                            && curEditPos.y < lastEditTextPos.y);

            imm.showInputMethodPicker();
            TestUtils.waitOnMainUntil(() -> imm.isInputMethodPickerShown() && editText.isLaidOut(),
                    TIMEOUT, "InputMethod picker should be shown");
            lastEditTextPos = new Point(curEditPos);
            curEditPos = getLocationOnScreenForView(editText);

            assertTrue("Insets visibility & EditText position should persist when " +
                            "the above IME window shown",
                    isInsetsVisible(insetsFromActivity[0], WindowInsets.Type.ime())
                            && curEditPos.equals(lastEditTextPos));

            InstrumentationRegistry.getInstrumentation().getContext().sendBroadcast(
                    new Intent(ACTION_CLOSE_SYSTEM_DIALOGS).setFlags(FLAG_RECEIVER_FOREGROUND));
            TestUtils.waitOnMainUntil(() -> !imm.isInputMethodPickerShown(), TIMEOUT,
                    "InputMethod picker should be closed");
        }
    }

    private boolean isInsetsVisible(WindowInsets winInsets, int type) {
        if (winInsets == null) {
            return false;
        }
        return winInsets.isVisible(type);
    }

    private Point getLocationOnScreenForView(View view) {
        return TestUtils.getOnMainSync(() -> {
            final int[] tmpPos = new int[2];
            view.getLocationOnScreen(tmpPos);
            return new Point(tmpPos[0], tmpPos[1]);
        });
    }

    private Pair<EditText, TestActivity> launchTestActivity(boolean useDialogTheme,
            @NonNull String focusedMarker) {
        final AtomicReference<EditText> focusedEditTextRef = new AtomicReference<>();
        final AtomicReference<TestActivity> testActivityRef = new AtomicReference<>();

        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);
            layout.setGravity(Gravity.BOTTOM);
            if (useDialogTheme) {
                // Create a floating Dialog
                activity.setTheme(android.R.style.Theme_Material_Dialog);
                TextView textView = new TextView(activity);
                textView.setText("I'm a TextView");
                textView.setHeight(activity.getWindowManager().getMaximumWindowMetrics()
                        .getBounds().height() / 3);
                layout.addView(textView);
            }

            final EditText focusedEditText = new EditText(activity);
            focusedEditText.setHint("focused editText");
            focusedEditText.setPrivateImeOptions(focusedMarker);

            focusedEditTextRef.set(focusedEditText);
            testActivityRef.set(activity);

            layout.addView(focusedEditText);
            return layout;
        });
        return new Pair<>(focusedEditTextRef.get(), testActivityRef.get());
    }

    private View addChildWindow(Activity activity) {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final WindowManager wm = context.getSystemService(WindowManager.class);
        final WindowManager.LayoutParams attrs = new WindowManager.LayoutParams();
        attrs.token = activity.getWindow().getAttributes().token;
        attrs.type = TYPE_APPLICATION;
        attrs.width = 200;
        attrs.height = 200;
        attrs.format = PixelFormat.TRANSPARENT;
        attrs.flags = FLAG_NOT_FOCUSABLE | FLAG_ALT_FOCUSABLE_IM;
        attrs.setFitInsetsTypes(WindowInsets.Type.ime() | WindowInsets.Type.statusBars()
                | WindowInsets.Type.navigationBars());
        final View childViewRoot = new View(context);
        childViewRoot.setVisibility(View.GONE);
        wm.addView(childViewRoot, attrs);
        return childViewRoot;
    }
}
