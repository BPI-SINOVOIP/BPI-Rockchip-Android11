/*
 * Copyright 2017, OpenCensus Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.opencensus.implcore.tags;

import com.google.common.collect.Lists;
import io.opencensus.tags.InternalUtils;
import io.opencensus.tags.Tag;
import io.opencensus.tags.TagContext;
import java.util.Collection;

/** Test utilities for tagging. */
public class TagsTestUtil {
  private TagsTestUtil() {}

  /** Returns a collection of all tags in a {@link TagContext}. */
  public static Collection<Tag> tagContextToList(TagContext tags) {
    return Lists.newArrayList(InternalUtils.getTags(tags));
  }
}
