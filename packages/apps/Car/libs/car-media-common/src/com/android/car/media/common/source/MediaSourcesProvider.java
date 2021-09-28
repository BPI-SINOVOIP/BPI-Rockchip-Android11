/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.media.common.source;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.service.media.MediaBrowserService;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.car.media.common.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Singleton that provides access to the list of all possible media sources that can be selected
 * to be played.
 */
public class MediaSourcesProvider {

    private static final String TAG = "MediaSources";

    private static MediaSourcesProvider sInstance;
    private final Context mAppContext;
    @Nullable
    private List<MediaSource> mMediaSources;

    private final BroadcastReceiver mAppInstallUninstallReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            reset();
        }
    };

    /** Returns the singleton instance. */
    public static MediaSourcesProvider getInstance(@NonNull Context context) {
        if (sInstance == null) {
            sInstance = new MediaSourcesProvider(context);
        }
        return sInstance;
    }

    /** Returns a different instance every time (tests don't like statics) */
    @VisibleForTesting
    public static MediaSourcesProvider createForTesting(@NonNull Context context) {
        return new MediaSourcesProvider(context);
    }

    @VisibleForTesting
    void reset() {
        mMediaSources = null;
    }

    private MediaSourcesProvider(@NonNull Context context) {
        mAppContext = context.getApplicationContext();
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_PACKAGE_ADDED);
        filter.addAction(Intent.ACTION_PACKAGE_REMOVED);
        filter.addDataScheme("package");
        mAppContext.registerReceiver(mAppInstallUninstallReceiver, filter);
    }

    /**
     * Returns the sorted list of available media sources. Sources listed in the array resource
     * R.array.preferred_media_sources are included first. Other sources follow in alphabetical
     * order.
     */
    public List<MediaSource> getList() {
        if (mMediaSources == null) {
            // Get the flattened components to display first.
            String[] preferredFlats = mAppContext.getResources().getStringArray(
                    R.array.preferred_media_sources);

            // Make a map of components to display first (the value is the component's index).
            HashMap<ComponentName, Integer> preferredComps = new HashMap<>(preferredFlats.length);
            for (int i = 0; i < preferredFlats.length; i++) {
                preferredComps.put(ComponentName.unflattenFromString(preferredFlats[i]), i);
            }

            // Prepare an array of the sources to display first (unavailable preferred components
            // will be excluded).
            MediaSource[] preferredSources = new MediaSource[preferredFlats.length];
            List<MediaSource> sortedSources = getComponentNames().stream()
                    .filter(Objects::nonNull)
                    .map(componentName -> MediaSource.create(mAppContext, componentName))
                    .filter(mediaSource -> {
                        if (mediaSource == null) {
                            Log.w(TAG, "Media source is null");
                            return false;
                        }
                        ComponentName srcComp = mediaSource.getBrowseServiceComponentName();
                        if (preferredComps.containsKey(srcComp)) {
                            // Record the source in the preferred array...
                            preferredSources[preferredComps.get(srcComp)] = mediaSource;
                            // And exclude it from the alpha sort.
                            return false;
                        }
                        return true;
                    })
                    .sorted(Comparator.comparing(
                            mediaSource -> mediaSource.getDisplayName().toString()))
                    .collect(Collectors.toList());

            // Concatenate the non null preferred sources and the sorted ones into the result.
            mMediaSources = new ArrayList<>(sortedSources.size() + preferredFlats.length);
            Arrays.stream(preferredSources).filter(Objects::nonNull).forEach(mMediaSources::add);
            mMediaSources.addAll(sortedSources);
        }
        return mMediaSources;
    }

    /**
     * Generates a set of all possible media services to choose from.
     */
    private Set<ComponentName> getComponentNames() {
        PackageManager packageManager = mAppContext.getPackageManager();
        Intent mediaIntent = new Intent();
        mediaIntent.setAction(MediaBrowserService.SERVICE_INTERFACE);
        List<ResolveInfo> mediaServices = packageManager.queryIntentServices(mediaIntent,
                PackageManager.GET_RESOLVED_FILTER);

        Set<ComponentName> components = new HashSet<>();
        for (ResolveInfo info : mediaServices) {
            ComponentName componentName = new ComponentName(info.serviceInfo.packageName,
                    info.serviceInfo.name);
            components.add(componentName);
        }
        return components;
    }

}
