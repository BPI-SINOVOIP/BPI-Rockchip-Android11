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

package com.android.car.ui.paintbooth.widescreenime;

import static android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI;

import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_DESC_TITLE_TO_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_DESC_TO_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_ERROR_DESC_TO_INPUT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.REQUEST_RENDER_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.WIDE_SCREEN_ACTION;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.WIDE_SCREEN_EXTRACTED_TEXT_ICON_RES_ID;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.imewidescreen.CarUiImeSearchListItem;
import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiContentListItem;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.Toolbar.State;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Activity that shows different scenarios for wide screen ime.
 */
public class WideScreenImeActivity extends AppCompatActivity implements InsetsChangedListener {

    private static final String TAG = "WideScreenImeActivity";

    private final List<MenuItem> mMenuItems = new ArrayList<>();
    private final List<ListElement> mWidescreenItems = new ArrayList<>();

    private final ArrayList<String> mItemIdList = new ArrayList<>();
    private final ArrayList<String> mTitleList = new ArrayList<>();
    private final ArrayList<String> mSubTitleList = new ArrayList<>();
    private final ArrayList<Integer> mPrimaryImageResId = new ArrayList<>();
    private final ArrayList<String> mSecondaryItemId = new ArrayList<>();
    private final ArrayList<Integer> mSecondaryImageResId = new ArrayList<>();
    private final List<CarUiImeSearchListItem> mSearchItems = new ArrayList<>();

