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

package android.systemui.cts;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

import android.annotation.MainThread;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

public class WindowInsetsActivity extends LightBarBaseActivity implements View.OnClickListener,
        View.OnApplyWindowInsetsListener {
    private static final int DISPLAY_CUTOUT_SLACK_DP = 20;

    private TextView mContent;
    private WindowInsets mContentWindowInsets;
    private WindowInsets mDecorViewWindowInsets;
    private Rect mDecorBound;
    private Rect mContentBound;

    private Consumer<Boolean> mInitialFinishCallBack;
    private int mClickCount;
    private Consumer<View> mClickConsumer;
    private final DisplayMetrics mDisplayMetrics = new DisplayMetrics();


    /**
     * To setup the Activity to get better diagnoise.
     * To setup WindowInsetPresenterDrawable that will show the boundary of the windowInset and
     * the touch track.
     */
    @Override
    protected void onCreate(Bundle bundle) {
        getWindow().requestFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(bundle);

        mContent = new TextView(this);
        mContent.setTextSize(10);
        mContent.setGravity(Gravity.CENTER);
        mContent.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        WindowInsetsPresenterDrawable presenterDrawable =
            new WindowInsetsPresenterDrawable(getWindow().getDecorView().getRootWindowInsets());
        mContent.setOnTouchListener(presenterDrawable);
        mContent.setBackground(presenterDrawable);
        mContent.setOnApplyWindowInsetsListener(this::onApplyWindowInsets);
        mContent.getRootView().setOnApplyWindowInsetsListener(this::onApplyWindowInsets);
        getWindow().getDecorView().setOnApplyWindowInsetsListener(this::onApplyWindowInsets);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        getWindow().getAttributes().layoutInDisplayCutoutMode
                = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setStatusBarColor(0x22ff0000);
        getWindow().setNavigationBarColor(0x22ff0000);
        mContent.setOnClickListener(this);
        mContent.requestApplyInsets();
        setContentView(mContent);

        mContentWindowInsets = getWindow().getDecorView().getRootWindowInsets();
        mInitialFinishCallBack = null;

        getDisplay().getRealMetrics(mDisplayMetrics);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mClickCount = 0;
    }

    private void showVisualBoundary(WindowInsets insets) {
        if (insets != null) {
            WindowInsetsPresenterDrawable presenterDrawable =
                    (WindowInsetsPresenterDrawable) mContent.getBackground();
            presenterDrawable.setWindowInsets(insets);
        }
    }

    /**
     * To get the WindowInsets that comes from onApplyWindowInsets(mContent, insets).
     */
    WindowInsets getContentViewWindowInsets() {
        showVisualBoundary(mContentWindowInsets);
        return mContentWindowInsets;
    }

    /**
     * To get the WindowInsets that comes from onApplyWindowInsets(DecorView, insets).
     */
    WindowInsets getDecorViewWindowInsets() {
        showVisualBoundary(mDecorViewWindowInsets);
        return mDecorViewWindowInsets;
    }


    /**
     * To catch the WindowInsets that passwd to the content view.
     * This WindowInset should have already consumed the SystemWindowInset.
     */
    @Override
    public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
        if (insets != null) {
            if (v == mContent) {
                mContentWindowInsets = new WindowInsets.Builder(insets).build();
            } else {
                mDecorViewWindowInsets = new WindowInsets.Builder(insets).build();
            }
            showInfoInTextView();
            showVisualBoundary(mDecorViewWindowInsets);
        }

        return insets;
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        mContent.post(() -> {
            mContentBound = getViewBound(mContent);
            mDecorBound = getViewBound(getWindow().getDecorView());
            showInfoInTextView();

            if (mInitialFinishCallBack != null) {
                mInitialFinishCallBack.accept(true);
            }
        });
    }

    /**
     * To present the WindowInsets information to mContent.
     * To show all of results of getSystemWindowInsets(), getMandatorySytemGestureInsets(),
     * getSystemGestureInsets(), getTappableElementsInsets() and the exclude rects
     *
     * @param rect the rectangle want to add or pass into to setSystemGestureExclusionRects
     */
    @MainThread
    public void setSystemGestureExclusion(Rect rect) {
        List<Rect> rects = new ArrayList<>();
        if (rect != null) {
            rects.add(rect);
        }
        getContentView().setSystemGestureExclusionRects(rects);
        showInfoInTextView();
    }

    private void showInfoInTextView() {
        StringBuilder sb = new StringBuilder();
        sb.append("exclude rect list = " + Arrays.deepToString(mContent
                .getSystemGestureExclusionRects().toArray())).append("\n");

        if (mDecorViewWindowInsets != null) {
            sb.append("onApplyWindowInsets mDecorViewWindowInsets = " + mDecorViewWindowInsets);
            sb.append("\n");
            sb.append("getSystemWindowInsets = " + mDecorViewWindowInsets.getSystemWindowInsets());
            sb.append("\n");
            sb.append("getSystemGestureInsets = "
                    + mDecorViewWindowInsets.getSystemGestureInsets()).append("\n");
            sb.append("getMandatorySystemGestureInsets = "
                    + mDecorViewWindowInsets.getMandatorySystemGestureInsets()).append("\n");
            sb.append("getTappableElementInsets = "
                    + mDecorViewWindowInsets.getTappableElementInsets()).append("\n");
            sb.append("decor boundary = ").append(mDecorBound).append("\n");
        }
        if (mContentWindowInsets != null) {
            sb.append("------------------------").append("\n");
            sb.append("onApplyWindowInsets mContentWindowInsets = " + mContentWindowInsets);
            sb.append("\n");
            sb.append("getSystemWindowInsets = " + mContentWindowInsets.getSystemWindowInsets());
            sb.append("\n");
            sb.append("getSystemGestureInsets = "
                    + mContentWindowInsets.getSystemGestureInsets()).append("\n");
            sb.append("getMandatorySystemGestureInsets = "
                    + mContentWindowInsets.getMandatorySystemGestureInsets()).append("\n");
            sb.append("getTappableElementInsets = "
                    + mContentWindowInsets.getTappableElementInsets()).append("\n");
            sb.append("content boundary = ").append(mContentBound).append("\n");
        }

        Display display = getDisplay();
        if (display != null) {
            sb.append("------------------------").append("\n");
            DisplayCutout displayCutout = display.getCutout();
            if (displayCutout != null) {
                sb.append("displayCutout = ").append(displayCutout.toString()).append("\n");
            } else {
                sb.append("Display cut out = null\n");
            }

            sb.append("real size = (").append(mDisplayMetrics.widthPixels).append(",")
                    .append(mDisplayMetrics.heightPixels).append(")\n");
        }


        mContent.setText(sb.toString());
    }

    @MainThread
    public View getContentView() {
        return mContent;
    }

    Rect getViewBound(View view) {
        int [] screenlocation = new int[2];
        view.getLocationOnScreen(screenlocation);
        return new Rect(screenlocation[0], screenlocation[1],
                screenlocation[0] + view.getWidth(),
                screenlocation[1] + view.getHeight());
    }

    /**
     * To count the draggable boundary that has consume the related insets.
     **/
    @MainThread
    public Rect getOperationArea(Insets insets, WindowInsets windowInsets) {
        int left = insets.left;
        int top = insets.top;
        int right = insets.right;
        int bottom = insets.bottom;

        final DisplayCutout cutout = windowInsets.getDisplayCutout();
        if (cutout != null) {
            int slack = (int) (DISPLAY_CUTOUT_SLACK_DP * mDisplayMetrics.density);
            if (cutout.getSafeInsetLeft() > 0) {
                left = Math.max(left, cutout.getSafeInsetLeft() + slack);
            }
            if (cutout.getSafeInsetTop() > 0) {
                top = Math.max(top, cutout.getSafeInsetTop() + slack);
            }
            if (cutout.getSafeInsetRight() > 0) {
                right = Math.max(right, cutout.getSafeInsetRight() + slack);
            }
            if (cutout.getSafeInsetBottom() > 0) {
                bottom = Math.max(bottom, cutout.getSafeInsetBottom() + slack);
            }
        }

        Rect windowBoundary = getViewBound(getContentView());
        Rect rect = new Rect(windowBoundary);
        rect.left += left;
        rect.top += top;
        rect.right -= right;
        rect.bottom -= bottom;

        return rect;
    }

    @MainThread
    public List<Point> getActionCancelPoints() {
        return ((WindowInsetsPresenterDrawable) mContent.getBackground()).getActionCancelPoints();
    }

    @MainThread
    public List<Point> getActionDownPoints() {
        return ((WindowInsetsPresenterDrawable) mContent.getBackground()).getActionDownPoints();
    }

    @MainThread
    public List<Point> getActionUpPoints() {
        return ((WindowInsetsPresenterDrawable) mContent.getBackground()).getActionUpPoints();
    }

    /**
     * To set the callback to notify the onClickListener is triggered.
     * @param clickConsumer trigger the callback after clicking view.
     */
    public void setOnClickConsumer(
            Consumer<View> clickConsumer) {
        mClickConsumer = clickConsumer;
    }

    /**
     * Because the view needs the focus to catch the ACTION_DOWN otherwise do nothing.
     * Only for the focus to receive the ACTION_DOWN, ACTION_MOVE, ACTION_UP and ACTION_CANCEL.
     **/
    @Override
    public void onClick(View v) {
        mClickCount++;
        if (mClickConsumer != null) {
            mClickConsumer.accept(v);
        }
    }

    public int getClickCount() {
        return mClickCount;
    }

    /**
     * To set the callback to notify the test with the initial finish.
     * @param initialFinishCallBack trigger the callback after initial finish.
     */
    public void setInitialFinishCallBack(
            Consumer<Boolean> initialFinishCallBack) {
        mInitialFinishCallBack = initialFinishCallBack;
    }
}
