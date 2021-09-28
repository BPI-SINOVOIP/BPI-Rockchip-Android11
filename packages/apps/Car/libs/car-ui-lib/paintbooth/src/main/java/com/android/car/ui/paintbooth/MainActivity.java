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

package com.android.car.ui.paintbooth;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Switch;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.paintbooth.caruirecyclerview.CarUiListItemActivity;
import com.android.car.ui.paintbooth.caruirecyclerview.CarUiRecyclerViewActivity;
import com.android.car.ui.paintbooth.caruirecyclerview.GridCarUiRecyclerViewActivity;
import com.android.car.ui.paintbooth.currentactivity.CurrentActivityService;
import com.android.car.ui.paintbooth.dialogs.DialogsActivity;
import com.android.car.ui.paintbooth.overlays.OverlayActivity;
import com.android.car.ui.paintbooth.preferences.PreferenceActivity;
import com.android.car.ui.paintbooth.toolbar.NoCarUiToolbarActivity;
import com.android.car.ui.paintbooth.toolbar.OldToolbarActivity;
import com.android.car.ui.paintbooth.toolbar.ToolbarActivity;
import com.android.car.ui.paintbooth.widescreenime.WideScreenImeActivity;
import com.android.car.ui.paintbooth.widgets.WidgetActivity;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.ToolbarController;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

/**
 * Paint booth app
 */
public class MainActivity extends Activity implements InsetsChangedListener {

    public static final String STOP_SERVICE = "com.android.car.ui.paintbooth.StopService";

    /**
     * List of all sample activities.
     */
    private final List<ListElement> mActivities = Arrays.asList(
            new ServiceElement("Show foreground activities", CurrentActivityService.class),
            new ServiceElement("Simulate Screen Bounds", VisibleBoundsSimulator.class),
            new ActivityElement("Dialogs sample", DialogsActivity.class),
            new ActivityElement("List sample", CarUiRecyclerViewActivity.class),
            new ActivityElement("Grid sample", GridCarUiRecyclerViewActivity.class),
            new ActivityElement("Preferences sample", PreferenceActivity.class),
            new ActivityElement("Overlays", OverlayActivity.class),
            new ActivityElement("Toolbar sample", ToolbarActivity.class),
            new ActivityElement("Old toolbar sample", OldToolbarActivity.class),
            new ActivityElement("No CarUiToolbar sample", NoCarUiToolbarActivity.class),
            new ActivityElement("Widget sample", WidgetActivity.class),
            new ActivityElement("Wide Screen IME", WideScreenImeActivity.class),
            new ActivityElement("ListItem sample", CarUiListItemActivity.class));

    private abstract static class ViewHolder extends RecyclerView.ViewHolder {

        ViewHolder(@NonNull View itemView) {
            super(itemView);
        }

        public abstract void bind(ListElement element);
    }

    private class ActivityViewHolder extends ViewHolder {
        private final Button mButton;

        ActivityViewHolder(@NonNull View itemView) {
            super(itemView);
            mButton = itemView.requireViewById(R.id.button);
        }

        @Override
        public void bind(ListElement e) {
            if (!(e instanceof ActivityElement)) {
                throw new IllegalArgumentException("Expected an ActivityElement");
            }
            ActivityElement element = (ActivityElement) e;
            mButton.setText(element.getText());
            mButton.setOnClickListener(v ->
                    startActivity(new Intent(itemView.getContext(), element.getActivity())));
        }
    }

    private class ServiceViewHolder extends ViewHolder {
        private final Switch mSwitch;

        ServiceViewHolder(@NonNull View itemView) {
            super(itemView);
            mSwitch = itemView.requireViewById(R.id.button);
        }

        @Override
        public void bind(ListElement e) {
            if (!(e instanceof ServiceElement)) {
                throw new IllegalArgumentException("Expected an ActivityElement");
            }
            ServiceElement element = (ServiceElement) e;
            mSwitch.setChecked(isServiceRunning(element.getService()));
            mSwitch.setText(element.getText());
            mSwitch.setOnClickListener(v -> {
                Intent intent = new Intent(itemView.getContext(), element.getService());
                if (isServiceRunning(element.getService())) {
                    intent.setAction(STOP_SERVICE);
                }
                startForegroundService(intent);
            });
        }
    }

    private final RecyclerView.Adapter<ViewHolder> mAdapter =
            new RecyclerView.Adapter<ViewHolder>() {
                @NonNull
                @Override
                public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
                    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                    if (viewType == ListElement.TYPE_ACTIVITY) {
                        return new ActivityViewHolder(
                                inflater.inflate(R.layout.list_item, parent, false));
                    } else if (viewType == ListElement.TYPE_SERVICE) {
                        return new ServiceViewHolder(
                                inflater.inflate(R.layout.list_item_switch, parent, false));
                    } else {
                        throw new IllegalArgumentException("Unknown viewType: " + viewType);
                    }
                }

