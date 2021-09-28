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

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

/** Generic ViewHolder which is used to represent data in rows. */
public class RowViewHolder extends RecyclerView.ViewHolder {

    public TextView textHeader;
    public TextView textDescription;
    public TextView textValueField;
    public CheckBox checkBox;
    public View layout;

    public RowViewHolder(@NonNull View itemView) {
        super(itemView);
        layout = itemView;
        textHeader = itemView.findViewById(R.id.firstLine);
        textDescription = itemView.findViewById(R.id.secondLine);
        textValueField = itemView.findViewById(R.id.dtc_number);
        checkBox = itemView.findViewById(R.id.checkBox);
    }

    /**
     * Full constructor which allows selective displaying of description, checkbox, and value
     * fields.
     *
     * @param itemView View to construct on
     * @param showDescription Display description if true, remove from view (View.GONE) if false
     * @param showCheckBox Display checkBox if true, remove from view (View.GONE) if false
     * @param showValue Display value if true, remove from view (View.GONE) if false
     */
    public RowViewHolder(
            @NonNull View itemView,
            boolean showDescription,
            boolean showCheckBox,
            boolean showValue) {
        this(itemView);
        checkBox.setVisibility(showCheckBox ? View.VISIBLE : View.GONE);
        textValueField.setVisibility(showValue ? View.VISIBLE : View.GONE);
        textDescription.setVisibility(showDescription ? View.VISIBLE : View.GONE);
    }

    /**
     * Set values of elements inside the view.
     *
     * @param header Header value to display
     * @param description Description value to display
     * @param valueField Value to display on the right side
     * @param chbx Whether or not the checkbox is checked
     */
    public void setFields(String header, String description, String valueField, boolean chbx) {
        textHeader.setText(header);
        textDescription.setText(description);
        textValueField.setText(valueField);
        checkBox.setChecked(chbx);
    }
}