    private InputMethodManager mInputMethodManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);

        mInputMethodManager = (InputMethodManager)
                getSystemService(Context.INPUT_METHOD_SERVICE);

        ToolbarController toolbar = CarUi.getToolbar(this);
        toolbar.setTitle(getTitle());
        toolbar.setState(Toolbar.State.SUBPAGE);
        toolbar.setLogo(R.drawable.ic_launcher);
        toolbar.registerOnBackListener(
                () -> {
                    if (toolbar.getState() == Toolbar.State.SEARCH
                            || toolbar.getState() == Toolbar.State.EDIT) {
                        toolbar.setState(Toolbar.State.SUBPAGE);
                        return true;
                    }
                    return false;
                });

        CarUiContentListItem.OnClickListener mainClickListener = i ->
                Toast.makeText(this, "Item clicked! " + i.getTitle(), Toast.LENGTH_SHORT).show();
        CarUiContentListItem.OnClickListener secondaryClickListener = i ->
                Toast.makeText(this, "Item's secondary action clicked!", Toast.LENGTH_SHORT).show();

        Drawable icon = getDrawable(R.drawable.ic_launcher);

        for (int i = 1; i <= 100; i++) {
            CarUiImeSearchListItem item = new CarUiImeSearchListItem(
                    CarUiContentListItem.Action.ICON);
            item.setTitle("Title " + i);
            item.setBody("Sub title " + i);
            item.setIcon(icon);
            item.setSupplementalIcon(icon, secondaryClickListener);
            item.setOnItemClickedListener(mainClickListener);

            mSearchItems.add(item);
        }

        AtomicBoolean showResultsInView = new AtomicBoolean(false);

        mMenuItems.add(MenuItem.builder(this)
                .setToSearch()
                .setOnClickListener(i -> {
                    toolbar.setState(State.SEARCH);
                    if (showResultsInView.get() && toolbar.canShowSearchResultsView()) {
                        LayoutInflater inflater = LayoutInflater.from(this);
                        View contentArea = inflater.inflate(R.layout.ime_wide_screen_dummy_view,
                                null, true);
                        contentArea.findViewById(R.id.button_1).setOnClickListener(v ->
                                Toast.makeText(this, "Button 1 clicked", Toast.LENGTH_SHORT).show()
                        );

                        contentArea.findViewById(R.id.button_2).setOnClickListener(v -> {
                                    Toast.makeText(this, "Clearing the view...",
                                            Toast.LENGTH_SHORT).show();
                                    toolbar.setSearchResultsView(null);
                                }
                        );
                        toolbar.setSearchResultsView(contentArea);
                    } else if (toolbar.canShowSearchResultItems()) {
                        toolbar.setSearchResultsView(null);
                        toolbar.setSearchResultItems(mSearchItems);
                    }
                })
                .build());

        toolbar.setMenuItems(mMenuItems);

        mWidescreenItems.add(new ButtonElement("Show custom search view", v -> {
            Switch swtch = (Switch) v;
            showResultsInView.set(swtch.isChecked());
        }));

        mWidescreenItems.add(new EditTextElement("Default Input Edit Text field", null));
        mWidescreenItems.add(
                new EditTextElement("Add Desc to content area", this::addDescToContentArea));
        mWidescreenItems.add(new EditTextElement("Hide the content area", this::hideContentArea));
        mWidescreenItems.add(new EditTextElement("Hide extraction view", this::hideExtractionView));

        for (int i = 0; i < 7; i++) {
            mItemIdList.add("itemId" + i);
            mTitleList.add("Title " + i);
            mSubTitleList.add("subtitle " + i);
            mPrimaryImageResId.add(R.drawable.ic_launcher);
            mSecondaryItemId.add("imageId" + i);
            mSecondaryImageResId.add(R.drawable.ic_launcher);
        }

        mWidescreenItems.add(
                new EditTextElement("Add icon to extracted view", this::addIconToExtractedView));
        mWidescreenItems.add(new EditTextElement("Add error message to content area",
                this::addErrorDescToContentArea));

        CarUiRecyclerView recyclerView = requireViewById(R.id.list);
        recyclerView.setAdapter(mAdapter);
    }

    private void addIconToExtractedView(View view, boolean hasFocus) {
        if (!hasFocus) {
            return;
        }

        Bundle bundle = new Bundle();
        bundle.putInt(WIDE_SCREEN_EXTRACTED_TEXT_ICON_RES_ID, R.drawable.car_ui_icon_edit);
        mInputMethodManager.sendAppPrivateCommand(view, WIDE_SCREEN_ACTION, bundle);
    }

    private void addErrorDescToContentArea(View view, boolean hasFocus) {
        if (!hasFocus) {
            return;
        }

        Bundle bundle = new Bundle();
        bundle.putString(ADD_ERROR_DESC_TO_INPUT_AREA, "Some error message");
        bundle.putString(ADD_DESC_TITLE_TO_CONTENT_AREA, "Title");
        bundle.putString(ADD_DESC_TO_CONTENT_AREA, "Description provided by the application");
        mInputMethodManager.sendAppPrivateCommand(view, WIDE_SCREEN_ACTION, bundle);
    }

    private void hideExtractionView(View view, boolean hasFocus) {
        if (!hasFocus) {
            return;
        }

        EditText editText = (EditText) view;
        editText.setImeOptions(IME_FLAG_NO_EXTRACT_UI);

        Bundle bundle = new Bundle();
        bundle.putBoolean(REQUEST_RENDER_CONTENT_AREA, false);
        mInputMethodManager.sendAppPrivateCommand(view, WIDE_SCREEN_ACTION, bundle);
    }

    private void addDescToContentArea(View view, boolean hasFocus) {
        if (!hasFocus) {
            return;
        }

        Bundle bundle = new Bundle();
        bundle.putString(ADD_DESC_TITLE_TO_CONTENT_AREA, "Title");
        bundle.putString(ADD_DESC_TO_CONTENT_AREA, "Description provided by the application");
        mInputMethodManager.sendAppPrivateCommand(view, WIDE_SCREEN_ACTION, bundle);
    }

    private void hideContentArea(View view, boolean hasFocus) {
        if (!hasFocus) {
            return;
        }

        Bundle bundle = new Bundle();
        bundle.putBoolean(REQUEST_RENDER_CONTENT_AREA, false);
        mInputMethodManager.sendAppPrivateCommand(view, WIDE_SCREEN_ACTION, bundle);
    }


    private abstract static class ViewHolder extends RecyclerView.ViewHolder {

        ViewHolder(@NonNull View itemView) {
            super(itemView);
        }

        public abstract void bind(ListElement element);
    }

    private static class EditTextViewHolder extends ViewHolder {
        private final EditText mEditText;

        EditTextViewHolder(@NonNull View itemView) {
            super(itemView);
            mEditText = itemView.requireViewById(R.id.edit_text);
        }

        @Override
        public void bind(ListElement e) {
            if (!(e instanceof EditTextElement)) {
                throw new IllegalArgumentException("Expected an EditTextElement");
            }
            EditTextElement element = (EditTextElement) e;
            mEditText.setText(element.getText());
            mEditText.setOnFocusChangeListener(element.getOnFocusChangeListener());
        }
    }

    private static class ButtonViewHolder extends ViewHolder {
        private final Switch mSwitch;

        ButtonViewHolder(@NonNull View itemView) {
            super(itemView);
            mSwitch = itemView.requireViewById(R.id.button);
        }

        @Override
        public void bind(ListElement e) {
            if (!(e instanceof ButtonElement)) {
                throw new IllegalArgumentException("Expected an ButtonElement");
            }
            ButtonElement element = (ButtonElement) e;
            mSwitch.setText(element.getText());
            mSwitch.setOnClickListener(element.getOnClickListener());
            mSwitch.setChecked(false);
        }
    }

    private final RecyclerView.Adapter<ViewHolder> mAdapter =
            new RecyclerView.Adapter<ViewHolder>() {
                @Override
                public int getItemCount() {
                    return mWidescreenItems.size();
                }

                @Override
                public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
                    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                    if (viewType == ListElement.TYPE_EDIT_TEXT) {
                        return new EditTextViewHolder(
                                inflater.inflate(R.layout.edit_text_list_item, parent, false));
                    } else if (viewType == ListElement.TYPE_BUTTON) {
                        return new ButtonViewHolder(
                                inflater.inflate(R.layout.list_item_switch, parent, false));
                    } else {
                        throw new IllegalArgumentException("Unknown viewType: " + viewType);
                    }
                }

                @Override
                public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
                    holder.bind(mWidescreenItems.get(position));
                }

                @Override
                public int getItemViewType(int position) {
                    return mWidescreenItems.get(position).getType();
                }
            };

    @Override
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        requireViewById(R.id.list)
                .setPadding(0, insets.getTop(), 0, insets.getBottom());
        requireViewById(android.R.id.content)
                .setPadding(insets.getLeft(), 0, insets.getRight(), 0);
    }

    private abstract static class ListElement {
        static final int TYPE_EDIT_TEXT = 0;
        static final int TYPE_BUTTON = 1;

        private final String mText;

        ListElement(String text) {
            mText = text;
        }

        String getText() {
            return mText;
        }

        abstract int getType();
    }

    private static class EditTextElement extends ListElement {
        private OnFocusChangeListener mListener;

        EditTextElement(String text, OnFocusChangeListener listener) {
            super(text);
            mListener = listener;
        }

        OnFocusChangeListener getOnFocusChangeListener() {
            return mListener;
        }

        @Override
        int getType() {
            return TYPE_EDIT_TEXT;
        }
    }

    private static class ButtonElement extends ListElement {
        private OnClickListener mOnClickListener;

        ButtonElement(String text, OnClickListener listener) {
            super(text);
            mOnClickListener = listener;
        }

        public OnClickListener getOnClickListener() {
            return mOnClickListener;
        }

        @Override
        int getType() {
            return TYPE_BUTTON;
        }
    }
}
