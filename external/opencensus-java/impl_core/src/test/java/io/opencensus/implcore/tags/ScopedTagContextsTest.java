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

import static com.google.common.truth.Truth.assertThat;
import static io.opencensus.implcore.tags.TagsTestUtil.tagContextToList;

import io.opencensus.common.Scope;
import io.opencensus.implcore.internal.CurrentState;
import io.opencensus.implcore.internal.CurrentState.State;
import io.opencensus.tags.Tag;
import io.opencensus.tags.TagContext;
import io.opencensus.tags.TagKey;
import io.opencensus.tags.TagValue;
import io.opencensus.tags.Tagger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for the methods in {@link TaggerImpl} and {@link TagContextBuilderImpl} that interact
 * with the current {@link TagContext}.
 */
@RunWith(JUnit4.class)
public class ScopedTagContextsTest {
  private static final TagKey KEY_1 = TagKey.create("key 1");
  private static final TagKey KEY_2 = TagKey.create("key 2");

  private static final TagValue VALUE_1 = TagValue.create("value 1");
  private static final TagValue VALUE_2 = TagValue.create("value 2");

  private final Tagger tagger = new TaggerImpl(new CurrentState(State.ENABLED));

  @Test
  public void defaultTagContext() {
    TagContext defaultTagContext = tagger.getCurrentTagContext();
    assertThat(tagContextToList(defaultTagContext)).isEmpty();
    assertThat(defaultTagContext).isInstanceOf(TagContextImpl.class);
  }

  @Test
  public void withTagContext() {
    assertThat(tagContextToList(tagger.getCurrentTagContext())).isEmpty();
    TagContext scopedTags = tagger.emptyBuilder().put(KEY_1, VALUE_1).build();
    Scope scope = tagger.withTagContext(scopedTags);
    try {
      assertThat(tagger.getCurrentTagContext()).isSameAs(scopedTags);
    } finally {
      scope.close();
    }
    assertThat(tagContextToList(tagger.getCurrentTagContext())).isEmpty();
  }

  @Test
  public void createBuilderFromCurrentTags() {
    TagContext scopedTags = tagger.emptyBuilder().put(KEY_1, VALUE_1).build();
    Scope scope = tagger.withTagContext(scopedTags);
    try {
      TagContext newTags = tagger.currentBuilder().put(KEY_2, VALUE_2).build();
      assertThat(tagContextToList(newTags))
          .containsExactly(Tag.create(KEY_1, VALUE_1), Tag.create(KEY_2, VALUE_2));
      assertThat(tagger.getCurrentTagContext()).isSameAs(scopedTags);
    } finally {
      scope.close();
    }
  }

  @Test
  public void setCurrentTagsWithBuilder() {
    assertThat(tagContextToList(tagger.getCurrentTagContext())).isEmpty();
    Scope scope = tagger.emptyBuilder().put(KEY_1, VALUE_1).buildScoped();
    try {
      assertThat(tagContextToList(tagger.getCurrentTagContext()))
          .containsExactly(Tag.create(KEY_1, VALUE_1));
    } finally {
      scope.close();
    }
    assertThat(tagContextToList(tagger.getCurrentTagContext())).isEmpty();
  }

  @Test
  public void addToCurrentTagsWithBuilder() {
    TagContext scopedTags = tagger.emptyBuilder().put(KEY_1, VALUE_1).build();
    Scope scope1 = tagger.withTagContext(scopedTags);
    try {
      Scope scope2 = tagger.currentBuilder().put(KEY_2, VALUE_2).buildScoped();
      try {
        assertThat(tagContextToList(tagger.getCurrentTagContext()))
            .containsExactly(Tag.create(KEY_1, VALUE_1), Tag.create(KEY_2, VALUE_2));
      } finally {
        scope2.close();
      }
      assertThat(tagger.getCurrentTagContext()).isSameAs(scopedTags);
    } finally {
      scope1.close();
    }
  }
}
