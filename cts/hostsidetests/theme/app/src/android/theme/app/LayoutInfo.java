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

import android.theme.app.modifiers.AbstractLayoutModifier;

/**
 * A class to encapsulate information about a layout.
 */
class LayoutInfo {
    public final int id;
    public final String name;
    public final AbstractLayoutModifier modifier;

    LayoutInfo(int id, String name) {
        this(id, name, null);
    }

    LayoutInfo(int id, String name, AbstractLayoutModifier modifier) {
        this.id = id;
        this.name = name;
        this.modifier = modifier;
    }
}
