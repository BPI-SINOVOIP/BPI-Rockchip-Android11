/*
 * Copyright 2018, OpenCensus Authors
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

package io.opencensus.metrics;

import io.opencensus.common.ToLongFunction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link DerivedLongGauge}. */
// TODO(mayurkale): Add more tests, once DerivedLongGauge plugs-in into the registry.
@RunWith(JUnit4.class)
public class DerivedLongGaugeTest {
  @Rule public ExpectedException thrown = ExpectedException.none();

  private static final String NAME = "name";
  private static final String DESCRIPTION = "description";
  private static final String UNIT = "1";
  private static final List<LabelKey> LABEL_KEY =
      Collections.singletonList(LabelKey.create("key", "key description"));
  private static final List<LabelValue> LABEL_VALUES =
      Collections.singletonList(LabelValue.create("value"));
  private static final List<LabelValue> EMPTY_LABEL_VALUES = new ArrayList<LabelValue>();

  private final DerivedLongGauge derivedLongGauge =
      DerivedLongGauge.newNoopDerivedLongGauge(NAME, DESCRIPTION, UNIT, LABEL_KEY);
  private static final ToLongFunction<Object> longFunction =
      new ToLongFunction<Object>() {
        @Override
        public long applyAsLong(Object value) {
          return 5;
        }
      };

  @Test
  public void noopCreateTimeSeries_WithNullLabelValues() {
    thrown.expect(NullPointerException.class);
    thrown.expectMessage("labelValues");
    derivedLongGauge.createTimeSeries(null, null, longFunction);
  }

  @Test
  public void noopCreateTimeSeries_WithNullElement() {
    List<LabelValue> labelValues = Collections.singletonList(null);
    thrown.expect(NullPointerException.class);
    thrown.expectMessage("labelValue element should not be null.");
    derivedLongGauge.createTimeSeries(labelValues, null, longFunction);
  }

  @Test
  public void noopCreateTimeSeries_WithInvalidLabelSize() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("Incorrect number of labels.");
    derivedLongGauge.createTimeSeries(EMPTY_LABEL_VALUES, null, longFunction);
  }

  @Test
  public void createTimeSeries_WithNullFunction() {
    thrown.expect(NullPointerException.class);
    thrown.expectMessage("function");
    derivedLongGauge.createTimeSeries(LABEL_VALUES, null, null);
  }

  @Test
  public void noopRemoveTimeSeries_WithNullLabelValues() {
    thrown.expect(NullPointerException.class);
    thrown.expectMessage("labelValues");
    derivedLongGauge.removeTimeSeries(null);
  }
}
