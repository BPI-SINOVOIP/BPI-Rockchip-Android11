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

package com.android.cts.verifier.qstiles;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

// This class is based on the same class from notifications
public abstract class InteractiveVerifierActivity extends PassFailButtons.Activity
        implements Runnable {
    private static final String TAG = "InteractiveVerifier";
    private static final String STATE = "state";
    private static final String STATUS = "status";
    protected static final String TILE_PATH = "com.android.cts.verifier/" +
            "com.android.cts.verifier.qstiles.MockTileService";
    protected static final ComponentName TILE_NAME = ComponentName.unflattenFromString(TILE_PATH);
    protected static final int SETUP = 0;
    protected static final int READY = 1;
    protected static final int RETEST = 2;
    protected static final int PASS = 3;
    protected static final int FAIL = 4;
    protected static final int WAIT_FOR_USER = 5;
    protected static final int RETEST_AFTER_LONG_DELAY = 6;
    protected static final int READY_AFTER_LONG_DELAY = 7;

    protected InteractiveTestCase mCurrentTest;
    protected PackageManager mPackageManager;
    protected Context mContext;
    protected Runnable mRunner;
    protected View mHandler;

    private LayoutInflater mInflater;
    private ViewGroup mItemList;
    private List<InteractiveTestCase> mTestList;
    private Iterator<InteractiveTestCase> mTestOrder;

    protected boolean setTileState(boolean enabled) {
        int state = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
        mPackageManager.setComponentEnabledSetting(TILE_NAME, state, PackageManager.DONT_KILL_APP);
        return mPackageManager.getComponentEnabledSetting(TILE_NAME) == state;
    }

    protected abstract class InteractiveTestCase {
        protected boolean mUserVerified;
        protected int status;
        private View view;
        protected long delayTime = 3000;

        protected abstract View inflate(ViewGroup parent);

        View getView(ViewGroup parent) {
            if (view == null) {
                view = inflate(parent);
            }
            return view;
        }

        /** @return true if the test should re-run when the test activity starts. */
        boolean autoStart() {
            return false;
        }

        /** Set status to {@link #READY} to proceed, or {@link #SETUP} to try again. */
        protected void setUp() {
            status = READY;
            next();
        }

        /** Set status to {@link #PASS} or @{link #FAIL} to proceed, or {@link #READY} to retry. */
        protected void test() {
            status = FAIL;
            next();
        }

        /** Do not modify status. */
        protected void tearDown() {
            next();
        }

        protected void setFailed() {
            status = FAIL;
            logFail();
        }

        protected void setFailed(String message) {
            status = FAIL;
            logFail(message);
        }

        protected void logFail() {
            logFail(null);
        }

        protected void logFail(String message) {
            logWithStack("failed " + this.getClass().getSimpleName() +
                    ((message == null) ? "" : ": " + message));
        }

        protected void logFail(String message, Throwable e) {
            Log.e(TAG, "failed " + this.getClass().getSimpleName() +
                    ((message == null) ? "" : ": " + message), e);
        }

    }

    protected abstract int getTitleResource();

    protected abstract int getInstructionsResource();

    protected void onCreate(Bundle savedState) {
        super.onCreate(savedState);
        int savedStateIndex = (savedState == null) ? 0 : savedState.getInt(STATE, 0);
        int savedStatus = (savedState == null) ? SETUP : savedState.getInt(STATUS, SETUP);
        Log.i(TAG, "restored state(" + savedStateIndex + "}, status(" + savedStatus + ")");
        mContext = this;
        mRunner = this;
        mPackageManager = getPackageManager();
        mInflater = getLayoutInflater();
        View view = mInflater.inflate(R.layout.tiles_main, null);
        mItemList = (ViewGroup) view.findViewById(R.id.tiles_test_items);
        mHandler = mItemList;
        mTestList = new ArrayList<>();
        mTestList.addAll(createTestItems());
        for (InteractiveTestCase test : mTestList) {
            mItemList.addView(test.getView(mItemList));
        }
        mTestOrder = mTestList.iterator();
        for (int i = 0; i < savedStateIndex; i++) {
            mCurrentTest = mTestOrder.next();
            mCurrentTest.status = PASS;
        }
        mCurrentTest = mTestOrder.next();
        mCurrentTest.status = savedStatus;

        setContentView(view);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        setInfoResources(getTitleResource(), getInstructionsResource(), -1);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        final int stateIndex = mTestList.indexOf(mCurrentTest);
        outState.putInt(STATE, stateIndex);
        final int status = mCurrentTest == null ? SETUP : mCurrentTest.status;
        outState.putInt(STATUS, status);
        Log.i(TAG, "saved state(" + stateIndex + "}, status(" + status + ")");
    }

    @Override
    protected void onResume() {
        super.onResume();
        //To avoid NPE during onResume,before start to iterate next test order
        if (mCurrentTest != null && mCurrentTest.autoStart()) {
            mCurrentTest.status = READY;
        }
        // Makes sure that the tile is there on resume
        setTileState(true);
        next();
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Makes sure that the tile is removed when test is not running
        setTileState(false);
    }

    // Interface Utilities

    protected void markItem(InteractiveTestCase test) {
        if (test == null) {
            return;
        }
        View item = test.view;
        ImageView status = (ImageView) item.findViewById(R.id.tiles_status);
        View buttonPass = item.findViewById(R.id.tiles_action_pass);
        View buttonFail = item.findViewById(R.id.tiles_action_fail);
        switch (test.status) {
            case WAIT_FOR_USER:
                status.setImageResource(R.drawable.fs_warning);
                break;

            case SETUP:
            case READY:
            case RETEST:
                status.setImageResource(R.drawable.fs_clock);
                buttonPass.setEnabled(true);
                buttonPass.setClickable(true);
                buttonFail.setEnabled(true);
                buttonFail.setClickable(true);
                break;

            case FAIL:
                status.setImageResource(R.drawable.fs_error);
                buttonFail.setClickable(false);
                buttonFail.setEnabled(false);
                buttonPass.setClickable(false);
                buttonPass.setEnabled(false);
                break;

            case PASS:
                status.setImageResource(R.drawable.fs_good);
                buttonFail.setClickable(false);
                buttonFail.setEnabled(false);
                buttonPass.setClickable(false);
                buttonPass.setEnabled(false);
                break;

        }
        status.invalidate();
    }


    protected View createUserPassFail(ViewGroup parent, int messageId,
            Object... messageFormatArgs) {
        View item = mInflater.inflate(R.layout.tiles_item, parent, false);
        TextView instructions = (TextView) item.findViewById(R.id.tiles_instructions);
        instructions.setText(getString(messageId, messageFormatArgs));
        return item;
    }

    protected View createAutoItem(ViewGroup parent, int stringId) {
        View item = mInflater.inflate(R.layout.tiles_item, parent, false);
        TextView instructions = (TextView) item.findViewById(R.id.tiles_instructions);
        instructions.setText(stringId);
        View buttonPass = item.findViewById(R.id.tiles_action_pass);
        View buttonFail = item.findViewById(R.id.tiles_action_fail);
        buttonPass.setVisibility(View.GONE);
        buttonFail.setVisibility(View.GONE);
        return item;
    }

    // Test management

    abstract protected List<InteractiveTestCase> createTestItems();

    public void run() {
        if (mCurrentTest == null) {
            return;
        }
        markItem(mCurrentTest);
        switch (mCurrentTest.status) {
            case SETUP:
                Log.i(TAG, "running setup for: " + mCurrentTest.getClass().getSimpleName());
                mCurrentTest.setUp();
                if (mCurrentTest.status == READY_AFTER_LONG_DELAY) {
                    delay(mCurrentTest.delayTime);
                } else {
                    delay();
                }
                break;

            case WAIT_FOR_USER:
                Log.i(TAG, "waiting for user: " + mCurrentTest.getClass().getSimpleName());
                break;

            case READY_AFTER_LONG_DELAY:
            case RETEST_AFTER_LONG_DELAY:
            case READY:
            case RETEST:
                Log.i(TAG, "running test for: " + mCurrentTest.getClass().getSimpleName());
                try {
                    mCurrentTest.test();
                    if (mCurrentTest.status == RETEST_AFTER_LONG_DELAY) {
                        delay(mCurrentTest.delayTime);
                    } else {
                        delay();
                    }
                } catch (Throwable t) {
                    mCurrentTest.status = FAIL;
                    markItem(mCurrentTest);
                    Log.e(TAG, "FAIL: " + mCurrentTest.getClass().getSimpleName(), t);
                    mCurrentTest.tearDown();
                    mCurrentTest = null;
                    delay();
                }

                break;

            case FAIL:
                Log.i(TAG, "FAIL: " + mCurrentTest.getClass().getSimpleName());
                mCurrentTest.tearDown();
                mCurrentTest = null;
                delay();
                break;

            case PASS:
                Log.i(TAG, "pass for: " + mCurrentTest.getClass().getSimpleName());
                mCurrentTest.tearDown();
                if (mTestOrder.hasNext()) {
                    mCurrentTest = mTestOrder.next();
                    Log.i(TAG, "next test is: " + mCurrentTest.getClass().getSimpleName());
                    next();
                } else {
                    Log.i(TAG, "no more tests");
                    mCurrentTest = null;
                    getPassButton().setEnabled(true);
                }
                break;
        }
        markItem(mCurrentTest);
    }

    /**
     * Return to the state machine to progress through the tests.
     */
    protected void next() {
        mHandler.removeCallbacks(mRunner);
        mHandler.post(mRunner);
    }

    /**
     * Wait for things to settle before returning to the state machine.
     */
    protected void delay() {
        delay(3000);
    }

    protected void sleep(long time) {
        try {
            Thread.sleep(time);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    /**
     * Wait for some time.
     */
    protected void delay(long waitTime) {
        mHandler.removeCallbacks(mRunner);
        mHandler.postDelayed(mRunner, waitTime);
    }

    // UI callbacks

    public void actionPressed(View v) {
        if (mCurrentTest != null) {
            switch (v.getId()) {
                case R.id.tiles_action_pass:
                    mCurrentTest.status = PASS;
                    mCurrentTest.mUserVerified = true;
                    next();
                    break;
                case R.id.tiles_action_fail:
                    mCurrentTest.status = FAIL;
                    mCurrentTest.mUserVerified = true;
                    next();
                    break;
                default:
                    break;
            }
        }
    }

    // Utilities

    protected void logWithStack(String message) {
        Throwable stackTrace = new Throwable();
        stackTrace.fillInStackTrace();
        Log.e(TAG, message, stackTrace);
    }

}
