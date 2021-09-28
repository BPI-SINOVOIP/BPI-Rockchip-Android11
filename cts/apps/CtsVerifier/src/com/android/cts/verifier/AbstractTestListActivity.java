/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cts.verifier;

import com.android.cts.verifier.TestListAdapter.TestListItem;
import android.app.ListActivity;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.widget.ListView;

/** {@link ListActivity} that displays a list of manual tests. */
public abstract class AbstractTestListActivity extends ListActivity {
    private static final int LAUNCH_TEST_REQUEST_CODE = 9001;
    //An invalid value which smaller than the edge of coordinate on the screen.
    private static final float DEFAULT_CLICKED_COORDINATE = -1;

    protected TestListAdapter mAdapter;
    // Start time of test case.
    protected long mStartTime;
    // End time of test case.
    protected long mEndTime;
    // X-axis of clicked coordinate when entering a test case.
    protected float mCoordinateX;
    // Y-axis of clicked coordinate when entering a test case.
    protected float mCoornidateY;
    // Whether test case was executed through automation.
    protected boolean mIsAutomated;

    protected void setTestListAdapter(TestListAdapter adapter) {
        mAdapter = adapter;
        setListAdapter(mAdapter);
        mAdapter.loadTestResults();
        setOnTouchListenerToListView();
    }

    private Intent getIntent(int position) {
        TestListItem item = mAdapter.getItem(position);
        return item.intent;
    }

    @Override
    protected final void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        handleActivityResult(requestCode, resultCode, data);
    }

    /** Override this in subclasses instead of onActivityResult */
    protected void handleActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case LAUNCH_TEST_REQUEST_CODE:
                handleLaunchTestResult(resultCode, data);
                break;

            default:
                throw new IllegalArgumentException("Unknown request code: " + requestCode);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if ((getResources().getConfiguration().uiMode & Configuration.UI_MODE_TYPE_MASK)
              == Configuration.UI_MODE_TYPE_TELEVISION) {
            getWindow().requestFeature(Window.FEATURE_OPTIONS_PANEL);
        }
        setContentView(R.layout.list_content);
    }

    protected void handleLaunchTestResult(int resultCode, Intent data) {
        if (resultCode == RESULT_OK) {
            // If subtest didn't set end time, set current time
            if (mEndTime == 0) {
                mEndTime = System.currentTimeMillis();
            }
            TestResult testResult = TestResult.fromActivityResult(resultCode, data);
            testResult.getHistoryCollection().add(
                testResult.getName(), mStartTime, mEndTime, mIsAutomated);
            mAdapter.setTestResult(testResult);
        }
        // Reset end time to avoid keeping same end time in retry.
        mEndTime = 0;
        // Reset mIsAutomated flag to false
        mIsAutomated = false;
        // Reset clicked coordinate.
        mCoordinateX = DEFAULT_CLICKED_COORDINATE;
        mCoornidateY = DEFAULT_CLICKED_COORDINATE;
    }

    /** Launch the activity when its {@link ListView} item is clicked. */
    @Override
    protected final void onListItemClick(ListView listView, View view, int position, long id) {
        super.onListItemClick(listView, view, position, id);
        mStartTime = System.currentTimeMillis();
        //Check whether the clicked coordinate is consistent with the center of the clicked Object.
        Rect rect = new Rect();
        view.getGlobalVisibleRect(rect);
        mIsAutomated = (mCoordinateX == rect.centerX()) && (mCoornidateY == rect.centerY());
        handleItemClick(listView, view, position, id);
    }

    /** Override this in subclasses instead of onListItemClick */
    protected void handleItemClick(ListView listView, View view, int position, long id) {
        Intent intent = getIntent(position);
        startActivityForResult(intent, LAUNCH_TEST_REQUEST_CODE);
    }

    /** Set OnTouchListener to ListView to get the clicked Coordinate*/
    protected void setOnTouchListenerToListView() {
        getListView().setOnTouchListener(null);
        getListView().setOnTouchListener(new View.OnTouchListener(){
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_UP) {
                    mCoordinateX = event.getRawX();
                    mCoornidateY = event.getRawY();
                } else {
                    // Reset clicked coordinate.
                    mCoordinateX = DEFAULT_CLICKED_COORDINATE;
                    mCoornidateY = DEFAULT_CLICKED_COORDINATE;
                }
                return false;
            }
        });
    }
}
