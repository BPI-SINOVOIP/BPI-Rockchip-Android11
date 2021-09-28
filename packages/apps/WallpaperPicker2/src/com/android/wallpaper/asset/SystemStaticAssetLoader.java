/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.asset;

import androidx.annotation.Nullable;

import com.android.wallpaper.asset.ResourceAssetLoader.ResourceAssetFetcher;

import com.bumptech.glide.load.Options;
import com.bumptech.glide.load.model.ModelLoader;
import com.bumptech.glide.load.model.ModelLoaderFactory;
import com.bumptech.glide.load.model.MultiModelLoaderFactory;

import java.io.InputStream;

/**
 * Glide ModelLoader which loads InputStreams from PartnerStaticAssets.
 */
public class SystemStaticAssetLoader implements ModelLoader<SystemStaticAsset, InputStream> {

    @Override
    public boolean handles(SystemStaticAsset systemStaticAsset) {
        return true;
    }

    @Nullable
    @Override
    public LoadData<InputStream> buildLoadData(SystemStaticAsset systemStaticAsset, int unusedWidth,
                                               int unusedHeight, Options options) {
        return new LoadData<>(systemStaticAsset.getKey(),
                new ResourceAssetFetcher(systemStaticAsset));
    }

    /**
     * Factory that constructs {@link SystemStaticAssetLoader} instances.
     */
    public static class SystemStaticAssetLoaderFactory
            implements ModelLoaderFactory<SystemStaticAsset, InputStream> {
        public SystemStaticAssetLoaderFactory() {
        }

        @Override
        public ModelLoader<SystemStaticAsset, InputStream> build(
                MultiModelLoaderFactory multiFactory) {
            return new SystemStaticAssetLoader();
        }

        @Override
        public void teardown() {
            // no-op
        }
    }
}
