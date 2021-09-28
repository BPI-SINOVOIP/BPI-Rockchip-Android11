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

import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.visitors.VisibleItemVisitor

/**
 * A {@link VisibleItemVisitor} that applies a regex replacement to the documentation of
 * {@link Item}s from the given packages and their sub-packages.
 */
class DocReplacement(
    private val packageNames: List<String>,
    private val regex: Regex,
    private val replacement: String
) : VisibleItemVisitor() {

    private fun appliesToPackage(packageItem: PackageItem): Boolean {
        val fqn = packageItem.qualifiedName()
        return packageNames.any {
                pkg -> fqn.startsWith(pkg) && (fqn.length == pkg.length || fqn[pkg.length] == '.')
        }
    }

    override fun visitItem(item: Item) {
        val doc = item.documentation
        if (doc.isBlank()) {
            return
        }
        val pkg = item.containingPackage(strict = false)
        if (pkg != null && appliesToPackage(pkg)) {
            val new = doc.replace(regex, replacement)
            if (new != doc) {
                item.documentation = new
            }
        }
    }
}