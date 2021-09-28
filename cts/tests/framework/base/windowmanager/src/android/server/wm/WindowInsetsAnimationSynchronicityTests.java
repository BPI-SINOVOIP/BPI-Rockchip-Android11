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

package android.server.wm;

import static android.server.wm.ActivityManagerTestBase.executeShellCommand;
import static android.server.wm.WindowInsetsAnimationUtils.requestControlThenTransitionToVisibility;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.WindowInsets.Type.ime;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Insets;
import android.graphics.Paint;
import android.graphics.Rect;
import android.inputmethodservice.InputMethodService;
import android.os.Bundle;
import android.platform.test.annotations.LargeTest;
import android.provider.Settings;
import android.server.wm.WindowInsetsAnimationControllerTests.LimitedErrorCollector;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;
import android.view.WindowInsetsAnimation.Callback;
import android.view.WindowInsetsController;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.Rule;
import org.junit.Test;

import java.util.List;
import java.util.Locale;
import java.util.function.Supplier;

@LargeTest
public class WindowInsetsAnimationSynchronicityTests {
    private static final int APP_COLOR = 0xff01fe10; // green
    private static final int BEHIND_IME_COLOR = 0xfffeef00; // yellow
    private static final int IME_COLOR = 0xfffe01fd; // pink

    @Rule
    public LimitedErrorCollector mErrorCollector = new LimitedErrorCollector();

    @Rule
    public ActivityTestRule<TestActivity> mActivityRule = new ActivityTestRule<>(
            TestActivity.class, false, false);

    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

    @Test
    public void testShowAndHide_renderSynchronouslyBetweenImeWindowAndAppContent() throws Throwable {
        runTest(false /* useControlApi */);
    }

    @Test
    public void testControl_rendersSynchronouslyBetweenImeWindowAndAppContent() throws Throwable {
        runTest(true /* useControlApi */);
    }

    private void runTest(boolean useControlApi) throws Exception {
        try (ImeSession imeSession = new ImeSession(SimpleIme.getName(mContext))) {
            TestActivity activity = mActivityRule.launchActivity(null);
            activity.setUseControlApi(useControlApi);
            PollingCheck.waitFor(activity::hasWindowFocus);
            activity.setEvaluator(() -> {
                // This runs from time to time on the UI thread.
                Bitmap screenshot = getInstrumentation().getUiAutomation().takeScreenshot();
                final int center = screenshot.getWidth() / 2;
                int imePositionApp = lowestPixelWithColor(APP_COLOR, 1, screenshot);
                int contentBottomMiddle = lowestPixelWithColor(APP_COLOR, center, screenshot);
                int behindImeBottomMiddle =
                        lowestPixelWithColor(BEHIND_IME_COLOR, center, screenshot);
                int imePositionIme = Math.max(contentBottomMiddle, behindImeBottomMiddle);
                if (imePositionApp != imePositionIme) {
                    mErrorCollector.addError(new AssertionError(String.format(Locale.US,
                            "IME is positioned at %d (max of %d, %d),"
                                    + " app thinks it is positioned at %d",
                            imePositionIme, contentBottomMiddle, behindImeBottomMiddle,
                            imePositionApp)));
                }
            });
            Thread.sleep(2000);
        }
    }

    private static int lowestPixelWithColor(int color, int x, Bitmap bitmap) {
        int[] pixels = new int[bitmap.getHeight()];
        bitmap.getPixels(pixels, 0, 1, x, 0, 1, bitmap.getHeight());
        for (int y = pixels.length - 1; y >= 0; y--) {
            if (pixels[y] == color) {
                return y;
            }
        }
        return -1;
    }

