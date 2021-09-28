/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.assist.testapp;

import android.assist.common.Utils;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

public class TextViewActivity extends BaseThirdPartyActivity {
    static final String TAG = "TextViewActivity";

    private TextView mTextView;
    private ScrollView mScrollView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "TextViewActivity created");
        setContentView(R.layout.text_view);
        mScrollView = findViewById(R.id.scroll_view);
        mTextView = findViewById(R.id.text_view);
        mTextView.setMovementMethod(new ScrollingMovementMethod());
    }

    @Override
    protected void onReceivedEventFromCaller(Bundle results, String action) {
        super.onReceivedEventFromCaller(results, action);

        int scrollX, scrollY;
        scrollX = results.getInt(Utils.SCROLL_X_POSITION, 0);
        scrollY = results.getInt(Utils.SCROLL_Y_POSITION, 0);
        if (action.equals(Utils.SCROLL_TEXTVIEW_ACTION)) {
            Log.i(TAG, "Scrolling textview to (" + scrollX + "," + scrollY + ")");
            if (scrollX < 0 || scrollY < 0) {
                // Scroll to bottom as negative positions are not possible.
                scrollX = mTextView.getWidth();
                scrollY = mTextView.getLayout().getLineTop(mTextView.getLineCount())
                        - mTextView.getHeight();
            }
            TextViewActivity.this.mTextView.scrollTo(scrollX, scrollY);
        } else if (action.equals(Utils.SCROLL_SCROLLVIEW_ACTION)) {
            Log.i(TAG, "Scrolling scrollview to (" + scrollX + "," + scrollY + ")");
            if (scrollX < 0 || scrollY < 0) {
                // Scroll to bottom
                TextViewActivity.this.mScrollView.fullScroll(View.FOCUS_DOWN);
                TextViewActivity.this.mScrollView.fullScroll(View.FOCUS_RIGHT);
            } else {
                TextViewActivity.this.mScrollView.scrollTo(scrollX, scrollY);
            }
        }
        Log.i(TAG, "the max height of this textview is: " + mTextView.getHeight());
        Log.i(TAG, "the max line count of this text view is: " + mTextView.getMaxLines());
    }
}
