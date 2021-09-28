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

package com.android.car.rotaryplayground;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

/**
 * Fragment with a RecyclerView Grid to demo and test z-pattern rotating navigation and
 * vertical scroll.
 */
public class RotaryGrid extends Fragment {

    private static final int NUMBER_OF_COLUMNS = 3;

    private static final String[] DATA = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
        "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
        "21", "22", "23", "24", "25", "26", "27", "28", "29", "30",
        "31", "32", "33", "34", "35", "36", "37", "38", "39", "40",
        "41", "42", "43", "44", "45", "46", "47", "48", "49", "50"};

    private RotaryGridAdapter mGridAdapter;

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.rotary_grid, container, false);
        populateGridItems(view);
        return view;
    }

    private void populateGridItems(View view) {
        RecyclerView gridView = view.findViewById(R.id.rotary_grid_view);
        gridView.setLayoutManager(new GridLayoutManager(getActivity(), NUMBER_OF_COLUMNS));
        mGridAdapter = new RotaryGridAdapter(getActivity(), DATA);
        gridView.setAdapter(mGridAdapter);
    }
}