                @Override
                public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
                    holder.bind(mActivities.get(position));
                }

                @Override
                public int getItemCount() {
                    return mActivities.size();
                }

                @Override
                public int getItemViewType(int position) {
                    return mActivities.get(position).getType();
                }
            };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_ui_recycler_view_activity);

        ToolbarController toolbar = CarUi.requireToolbar(this);
        toolbar.setLogo(R.drawable.ic_launcher);
        toolbar.setTitle(getTitle());

        CarUiRecyclerView prv = findViewById(R.id.list);
        prv.setAdapter(mAdapter);

        initLeakCanary();
    }

    private void initLeakCanary() {
        // This sets LeakCanary to report errors after a single leak instead of 5, and to ask for
        // permission to use storage, which it needs to work.
        //
        // Equivalent to this non-reflection code:
        //
        // Config config = LeakCanary.INSTANCE.getConfig();
        // LeakCanary.INSTANCE.setConfig(config.copy(config.getDumpHeap(),
        //     config.getDumpHeapWhenDebugging(),
        //     1,
        //     config.getReferenceMatchers(),
        //     config.getObjectInspectors(),
        //     config.getOnHeapAnalyzedListener(),
        //     config.getMetatadaExtractor(),
        //     config.getComputeRetainedHeapSize(),
        //     config.getMaxStoredHeapDumps(),
        //     true,
        //     config.getUseExperimentalLeakFinders()));
        try {
            Class<?> canaryClass = Class.forName("leakcanary.LeakCanary");
            try {
                Class<?> onHeapAnalyzedListenerClass =
                        Class.forName("leakcanary.OnHeapAnalyzedListener");
                Class<?> metadataExtractorClass = Class.forName("shark.MetadataExtractor");
                Method getConfig = canaryClass.getMethod("getConfig");
                Class<?> configClass = getConfig.getReturnType();
                Method setConfig = canaryClass.getMethod("setConfig", configClass);
                Method copy = configClass.getMethod("copy", boolean.class, boolean.class,
                        int.class, List.class, List.class, onHeapAnalyzedListenerClass,
                        metadataExtractorClass, boolean.class, int.class, boolean.class,
                        boolean.class);

                Object canary = canaryClass.getField("INSTANCE").get(null);
                Object currentConfig = getConfig.invoke(canary);

                Boolean dumpHeap = (Boolean) configClass
                        .getMethod("getDumpHeap").invoke(currentConfig);
                Boolean dumpHeapWhenDebugging = (Boolean) configClass
                        .getMethod("getDumpHeapWhenDebugging").invoke(currentConfig);
                List<?> referenceMatchers = (List<?>) configClass
                        .getMethod("getReferenceMatchers").invoke(currentConfig);
                List<?> objectInspectors = (List<?>) configClass
                        .getMethod("getObjectInspectors").invoke(currentConfig);
                Object onHeapAnalyzedListener = configClass
                        .getMethod("getOnHeapAnalyzedListener").invoke(currentConfig);
                // Yes, LeakCanary misspelled metadata
                Object metadataExtractor = configClass
                        .getMethod("getMetatadaExtractor").invoke(currentConfig);
                Boolean computeRetainedHeapSize = (Boolean) configClass
                        .getMethod("getComputeRetainedHeapSize").invoke(currentConfig);
                Integer maxStoredHeapDumps = (Integer) configClass
                        .getMethod("getMaxStoredHeapDumps").invoke(currentConfig);
                Boolean useExperimentalLeakFinders = (Boolean) configClass
                        .getMethod("getUseExperimentalLeakFinders").invoke(currentConfig);

                setConfig.invoke(canary, copy.invoke(currentConfig,
                        dumpHeap,
                        dumpHeapWhenDebugging,
                        1,
                        referenceMatchers,
                        objectInspectors,
                        onHeapAnalyzedListener,
                        metadataExtractor,
                        computeRetainedHeapSize,
                        maxStoredHeapDumps,
                        true,
                        useExperimentalLeakFinders));

            } catch (ReflectiveOperationException e) {
                Log.e("paintbooth", "Error initializing LeakCanary", e);
                Toast.makeText(this, "Error initializing LeakCanary", Toast.LENGTH_LONG).show();
            }
        } catch (ClassNotFoundException e) {
            // LeakCanary is not used in this build, do nothing.
        }
    }

    private boolean isServiceRunning(Class<? extends Service> serviceClazz) {
        ActivityManager manager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningServiceInfo service : manager.getRunningServices(
                Integer.MAX_VALUE)) {
            if (serviceClazz.getName().equals(service.service.getClassName())) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        requireViewById(R.id.list)
                .setPadding(0, insets.getTop(), 0, insets.getBottom());
        requireViewById(android.R.id.content)
                .setPadding(insets.getLeft(), 0, insets.getRight(), 0);
    }

    private abstract static class ListElement {
        static final int TYPE_ACTIVITY = 0;
        static final int TYPE_SERVICE = 1;

        private final String mText;

        ListElement(String text) {
            mText = text;
        }

        String getText() {
            return mText;
        }

        abstract int getType();
    }

    private static class ActivityElement extends ListElement {
        private final Class<? extends Activity> mActivityClass;

        ActivityElement(String text, Class<? extends Activity> activityClass) {
            super(text);
            mActivityClass = activityClass;
        }

        Class<? extends Activity> getActivity() {
            return mActivityClass;
        }

        @Override
        int getType() {
            return TYPE_ACTIVITY;
        }
    }

    private static class ServiceElement extends ListElement {
        private final Class<? extends Service> mServiceClass;

        ServiceElement(String text, Class<? extends Service> serviceClass) {
            super(text);
            mServiceClass = serviceClass;
        }

        Class<? extends Service> getService() {
            return mServiceClass;
        }

        @Override
        int getType() {
            return TYPE_SERVICE;
        }
    }
}