    public static class TestActivity extends Activity implements
            WindowInsetsController.OnControllableInsetsChangedListener {

        private TestView mTestView;
        private EditText mEditText;
        private Runnable mEvaluator;
        private boolean mUseControlApi;

        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().requestFeature(Window.FEATURE_NO_TITLE);
            getWindow().setDecorFitsSystemWindows(false);
            getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_HIDDEN);
            mTestView = new TestView(this);
            mEditText = new EditText(this);
            mEditText.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);
            mTestView.addView(mEditText);
            mTestView.mEvaluator = () -> {
                if (mEvaluator != null) {
                    mEvaluator.run();
                }
            };
            mEditText.requestFocus();
            setContentView(mTestView);
            mEditText.getWindowInsetsController().addOnControllableInsetsChangedListener(this);
        }

        void setEvaluator(Runnable evaluator) {
            mEvaluator = evaluator;
        }

        void setUseControlApi(boolean useControlApi) {
            mUseControlApi = useControlApi;
        }

        @Override
        public void onControllableInsetsChanged(@NonNull WindowInsetsController controller,
                int typeMask) {
            if ((typeMask & ime()) != 0) {
                mEditText.getWindowInsetsController().removeOnControllableInsetsChangedListener(
                        this);
                showIme();
            }
        }

        private void showIme() {
            if (mUseControlApi) {
                requestControlThenTransitionToVisibility(mTestView.getWindowInsetsController(),
                        ime(), true);
            } else {
                mTestView.getWindowInsetsController().show(ime());
            }
        }

        private void hideIme() {
            if (mUseControlApi) {
                requestControlThenTransitionToVisibility(mTestView.getWindowInsetsController(),
                        ime(), false);
            } else {
                mTestView.getWindowInsetsController().hide(ime());
            }
        }

        private static class TestView extends FrameLayout {
            private WindowInsets mLayoutInsets;
            private WindowInsets mAnimationInsets;
            private final Rect mTmpRect = new Rect();
            private final Paint mContentPaint = new Paint();

            private final Callback mInsetsCallback = new Callback(Callback.DISPATCH_MODE_STOP) {
                @NonNull
                @Override
                public WindowInsets onProgress(@NonNull WindowInsets insets,
                        @NonNull List<WindowInsetsAnimation> runningAnimations) {
                    if (runningAnimations.stream().anyMatch(TestView::isImeAnimation)) {
                        mAnimationInsets = insets;
                        invalidate();
                    }
                    return WindowInsets.CONSUMED;
                }

                @Override
                public void onEnd(@NonNull WindowInsetsAnimation animation) {
                    if (isImeAnimation(animation)) {
                        mAnimationInsets = null;
                        post(() -> {
                            if (mLayoutInsets.isVisible(ime())) {
                                ((TestActivity) getContext()).hideIme();
                            } else {
                                ((TestActivity) getContext()).showIme();
                            }
                        });

                    }
                }
            };
            private final Runnable mRunEvaluator;
            private Runnable mEvaluator;

            TestView(Context context) {
                super(context);
                setWindowInsetsAnimationCallback(mInsetsCallback);
                mContentPaint.setColor(APP_COLOR);
                mContentPaint.setStyle(Paint.Style.FILL);
                setWillNotDraw(false);
                mRunEvaluator = () -> {
                    if (mEvaluator != null) {
                        mEvaluator.run();
                    }
                };
            }

            @Override
            public WindowInsets onApplyWindowInsets(WindowInsets insets) {
                mLayoutInsets = insets;
                return WindowInsets.CONSUMED;
            }

            private WindowInsets getEffectiveInsets() {
                return mAnimationInsets != null ? mAnimationInsets : mLayoutInsets;
            }

            @Override
            protected void onDraw(Canvas canvas) {
                canvas.drawColor(BEHIND_IME_COLOR);
                mTmpRect.set(0, 0, getWidth(), getHeight());
                Insets insets = getEffectiveInsets().getInsets(ime());
                insetRect(mTmpRect, insets);
                canvas.drawRect(mTmpRect, mContentPaint);
                removeCallbacks(mRunEvaluator);
                post(mRunEvaluator);
            }

            private static boolean isImeAnimation(WindowInsetsAnimation animation) {
                return (animation.getTypeMask() & ime()) != 0;
            }

            private static void insetRect(Rect rect, Insets insets) {
                rect.left += insets.left;
                rect.top += insets.top;
                rect.right -= insets.right;
                rect.bottom -= insets.bottom;
            }
        }
    }

    private static class ImeSession implements AutoCloseable {

        private static final long TIMEOUT = 2000;
        private final ComponentName mImeName;
        private Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

        ImeSession(ComponentName ime) throws Exception {
            mImeName = ime;
            executeShellCommand("ime reset");
            executeShellCommand("ime enable " + ime.flattenToShortString());
            executeShellCommand("ime set " + ime.flattenToShortString());
            PollingCheck.check("Make sure that MockIME becomes available", TIMEOUT,
                    () -> ime.equals(getCurrentInputMethodId()));
        }

        @Override
        public void close() throws Exception {
            executeShellCommand("ime reset");
            PollingCheck.check("Make sure that MockIME becomes unavailable", TIMEOUT, () ->
                    mContext.getSystemService(InputMethodManager.class)
                            .getEnabledInputMethodList()
                            .stream()
                            .noneMatch(info -> mImeName.equals(info.getComponent())));
        }

        @Nullable
        private ComponentName getCurrentInputMethodId() {
            // TODO: Replace this with IMM#getCurrentInputMethodIdForTesting()
            return ComponentName.unflattenFromString(
                    Settings.Secure.getString(mContext.getContentResolver(),
                    Settings.Secure.DEFAULT_INPUT_METHOD));
        }
    }

    public static class SimpleIme extends InputMethodService {

        public static final int HEIGHT_DP = 200;
        public static final int SIDE_PADDING_DP = 50;

        @Override
        public View onCreateInputView() {
            final ViewGroup view = new FrameLayout(this);
            final View inner = new View(this);
            final float density = getResources().getDisplayMetrics().density;
            final int height = (int) (HEIGHT_DP * density);
            final int sidePadding = (int) (SIDE_PADDING_DP * density);
            view.setPadding(sidePadding, 0, sidePadding, 0);
            view.addView(inner, new FrameLayout.LayoutParams(MATCH_PARENT,
                    height));
            inner.setBackgroundColor(IME_COLOR);
            return view;
        }

        static ComponentName getName(Context context) {
            return new ComponentName(context, SimpleIme.class);
        }
    }
}
