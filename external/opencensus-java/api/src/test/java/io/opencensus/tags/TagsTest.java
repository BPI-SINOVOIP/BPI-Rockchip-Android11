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

package io.opencensus.tags;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link Tags}. */
@RunWith(JUnit4.class)
public class TagsTest {
  @Rule public ExpectedException thrown = ExpectedException.none();

  @Test
  public void loadTagsComponent_UsesProvidedClassLoader() {
    final RuntimeException toThrow = new RuntimeException("UseClassLoader");
    thrown.expect(RuntimeException.class);
    thrown.expectMessage("UseClassLoader");
    Tags.loadTagsComponent(
        new ClassLoader() {
          @Override
          public Class<?> loadClass(String name) {
            throw toThrow;
          }
        });
  }

  @Test
  public void loadTagsComponent_IgnoresMissingClasses() {
    ClassLoader classLoader =
        new ClassLoader() {
          @Override
          public Class<?> loadClass(String name) throws ClassNotFoundException {
            throw new ClassNotFoundException();
          }
        };
    assertThat(Tags.loadTagsComponent(classLoader).getClass().getName())
        .isEqualTo("io.opencensus.tags.NoopTags$NoopTagsComponent");
  }

  // There is only one test that modifies tagging state in the Tags class, since the state is
  // global, and it could affect other tests. NoopTagsTest has more thorough tests for tagging
  // state.
  @Test
  @SuppressWarnings("deprecation")
  public void testState() {
    // Test that setState ignores its input.
    Tags.setState(TaggingState.ENABLED);
    assertThat(Tags.getState()).isEqualTo(TaggingState.DISABLED);

    // Test that setState cannot be called after getState.
    thrown.expect(IllegalStateException.class);
    thrown.expectMessage("State was already read, cannot set state.");
    Tags.setState(TaggingState.ENABLED);
  }

  @Test
  public void defaultTagger() {
    assertThat(Tags.getTagger()).isEqualTo(NoopTags.getNoopTagger());
  }

  @Test
  public void defaultTagContextSerializer() {
    assertThat(Tags.getTagPropagationComponent())
        .isEqualTo(NoopTags.getNoopTagPropagationComponent());
  }
}
