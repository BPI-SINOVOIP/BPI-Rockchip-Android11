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

package com.google.android.car.diagnostictools.utils;

import android.graphics.Color;

import com.google.android.car.diagnostictools.RowViewHolder;

/**
 * Data model class which ECUs and DTCs extend. Allows those classes to be connected with a
 * SelectableAdapter to enable a RecyclerView with elements getting selected
 */
public abstract class SelectableRowModel {

    /** If model is selected */
    private boolean mIsSelected;

    boolean isSelected() {
        return mIsSelected;
    }

    void setSelected(boolean selected) {
        mIsSelected = selected;
    }

    /**
     * Takes in a RowViewHolder object and sets its layout background to its appropriate color based
     * on if it is selected or not. Uses hardcoded values for the selected and default backgrounds.
     *
     * @param holder RowViewHolder object associated with the model object
     */
    public void setColor(RowViewHolder holder) {
        int color = mIsSelected ? Color.GRAY : Color.parseColor("#303030");
        holder.layout.setBackgroundColor(color);
    }

    /**
     * Get the number of elements selected when this model is selected. If the object has children
     * which are also selected as a result of this select, then those should be counted in that
     * number.
     *
     * @return number of elements selected
     */
    public abstract int numSelected();

    /**
     * Delete this model and all of its children. This allows the model to run special functions to
     * delete itself in any other representation (DTCs can be deleted from the VHAL).
     */
    public abstract void delete();
}
