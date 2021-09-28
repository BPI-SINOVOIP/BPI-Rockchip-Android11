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
 * limitations under the License
 */

package android.theme.app;

/**
 * A class to encapsulate information about a theme.
 */
class ThemeInfo {
    public static final int HOLO = 0;
    public static final int MATERIAL = 1;

    public final int spec;
    public final int id;
    public final int apiLevel;
    public final String name;

    ThemeInfo(int spec, int id, int apiLevel, String name) {
        this.spec = spec;
        this.id = id;
        this.apiLevel = apiLevel;
        this.name = name;
    }
}
