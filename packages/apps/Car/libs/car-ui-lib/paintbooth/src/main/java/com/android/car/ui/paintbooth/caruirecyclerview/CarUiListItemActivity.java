/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.ui.paintbooth.caruirecyclerview;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.recyclerview.CarUiHeaderListItem;
import com.android.car.ui.recyclerview.CarUiListItem;
import com.android.car.ui.recyclerview.CarUiListItemAdapter;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.ArrayList;

/**
 * Activity that shows {@link CarUiRecyclerView} with sample {@link CarUiContentListItem} entries
 */
public class CarUiListItemActivity extends Activity implements InsetsChangedListener {

    private final ArrayList<CarUiListItem> mData = new ArrayList<>();
    private CarUiListItemAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);

        ToolbarController toolbar = CarUi.requireToolbar(this);
        toolbar.setTitle(getTitle());
        toolbar.setState(Toolbar.State.SUBPAGE);

        CarUiRecyclerView recyclerView = findViewById(R.id.list);
        mAdapter = new CarUiListItemAdapter(generateSampleData());
        recyclerView.setAdapter(mAdapter);
    }

    private ArrayList<CarUiListItem> generateSampleData() {
        Context context = this;

        CarUiHeaderListItem header = new CarUiHeaderListItem("First header");
        mData.add(header);

        CarUiContentListItem item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test title");
        item.setBody("Test body");
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test title with no body");
        mData.add(item);

        header = new CarUiHeaderListItem("Random header", "with header body");
        mData.add(header);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setBody("Test body with no title");
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test Title");
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test Title");
        item.setBody("Test body text");
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test Title -- with content icon");
        item.setPrimaryIconType(CarUiContentListItem.IconType.CONTENT);
        item.setIcon(getDrawable(R.drawable.ic_sample_logo));
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test Title");
        item.setBody("With avatar icon.");
        item.setIcon(getDrawable(R.drawable.ic_sample_logo));
        item.setPrimaryIconType(CarUiContentListItem.IconType.AVATAR);

        item = new CarUiContentListItem(CarUiContentListItem.Action.NONE);
        item.setTitle("Test Title");
        item.setBody("Displays toast on click");
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setOnItemClickedListener(item1 -> {
            Toast.makeText(context, "Item clicked", Toast.LENGTH_SHORT).show();
        });
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.CHECK_BOX);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title -- Item with checkbox");
        item.setBody("Will present toast on change of selection state.");
        item.setOnCheckedChangeListener(
                (listItem, isChecked) -> Toast.makeText(context,
                        "Item checked state is: " + isChecked, Toast.LENGTH_SHORT).show());
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.CHECK_BOX);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setEnabled(false);
        item.setTitle("Title -- Checkbox that is disabled");
        item.setBody("Clicks should not have any affect");
        item.setOnCheckedChangeListener(
                (listItem, isChecked) -> Toast.makeText(context,
                        "Item checked state is: " + isChecked, Toast.LENGTH_SHORT).show());
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.SWITCH);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setBody("Body -- Item with switch  -- with click listener");
        item.setOnItemClickedListener(item1 -> {
            Toast.makeText(context, "Click on item with switch", Toast.LENGTH_SHORT).show();
        });
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.CHECK_BOX);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title -- Item with checkbox");
        item.setBody("Item is initially checked");
        item.setChecked(true);
        mData.add(item);

        CarUiContentListItem radioItem1 = new CarUiContentListItem(
                CarUiContentListItem.Action.RADIO_BUTTON);
        CarUiContentListItem radioItem2 = new CarUiContentListItem(
                CarUiContentListItem.Action.RADIO_BUTTON);

        radioItem1.setTitle("Title -- Item with radio button");
        radioItem1.setBody("Item is initially unchecked checked");
        radioItem1.setChecked(false);
        radioItem1.setOnCheckedChangeListener((listItem, isChecked) -> {
            if (isChecked) {
                radioItem2.setChecked(false);
                mAdapter.notifyItemChanged(mData.indexOf(radioItem2));
            }
        });
        mData.add(radioItem1);

        radioItem2.setIcon(getDrawable(R.drawable.ic_launcher));
        radioItem2.setTitle("Item is mutually exclusive with item above");
        radioItem2.setChecked(true);
        radioItem2.setOnCheckedChangeListener((listItem, isChecked) -> {
            if (isChecked) {
                radioItem1.setChecked(false);
                mAdapter.notifyItemChanged(mData.indexOf(radioItem1));
            }
        });
        mData.add(radioItem2);

        item = new CarUiContentListItem(CarUiContentListItem.Action.ICON);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Title");
        item.setBody("Random body text -- with action divider");
        item.setActionDividerVisible(true);
        item.setSupplementalIcon(getDrawable(R.drawable.ic_launcher));
        item.setChecked(true);
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.ICON);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setTitle("Null supplemental icon");
        item.setChecked(true);
        mData.add(item);

        item = new CarUiContentListItem(CarUiContentListItem.Action.ICON);
        item.setTitle("Supplemental icon with listener");
        item.setPrimaryIconType(CarUiContentListItem.IconType.CONTENT);
        item.setIcon(getDrawable(R.drawable.ic_launcher));
        item.setBody("body");
        item.setOnItemClickedListener(v -> Toast.makeText(context, "Clicked item",
                Toast.LENGTH_SHORT).show());
        item.setSupplementalIcon(getDrawable(R.drawable.ic_launcher),
                v -> Toast.makeText(context, "Clicked supplemental icon",
                        Toast.LENGTH_SHORT).show());
        item.setChecked(true);
        mData.add(item);

        return mData;
    }

    @Override
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        requireViewById(R.id.list)
                .setPadding(0, insets.getTop(), 0, insets.getBottom());
        requireViewById(android.R.id.content)
                .setPadding(insets.getLeft(), 0, insets.getRight(), 0);
    }
}
