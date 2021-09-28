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

package android.content.res.loader.cts

import android.content.res.loader.ResourcesLoader
import android.util.TypedValue
import org.junit.Assert.assertEquals
import org.junit.Test

class ResourcesLoaderFallthroughTest : ResourcesLoaderTestBase() {

    @Test
    fun loadersFallthrough() {
        val p1 = PROVIDER_ONE.openProvider(DataType.APK_DISK_FD)
        val p2 = PROVIDER_TWO.openProvider(DataType.APK_DISK_FD)
        val p3 = PROVIDER_THREE.openProvider(DataType.APK_DISK_FD,
                MemoryAssetsProvider().addLoadAssetFdResult("assets/one_and_three_only.txt",
                        "Three"))
        val p4 = PROVIDER_FOUR.openProvider(DataType.APK_DISK_FD)

        val actual = query(mapOf(
                "getString" to { res ->
                    res.getString(0x7f0400ff /* R.string.two_and_four_only */)
                },
                "openAsset" to { res ->
                    res.assets.open("one_and_three_only.txt").reader().readText()
                },
                "openAssetFd" to { res ->
                    res.assets.openFd("one_and_three_only.txt").readText()
                }))

        val loader = ResourcesLoader()
        val loader2 = ResourcesLoader()
        resources.addLoaders(loader, loader2)
        assertEquals(mapToString(mapOf("getString" to "NotFoundException",
                "openAsset" to "FileNotFoundException",
                "openAssetFd" to "FileNotFoundException")), actual(resources))

        loader.addProvider(p1)
        assertEquals(mapToString(mapOf("getString" to "NotFoundException",
                "openAsset" to "One",
                "openAssetFd" to "One")), actual(resources))

        loader.addProvider(p2)
        assertEquals(mapToString(mapOf("getString" to "Two",
                "openAsset" to "One",
                "openAssetFd" to "One")), actual(resources))

        loader2.addProvider(p3)
        assertEquals(mapToString(mapOf("getString" to "Two",
                "openAsset" to "Three",
                "openAssetFd" to "Three")), actual(resources))

        loader2.addProvider(p4)
        assertEquals(mapToString(mapOf("getString" to "Four",
                "openAsset" to "Three",
                "openAssetFd" to "Three")), actual(resources))
    }

    @Test
    fun cookieBased() {
        val p1 = PROVIDER_ONE.openProvider(DataType.APK_DISK_FD)
        val p2 = PROVIDER_TWO.openProvider(DataType.APK_DISK_FD)
        val p3 = PROVIDER_THREE.openProvider(DataType.APK_DISK_FD)

        val loader = ResourcesLoader()
                .apply { providers = listOf(p1, p2, p3) }
                .also { resources.addLoaders(it)}

        // Retrieve the cookie of the second provider to check that cookie based APIs work as
        // expected with loaders.
        fun getCookie() : Int {
                return TypedValue()
                    .apply { resources.getValue(0x7f0400ff /* R.string.two_and_four_only */, this,
                            true /* resolveRefs */) }
                    .assetCookie
        }

        val actual = query(mapOf(
                "layout" to { res ->
                    res.assets.openXmlResourceParser(getCookie(), "res/layout/layout.xml")
                            .advanceToRoot().name
                },
                "openAsset" to { res ->
                    res.assets.openNonAsset(getCookie(), "assets/asset.txt").reader().readText()
                },
                "openAssetFd" to { res ->
                    res.assets.openNonAssetFd(getCookie(), "assets/asset.txt").readText()
                }))

        assertEquals(mapToString(mapOf(
                "layout" to "LinearLayout",
                "openAsset" to "Two",
                "openAssetFd" to "Two")), actual(resources))

        val p22 = PROVIDER_TWO.openProvider(DataType.APK_DISK_FD,
                MemoryAssetsProvider().addLoadAssetFdResult("assets/asset.txt", "AssetsTwo"))
        loader.providers = listOf(p1, p22, p3)

        assertEquals(mapToString(mapOf(
                "layout" to "LinearLayout",
                "openAsset" to "AssetsTwo",
                "openAssetFd" to "AssetsTwo")), actual(resources))
    }
}