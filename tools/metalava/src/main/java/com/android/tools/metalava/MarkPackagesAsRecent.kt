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

package com.android.tools.metalava

import com.android.tools.metalava.doclava1.ApiPredicate
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.visitors.ApiVisitor

/**
 * Iterates over APIs matching a certain filter, and calls markRecent on each.
 *
 * If you want to compare a previous API and a current API and migrate only the APIs that changed
 * between the two, then see {@link com.android.tools.metalava.NullnessMigration} instead.
 */
class MarkPackagesAsRecent(val filter: PackageFilter) : ApiVisitor(
    filterEmit = ApiPredicate(ignoreShown = true),
    filterReference = ApiPredicate(ignoreShown = true),
    includeEmptyOuterClasses = true
) {
    override fun include(cls: ClassItem): Boolean {
        return filter.matches(cls.containingPackage())
    }

    override fun visitItem(item: Item) {
        item.markRecent()
    }
}
