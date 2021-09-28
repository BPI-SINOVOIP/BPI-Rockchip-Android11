/*
 * Copyright 2017 Google Inc.
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

package trebuchet.model

import org.junit.Test
import trebuchet.model.fragments.SliceGroupBuilder
import kotlin.test.assertEquals

class SliceGroupBuilderTest {
    @Test
    fun testSimpleBuild() {
        val group = SliceGroupBuilder()
        group.beginSlice {
            it.startTime = 1.0
            it.name = "first"
        }
        val slice = group.endSlice {
            it.endTime = 2.0
        }!!
        assertEquals(1.0, slice.startTime)
        assertEquals(2.0, slice.endTime)
        assertEquals("first", slice.name)
        assertEquals(slice, group.slices.first())
    }

    @Test fun testNestedBuild() {
        val group = SliceGroupBuilder()
        group.beginSlice {
            it.startTime = 1.0
            it.name = "first"
        }
        group.beginSlice {
            it.startTime = 1.1
            it.name = "nested"
        }
        val child = group.endSlice {
            it.endTime = 1.2
        }!!
        assertEquals(1.1, child.startTime)
        assertEquals(1.2, child.endTime)
        assertEquals("nested", child.name)
        assertEquals(0, group.slices.size)
        val slice = group.endSlice {
            it.endTime = 2.0
        }!!
        assertEquals(1.0, slice.startTime)
        assertEquals(2.0, slice.endTime)
        assertEquals("first", slice.name)
        assertEquals(1, group.slices.size)
        assertEquals(slice, group.slices.first())
        assertEquals(1, slice.children.size)
        assertEquals(child, slice.children.first())
        assertEquals(0, child.children.size)
    }
}