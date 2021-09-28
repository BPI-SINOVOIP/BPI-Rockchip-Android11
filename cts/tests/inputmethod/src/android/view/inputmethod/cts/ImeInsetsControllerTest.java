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

import static android.view.WindowInsets.CONSUMED;
import static android.view.WindowInsets.Type.ime;

import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.graphics.Point;
import android.os.Process;
import android.os.SystemClock;
import android.util.Pair;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimationControlListener;
import android.view.WindowInsetsAnimationController;
import android.view.WindowInsetsController;
import android.view.WindowInsetsController.OnControllableInsetsChangedListener;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Rule;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@MediumTest
public class ImeInsetsControllerTest extends EndToEndImeTestBase {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);

    private static final String TEST_MARKER = "android.view.inputmethod.cts.InsetsControllerTest";

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    public Pair<EditText, Window> launchTestActivity() {
        final AtomicReference<EditText> editTextRef = new AtomicReference<>();
        final AtomicReference<Window> windowRef = new AtomicReference<>();
        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final EditText editText = new EditText(activity);
            editText.setPrivateImeOptions(TEST_MARKER);
            editText.setHint("editText");
            editText.requestFocus();
            editTextRef.set(editText);

            windowRef.set(activity.getWindow());

            layout.addView(editText);
            return layout;
        });
        return new Pair<>(editTextRef.get(), windowRef.get());
    }

    private static final int INITIAL_KEYBOARD_HEIGHT = 200;
    private static final int NEW_KEYBOARD_HEIGHT = 400;

    @Test
    public void testChangeSizeWhileControlling() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder()
                        .setInputViewHeight(INITIAL_KEYBOARD_HEIGHT)
                        .setDrawsBehindNavBar(true))) {
            final ImeEventStream stream = imeSession.openEventStream();

            Pair<EditText, Window> launchResult = launchTestActivity();
            final EditText editText = launchResult.first;
            final View decorView = launchResult.second.getDecorView();

            WindowInsets[] lastInsets = new WindowInsets[1];

            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
                launchResult.second.setDecorFitsSystemWindows(false);
                editText.setOnApplyWindowInsetsListener((v, insets) -> {
                    lastInsets[0] = insets;
                    return CONSUMED;
                });
            });

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            CountDownLatch controlLatch = new CountDownLatch(1);
            WindowInsetsAnimationController[] animController =
                    new WindowInsetsAnimationController[1];
            boolean[] cancelled = new boolean[1];

            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
                editText.getWindowInsetsController().addOnControllableInsetsChangedListener(
                        new OnControllableInsetsChangedListener() {

                            boolean mControlling = false;

                            @Override
                            public void onControllableInsetsChanged(
                                    @NonNull WindowInsetsController controller, int typeMask) {
                                if ((typeMask & ime()) != 0 && !mControlling) {
                                    mControlling = true;
                                    controller.controlWindowInsetsAnimation(ime(), -1, null, null,
                                            createControlListener(animController, controlLatch,
                                                    cancelled));
                                }
                            }
                        });
            });

            controlLatch.await(5, TimeUnit.SECONDS);
            assertEquals(0, controlLatch.getCount());
            assertEquals(getExpectedBottomInsets(INITIAL_KEYBOARD_HEIGHT, decorView),
                         lastInsets[0].getInsets(ime()).bottom);
            assertEquals(animController[0].getShownStateInsets(), lastInsets[0].getInsets(ime()));

            // Change keyboard height, but make sure the insets don't change until the controlling
            // is done.
            expectCommand(stream, imeSession.callSetHeight(NEW_KEYBOARD_HEIGHT), TIMEOUT);

            SystemClock.sleep(500);

            // Make sure keyboard height hasn't changed yet.
            assertEquals(getExpectedBottomInsets(INITIAL_KEYBOARD_HEIGHT, decorView),
                         lastInsets[0].getInsets(ime()).bottom);

            // Wait until new insets dispatch
            CountDownLatch insetsLatch = new CountDownLatch(1);
            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
                editText.setOnApplyWindowInsetsListener((v, insets) -> {
                    insetsLatch.countDown();
                    lastInsets[0] = insets;
                    return CONSUMED;
                });
                animController[0].finish(true);
            });
            insetsLatch.await(5, TimeUnit.SECONDS);
            assertEquals(0, insetsLatch.getCount());

            // Verify new height
            assertEquals(getExpectedBottomInsets(NEW_KEYBOARD_HEIGHT, decorView),
                         lastInsets[0].getInsets(ime()).bottom);

            assertFalse(cancelled[0]);
        }
    }

    private WindowInsetsAnimationControlListener createControlListener(
            WindowInsetsAnimationController[] outController, CountDownLatch controlLatch,
            boolean[] outCancelled) {
        return new WindowInsetsAnimationControlListener() {
            @Override
            public void onReady(
                    @NonNull WindowInsetsAnimationController controller,
                    int types) {
                outController[0] = controller;
                controlLatch.countDown();
            }

            @Override
            public void onFinished(
                    @NonNull WindowInsetsAnimationController controller) {
            }

            @Override
            public void onCancelled(
                    @Nullable WindowInsetsAnimationController controller) {
                outCancelled[0] = true;
            }
        };
    }

    private int getDisplayHeight(View view) {
        final Point size = new Point();
        view.getDisplay().getRealSize(size);
        return size.y;
    }

    private int getBottomOfWindow(View decorView) {
        int viewPos[] = new int[2];
        decorView.getLocationOnScreen(viewPos);
        return decorView.getHeight() + viewPos[1];
    }

    private int getExpectedBottomInsets(int keyboardHeight, View decorView) {
        return Math.max(
                0,
                keyboardHeight
                        - Math.max(0, getDisplayHeight(decorView) - getBottomOfWindow(decorView)));
    }
}
