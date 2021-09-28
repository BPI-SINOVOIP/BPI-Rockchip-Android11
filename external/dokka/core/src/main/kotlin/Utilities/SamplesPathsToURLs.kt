/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.jetbrains.dokka.Utilities

//HashMap of all filepaths to the URLs that should be downloaded to that filepath
val filepathsToUrls: HashMap<String, String> = hashMapOf(
    "./samples/development/samples/ApiDemos" to "https://android.googlesource.com/platform/development/+archive/refs/heads/master/samples/ApiDemos.tar.gz",
    "./samples/development/samples/NotePad" to "https://android.googlesource.com/platform/development/+archive/refs/heads/master/samples/NotePad.tar.gz",
    "./samples/external/icu/android_icu4j/src/samples/java/android/icu/samples/text" to "https://android.googlesource.com/platform/external/icu/+archive/refs/heads/master/android_icu4j/src/samples/java/android/icu/samples/text.tar.gz",
    "./samples/frameworks/base/core/java/android/content" to "https://android.googlesource.com/platform/frameworks/base/+archive/refs/heads/master/core/java/android/content.tar.gz",
    "./samples/frameworks/base/tests/appwidgets/AppWidgetHostTest/src/com/android/tests/appwidgethost" to "https://android.googlesource.com/platform/frameworks/base/+archive/refs/heads/master/tests/appwidgets/AppWidgetHostTest/src/com/android/tests/appwidgethost.tar.gz"
    )
