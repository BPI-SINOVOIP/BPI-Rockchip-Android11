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
#include <NdkMediaExtractor.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <nativehelper/ScopedUtfChars.h>
#include <thread>

extern "C" JNIEXPORT void JNICALL
Java_android_media_cts_MediaExtractorDeviceSideTest_extractUsingNdkMediaExtractor(
        JNIEnv* env, jobject, jobject assetManager, jstring assetPath, jboolean withAttachedJvm) {
    ScopedUtfChars scopedPath(env, assetPath);

    AAssetManager* nativeAssetManager = AAssetManager_fromJava(env, assetManager);
    AAsset* asset = AAssetManager_open(nativeAssetManager, scopedPath.c_str(), AASSET_MODE_RANDOM);
    off_t start;
    off_t length;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);

    auto mediaExtractorTask = [=]() {
        AMediaExtractor* mediaExtractor = AMediaExtractor_new();
        AMediaExtractor_setDataSourceFd(mediaExtractor, fd, start, length);
        AMediaExtractor_delete(mediaExtractor);
    };

    if (withAttachedJvm) {
        // The currently running thread is a Java thread so it has an attached JVM.
        mediaExtractorTask();
    } else {
        // We want to run the MediaExtractor calls on a thread with no JVM, so we spawn a new native
        // thread which will not have an associated JVM. We execute the MediaExtractor calls on the
        // new thread, and immediately join its execution so as to wait for its completion.
        std::thread(mediaExtractorTask).join();
    }
    // TODO: Make resource management automatic through scoped handles.
    close(fd);
    AAsset_close(asset);
}
