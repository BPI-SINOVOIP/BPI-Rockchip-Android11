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
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Toolbar;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

/** Displays a list of ECUs */
public class ECUListActivity extends Activity {

    private RecyclerView mRecyclerView;
    private ECUAdapter mAdapter;
    private RecyclerView.LayoutManager mLayoutManager;
    private Toolbar mToolbar;

    /** Called with the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View view = getLayoutInflater().inflate(R.layout.diagnostic_tools, null);
        setContentView(view);

        mRecyclerView = findViewById(R.id.my_recycler_view);

        mRecyclerView.setHasFixedSize(true);

        mLayoutManager = new LinearLayoutManager(this);
        mRecyclerView.setLayoutManager(mLayoutManager);
        List<ECU> input = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            input.add(ECU.createSampleECU(i, this));
        }
        mAdapter = new ECUAdapter(input, this);
        mRecyclerView.setAdapter(mAdapter);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(
                        mRecyclerView.getContext(), DividerItemDecoration.VERTICAL));

        mToolbar = findViewById(R.id.toolbar);
        setActionBar(mToolbar);
        mToolbar.setNavigationIcon(R.drawable.ic_show_chart_black_24dp);
        mToolbar.setTitle("ECU Overview");
        final Context mContext = this;
        mToolbar.setNavigationOnClickListener(
                view1 -> {
                    Intent intent = new Intent(mContext, LiveDataActivity.class);
                    mContext.startActivity(intent);
                });
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
