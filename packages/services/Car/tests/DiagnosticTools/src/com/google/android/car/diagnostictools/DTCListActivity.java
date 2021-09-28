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

package com.google.android.car.diagnostictools;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Toolbar;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

/** Displays a list of DTCs associated with an ECU */
public class DTCListActivity extends Activity {

    private static final String TAG = "DTCListActivity";
    private String mEcuName;
    private List<DTC> mDtcs;
    private Toolbar mEcuTitle;
    private RecyclerView mDTCList;
    private DTCAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_dtc_list);
        mEcuTitle = findViewById(R.id.toolbar);
        mEcuTitle.setNavigationIcon(R.drawable.ic_arrow_back_black_24dp);
        mEcuTitle.setNavigationOnClickListener(view -> finish());

        mDTCList = findViewById(R.id.dtc_list);
        mDTCList.setHasFixedSize(true);
        mDTCList.setLayoutManager(new LinearLayoutManager(this));
        getIncomingIntent();
    }

    /** Handle incoming intent and extras. Extract ECU name and DTC list */
    private void getIncomingIntent() {
        Log.d(TAG, "getIncomingIntent: extras: " + getIntent().toString());
        if (getIntent().hasExtra("name")) {
            mEcuName = getIntent().getStringExtra("name");
            mEcuTitle.setTitle(mEcuName);
        }
        if (getIntent().hasExtra("dtcs")) {
            mDtcs = getIntent().getParcelableArrayListExtra("dtcs");
            mAdapter = new DTCAdapter(mDtcs, this);
            mDTCList.setAdapter(mAdapter);
            mDTCList.addItemDecoration(
                    new DividerItemDecoration(
                            mDTCList.getContext(), DividerItemDecoration.VERTICAL));
        }
    }

    /**
     * Handle clicks from the ActionButton. Confirms that the user wants to clear selected DTCs and
     * then deletes them on confirmation.
     */
    public void onClickActionButton(View view) {
        new AlertDialog.Builder(this)
                .setTitle("Clear DTCs")
                .setMessage(
                        String.format(
                                "Do you really want to clear %d mDtcs?", mAdapter.numSelected()))
                .setIcon(android.R.drawable.ic_dialog_alert)
                .setPositiveButton(
                        android.R.string.yes, (dialog, whichButton) -> mAdapter.deleteSelected())
                .setNegativeButton(android.R.string.no, null)
                .show();
    }
}
