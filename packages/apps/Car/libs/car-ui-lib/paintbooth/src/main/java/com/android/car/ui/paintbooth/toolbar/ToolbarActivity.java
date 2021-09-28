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
package com.android.car.ui.paintbooth.toolbar;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.AlertDialogBuilder;
import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.paintbooth.R;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.TabLayout;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class ToolbarActivity extends AppCompatActivity implements InsetsChangedListener {

    private List<MenuItem> mMenuItems = new ArrayList<>();
    private List<Pair<CharSequence, View.OnClickListener>> mButtons = new ArrayList<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(getLayout());

        ToolbarController toolbarNonFinal = CarUi.getToolbar(this);
        if (toolbarNonFinal == null) {
            toolbarNonFinal = requireViewById(R.id.toolbar);
        }
        ToolbarController toolbar = toolbarNonFinal;
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

        mMenuItems.add(MenuItem.builder(this)
                .setToSearch()
                .setOnClickListener(i -> toolbar.setState(Toolbar.State.SEARCH))
                .build());

        toolbar.setMenuItems(mMenuItems);

        mButtons.add(Pair.create("Toggle progress bar", v -> {
            toolbar.getProgressBar().setVisible(!toolbar.getProgressBar().isVisible());
        }));

        mButtons.add(Pair.create("Change title", v ->
                toolbar.setTitle(toolbar.getTitle() + " X")));

        mButtons.add(Pair.create("Add/Change subtitle", v -> {
            CharSequence subtitle = toolbar.getSubtitle();
            if (TextUtils.isEmpty(subtitle)) {
                toolbar.setSubtitle("Subtitle");
            } else {
                toolbar.setSubtitle(subtitle + " X");
            }
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_set_xml_resource), v -> {
            mMenuItems.clear();
            toolbar.setMenuItems(R.xml.menuitems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_icon), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setToSettings()
                    .setOnClickListener(i -> Toast.makeText(this, "Clicked",
                            Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_untined_icon), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setTinted(false)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_switch), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setCheckable()
                    .setOnClickListener(
                            i ->
                                    Toast.makeText(this,
                                            "Checked? " + i.isChecked(),
                                            Toast.LENGTH_SHORT)
                                            .show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_text), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setTitle("Baz")
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_icon_text), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setTitle("Bar")
                    .setShowIconAndTitle(true)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_bordered_text), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setTitle("Baz")
                    .setPrimary(true)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_bordered_icon_text), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setTitle("Bar")
                    .setPrimary(true)
                    .setShowIconAndTitle(true)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_untinted_icon_and_text), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setTitle("Bar")
                    .setShowIconAndTitle(true)
                    .setTinted(false)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_activatable), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setActivatable()
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_morphing), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setTitle("Become icon")
                    .setOnClickListener(i ->
                            i.setIcon(i.getIcon() == null ? R.drawable.ic_tracklist : 0))
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        Mutable<Integer> overflowCounter = new Mutable<>(1);
        mButtons.add(Pair.create(getString(R.string.toolbar_add_overflow), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setTitle("Foo " + overflowCounter.value)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .setDisplayBehavior(MenuItem.DisplayBehavior.NEVER)
                    .build());
            toolbar.setMenuItems(mMenuItems);
            overflowCounter.value++;
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_overflow_switch), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setTitle("Foo " + overflowCounter.value)
                    .setOnClickListener(
                            i -> Toast.makeText(this,
                                    i.isChecked() ? "Checked" : "Unchecked",
                                    Toast.LENGTH_SHORT)
                                    .show())
                    .setDisplayBehavior(MenuItem.DisplayBehavior.NEVER)
                    .setCheckable()
                    .build());
            toolbar.setMenuItems(mMenuItems);
            overflowCounter.value++;
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_icon_text_overflow), v -> {
            mMenuItems.add(MenuItem.builder(this)
                    .setIcon(R.drawable.ic_tracklist)
                    .setTitle("Bar")
                    .setShowIconAndTitle(true)
                    .setOnClickListener(
                            i -> Toast.makeText(this, "Clicked",
                                    Toast.LENGTH_SHORT).show())
                    .setDisplayBehavior(MenuItem.DisplayBehavior.NEVER)
                    .build());
            toolbar.setMenuItems(mMenuItems);
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_visibility),
                v -> getMenuItem(item -> item.setVisible(!item.isVisible()))));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_enable),
                v -> getMenuItem(item -> item.setEnabled(!item.isEnabled()))));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_perform_click),
                v -> getMenuItem(MenuItem::performClick)));

        final Drawable altIcon = getDrawable(R.drawable.ic_cut);
        Map<MenuItem, Drawable> iconBackups = new HashMap<>();
        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_icon), v ->
                getMenuItem(item -> {
                    Drawable currentIcon = item.getIcon();
                    Drawable newIcon = altIcon;
                    if (iconBackups.containsKey(item)) {
                        newIcon = iconBackups.get(item);
                    }
                    item.setIcon(newIcon);
                    iconBackups.put(item, currentIcon);
                })));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_show_while_search), v ->
                toolbar.setShowMenuItemsWhileSearching(
                        !toolbar.getShowMenuItemsWhileSearching())));

        mButtons.add(Pair.create(getString(R.string.toolbar_cycle_nav_button), v -> {
            Toolbar.NavButtonMode mode = toolbar.getNavButtonMode();
            if (mode == Toolbar.NavButtonMode.BACK) {
                toolbar.setNavButtonMode(Toolbar.NavButtonMode.CLOSE);
            } else if (mode == Toolbar.NavButtonMode.CLOSE) {
                toolbar.setNavButtonMode(Toolbar.NavButtonMode.DOWN);
            } else {
                toolbar.setNavButtonMode(Toolbar.NavButtonMode.BACK);
            }
        }));

        Mutable<Boolean> hasLogo = new Mutable<>(true);
        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_logo), v -> {
            toolbar.setLogo(hasLogo.value ? 0 : R.drawable.ic_launcher);
            hasLogo.value = !hasLogo.value;
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_cycle_state), v -> {
            if (toolbar.getState() == Toolbar.State.SUBPAGE) {
                toolbar.setState(Toolbar.State.HOME);
            } else if (toolbar.getState() == Toolbar.State.HOME) {
                toolbar.setState(Toolbar.State.EDIT);
            } else {
                toolbar.setState(Toolbar.State.SUBPAGE);
            }
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_search_hint), v -> {
            if (toolbar.getSearchHint().toString().contentEquals("Foo")) {
                toolbar.setSearchHint("Bar");
            } else {
                toolbar.setSearchHint("Foo");
            }
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_background),
                v -> toolbar.setBackgroundShown(!toolbar.getBackgroundShown())));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_tab), v -> toolbar.addTab(
                new TabLayout.Tab(getDrawable(R.drawable.ic_launcher), "Foo"))));

        mButtons.add(Pair.create(getString(R.string.toolbar_add_tab_with_custom_text), v -> {
            SimpleTextWatcher textWatcher = new SimpleTextWatcher();
            new AlertDialogBuilder(this)
                    .setEditBox(null, textWatcher, null)
                    .setTitle("Enter the text for the title")
                    .setPositiveButton("Ok", (dialog, which) ->
                            toolbar.addTab(
                                    new TabLayout.Tab(
                                            getDrawable(
                                                    R.drawable.ic_launcher),
                                            textWatcher.getText())))
                    .show();
        }));

        mButtons.add(Pair.create(getString(R.string.toolbar_show_tabs_in_subpage), v ->
                toolbar.setShowTabsInSubpage(!toolbar.getShowTabsInSubpage())));

        Mutable<Boolean> showingLauncherIcon = new Mutable<>(false);
        mButtons.add(Pair.create(getString(R.string.toolbar_toggle_search_icon), v -> {
            if (showingLauncherIcon.value) {
                toolbar.setSearchIcon(null);
            } else {
                toolbar.setSearchIcon(R.drawable.ic_launcher);
            }
            showingLauncherIcon.value = !showingLauncherIcon.value;
        }));

        CarUiRecyclerView prv = requireViewById(R.id.list);
        prv.setAdapter(mAdapter);
    }

    /** Override in subclasses to change the layout */
    protected int getLayout() {
        return R.layout.car_ui_recycler_view_activity;
    }

    @Override
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        requireViewById(R.id.list)
                .setPadding(0, insets.getTop(), 0, insets.getBottom());
        requireViewById(android.R.id.content)
                .setPadding(insets.getLeft(), 0, insets.getRight(), 0);
    }

    public void xmlMenuItemClicked(MenuItem item) {
        Toast.makeText(this, "Xml item clicked! " + item.getTitle() + ", id: " + item.getId(),
                Toast.LENGTH_SHORT).show();
    }

    private void getMenuItem(MenuItem.OnClickListener listener) {
        if (mMenuItems.size() == 1) {
            listener.onClick(mMenuItems.get(0));
            return;
        }

        SimpleTextWatcher textWatcher = new SimpleTextWatcher();
        new AlertDialogBuilder(this)
                .setEditBox("", textWatcher, null, InputType.TYPE_CLASS_NUMBER)
                .setTitle("Enter the index of the MenuItem")
                .setPositiveButton("Ok", (dialog, which) -> {
                    try {
                        MenuItem item = mMenuItems.get(
                                Integer.parseInt(textWatcher.getText()));
                        listener.onClick(item);
                    } catch (NumberFormatException | IndexOutOfBoundsException e) {
                        Toast.makeText(this, "Invalid index \"" + textWatcher.getText()
                                        + "\", valid range is 0 to " + (mMenuItems.size() - 1),
                                Toast.LENGTH_LONG).show();
                    }
                }).show();
    }

    private static class ViewHolder extends RecyclerView.ViewHolder {

        private final Button mButton;

        ViewHolder(View itemView) {
            super(itemView);
            mButton = itemView.requireViewById(R.id.button);
        }

        public void bind(CharSequence title, View.OnClickListener listener) {
            mButton.setText(title);
            mButton.setOnClickListener(listener);
        }
    }

    private final RecyclerView.Adapter<ViewHolder> mAdapter =
            new RecyclerView.Adapter<ViewHolder>() {
                @Override
                public int getItemCount() {
                    return mButtons.size();
                }

                @Override
                public ViewHolder onCreateViewHolder(ViewGroup parent, int position) {
                    View item =
                            LayoutInflater.from(parent.getContext()).inflate(R.layout.list_item,
                                    parent, false);
                    return new ViewHolder(item);
                }

                @Override
                public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
                    Pair<CharSequence, View.OnClickListener> pair = mButtons.get(position);
                    holder.bind(pair.first, pair.second);
                }
            };

    /**
     * For changing values from lambdas
     */
    private static final class Mutable<E> {

        public E value;

        Mutable() {
            value = null;
        }

        Mutable(E value) {
            this.value = value;
        }
    }

    /**
     * Used for getting text from a dialog.
     */
    private static final class SimpleTextWatcher implements TextWatcher {

        private String mValue;

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
        }

        @Override
        public void afterTextChanged(Editable s) {
            mValue = s.toString();
        }

        public String getText() {
            return mValue;
        }
    }
}
