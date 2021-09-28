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

package io.opencensus.implcore.trace.export;

import io.opencensus.implcore.trace.RecordEventsSpanImpl;
import io.opencensus.implcore.trace.internal.ConcurrentIntrusiveList;
import io.opencensus.trace.export.RunningSpanStore;
import io.opencensus.trace.export.SpanData;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import javax.annotation.concurrent.ThreadSafe;

/** In-process implementation of the {@link RunningSpanStore}. */
@ThreadSafe
public final class InProcessRunningSpanStoreImpl extends RunningSpanStoreImpl {
  private final ConcurrentIntrusiveList<RecordEventsSpanImpl> runningSpans;

  public InProcessRunningSpanStoreImpl() {
    runningSpans = new ConcurrentIntrusiveList<RecordEventsSpanImpl>();
  }

  @Override
  public void onStart(RecordEventsSpanImpl span) {
    runningSpans.addElement(span);
  }

  @Override
  public void onEnd(RecordEventsSpanImpl span) {
    runningSpans.removeElement(span);
  }

  @Override
  public Summary getSummary() {
    Collection<RecordEventsSpanImpl> allRunningSpans = runningSpans.getAll();
    Map<String, Integer> numSpansPerName = new HashMap<String, Integer>();
    for (RecordEventsSpanImpl span : allRunningSpans) {
      Integer prevValue = numSpansPerName.get(span.getName());
      numSpansPerName.put(span.getName(), prevValue != null ? prevValue + 1 : 1);
    }
    Map<String, PerSpanNameSummary> perSpanNameSummary = new HashMap<String, PerSpanNameSummary>();
    for (Map.Entry<String, Integer> it : numSpansPerName.entrySet()) {
      perSpanNameSummary.put(it.getKey(), PerSpanNameSummary.create(it.getValue()));
    }
    Summary summary = Summary.create(perSpanNameSummary);
    return summary;
  }

  @Override
  public Collection<SpanData> getRunningSpans(Filter filter) {
    Collection<RecordEventsSpanImpl> allRunningSpans = runningSpans.getAll();
    int maxSpansToReturn =
        filter.getMaxSpansToReturn() == 0 ? allRunningSpans.size() : filter.getMaxSpansToReturn();
    List<SpanData> ret = new ArrayList<SpanData>(maxSpansToReturn);
    for (RecordEventsSpanImpl span : allRunningSpans) {
      if (ret.size() == maxSpansToReturn) {
        break;
      }
      if (span.getName().equals(filter.getSpanName())) {
        ret.add(span.toSpanData());
      }
    }
    return ret;
  }
}
