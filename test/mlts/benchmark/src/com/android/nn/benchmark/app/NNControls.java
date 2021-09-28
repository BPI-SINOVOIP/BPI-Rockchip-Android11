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

package com.android.nn.benchmark.app;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.android.nn.benchmark.core.BenchmarkResult;
import com.android.nn.benchmark.core.TestModels;
import com.android.nn.benchmark.util.TestExternalStorageActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Random;

public class NNControls extends Activity {
    private static final String TAG = NNControls.class.getSimpleName();

    private ListView mTestListView;
    private TextView mResultView;

    private ArrayAdapter<String> mTestListAdapter;
    private ArrayList<String> mTestList = new ArrayList<String>();

    private boolean mSettings[] = {false, false, false};
    private static final int SETTING_LONG_RUN = 0;
    private static final int SETTING_PAUSE = 1;
    private static final int SETTING_DISABLE_NNAPI = 2;

    private float mResults[];
    private String mInfo[];

    private static int DOGFOOD_MODELS_PER_RUN = 20;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu items for use in the action bar
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_activity_actions, menu);

        return super.onCreateOptionsMenu(menu);
    }

    void init() {
        TestExternalStorageActivity.testWriteExternalStorage(this, true);

        for (TestModels.TestModelEntry testModel : TestModels.modelsList()) {
            mTestList.add(testModel.toString());
        }

        mTestListView = findViewById(R.id.test_list);
        mTestListAdapter = new ArrayAdapter(this,
                android.R.layout.simple_list_item_activated_1,
                mTestList);

        mTestListView.setAdapter(mTestListAdapter);
        mTestListView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
        mTestListAdapter.notifyDataSetChanged();

        mResultView = findViewById(R.id.results);
        mResultView.setMovementMethod(new ScrollingMovementMethod());
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.controls);
        init();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    Intent makeBasicLaunchIntent() {
        Intent intent = new Intent(this, NNBenchmark.class);
        intent.putExtra(NNBenchmark.EXTRA_ENABLE_LONG, mSettings[SETTING_LONG_RUN]);
        intent.putExtra(NNBenchmark.EXTRA_ENABLE_PAUSE, mSettings[SETTING_PAUSE]);
        intent.putExtra(NNBenchmark.EXTRA_DISABLE_NNAPI, mSettings[SETTING_DISABLE_NNAPI]);
        return intent;
    }

    public void btnRun(View v) {
        int count = 0;
        int modelsCount = TestModels.modelsList().size();
        for (int i = 0; i < modelsCount; i++) {
            if (mTestListView.isItemChecked(i)) {
                count++;
            }
        }
        if (count == 0) {
            return;
        }

        int testList[] = new int[count];
        count = 0;
        for (int i = 0; i < modelsCount; i++) {
            if (mTestListView.isItemChecked(i)) {
                testList[count++] = i;
            }
        }

        Intent intent = makeBasicLaunchIntent();
        intent.putExtra(NNBenchmark.EXTRA_TESTS, testList);
        startActivityForResult(intent, 0);
    }

    String getResultShortSummary(BenchmarkResult br, TestModels.TestModelEntry t) {
        return br.getSummary(t.mBaselineSec);
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == 0) {
            if (resultCode == RESULT_OK) {
                int size = TestModels.modelsList().size();
                mResults = new float[size];
                mInfo = new String[size];

                Parcelable r[] = data.getParcelableArrayExtra(NNBenchmark.EXTRA_RESULTS_RESULTS);
                int id[] = data.getIntArrayExtra(NNBenchmark.EXTRA_RESULTS_TESTS);

                String mOutResult = "";
                for (int ct = 0; ct < id.length; ct++) {
                    TestModels.TestModelEntry t = TestModels.modelsList().get(id[ct]);
                    BenchmarkResult br = (BenchmarkResult) r[ct];

                    String s = t.toString() + " " + getResultShortSummary(br, t);
                    mTestList.set(id[ct], s);
                    mTestListAdapter.notifyDataSetChanged();
                    mOutResult += s + '\n';
                    mResults[id[ct]] = ((BenchmarkResult) r[ct]).getMeanTimeSec();
                }

                mResultView.setText(mOutResult);
            }
        }
    }

    public void btnSelAll(View v) {
        for (int i = 0; i < TestModels.modelsList().size(); i++) {
            mTestListView.setItemChecked(i, true);
        }
    }

    public void btnSelNone(View v) {
        for (int i = 0; i < TestModels.modelsList().size(); i++) {
            mTestListView.setItemChecked(i, false);
        }
    }

    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle presses on the action bar items
        switch (item.getItemId()) {
            case R.id.action_settings:
                NNSettings newFragment = new NNSettings(mSettings);
                newFragment.show(getFragmentManager(), "settings");
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    public void btnSettings(View v) {
        NNSettings newFragment = new NNSettings(mSettings);
        newFragment.show(getFragmentManager(), "settings");
    }

    public void btnRunDogfood(View v) {
        // Update settings for dogfood.
        mSettings[SETTING_LONG_RUN] = true;
        mSettings[SETTING_PAUSE] = false;
        mSettings[SETTING_DISABLE_NNAPI] = false;

        // Select dogfood models.
        long seed = System.currentTimeMillis();
        Log.v(NNBenchmark.TAG, "Dogfood run seed " + seed);
        Random random = new Random(seed);
        int numModelsToSelect = Math.min(DOGFOOD_MODELS_PER_RUN, mTestList.size());
        for (int i = 0; i < mTestList.size(); i++) {
          mTestListView.setItemChecked(i, false);
        }
        while (numModelsToSelect > 0) {
            int i = random.nextInt(mTestList.size());
            if (mTestListView.isItemChecked(i)) {
                continue;
            }
            mTestListView.setItemChecked(i, true);
            numModelsToSelect--;
        }

        // Run benchmark.
        btnRun(v);
    }
}
