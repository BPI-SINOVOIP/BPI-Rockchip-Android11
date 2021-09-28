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

import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

/**
 * Used to enable a RecyclerView Adapter to select and delete row elements. Designed to be self
 * contained so that implementing the abstract function will enable selecting behavior.
 *
 * @param <M> Base row model used. For example, the `DTC` class is used for the DTCAdapter
 * @param <VH> Base view holder used. Included so that SelectableAdapter can use
 *     notifyDataSetChanged
 */
public abstract class SelectableAdapter<
                M extends SelectableRowModel, VH extends RecyclerView.ViewHolder>
        extends RecyclerView.Adapter<VH> {

    private List<M> mSelected = new ArrayList<>();

    /**
     * Toggle if an element is selected or not.
     *
     * @param model The model object that is related to the row selected
     */
    public void toggleSelect(M model) {
        if (model.isSelected()) {
            model.setSelected(false);
            mSelected.remove(model);
        } else {
            model.setSelected(true);
            mSelected.add(model);
        }
    }

    /**
     * Returns the number of elements selected. Uses the model's numSelected method to allow for one
     * model object to represent multiple element (one ECU holds multiple DTCs). If no elements are
     * explicitly selected, it is assumed that all elements are selected.
     *
     * @return number of elements selected
     */
    public int numSelected() {
        int count = 0;
        for (M model : mSelected) {
            count += model.numSelected();
        }
        if (count == 0) {
            for (M model : getBaseList()) {
                count += model.numSelected();
            }
        }
        return count;
    }

    /**
     * Deletes all selected element or all elements if no elements are selected. Additionally, calls
     * the delete methods for all related models.
     */
    public void deleteSelected() {
        if (mSelected.size() == 0) {
            for (M model : getBaseList()) {
                model.delete();
            }
            getBaseList().clear();
        } else {
            for (M model : mSelected) {
                model.delete();
            }
            getBaseList().removeAll(mSelected);
            mSelected.clear();
        }
        notifyDataSetChanged();
    }

    /**
     * If there are any models explicitly selected. Does not take into account there are any
     * elements in that model (an ECU with 0 DTCs).
     *
     * @return if there are any models selected
     */
    public boolean hasSelected() {
        return mSelected.size() > 0;
    }

    /**
     * Gets base list that the adapter is built off of. This must be one list which the adapter uses
     * for both order and elements.
     *
     * @return base list that adapter is built off of
     */
    protected abstract List<M> getBaseList();
}
