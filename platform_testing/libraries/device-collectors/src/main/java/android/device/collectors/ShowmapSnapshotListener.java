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

package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import com.android.helpers.ShowmapSnapshotHelper;
import java.util.HashMap;
import java.util.Map;

/**
 * A {@link ShowmapSnapshotListener} that takes a snapshot of memory sizes for the list of
 * specified processes.
 *
 * Options:
 * -e process-names [processNames] : a comma-separated list of processes
 * -e drop-cache [pagecache | slab | all] : drop cache flag
 * -e test-output-dir [path] : path to the output directory
 * -e metric-index [rss:2,pss:3,privatedirty:7] : memory metric name corresponding
 *  to index in the showmap output.
 */
@OptionClass(alias = "showmapsnapshot-collector")
public class ShowmapSnapshotListener extends BaseCollectionListener<String> {
  private static final String TAG = ShowmapSnapshotListener.class.getSimpleName();
  private static final String DEFAULT_OUTPUT_DIR = "/sdcard/test_results";

  @VisibleForTesting static final String PROCESS_SEPARATOR = ",";
  @VisibleForTesting static final String PROCESS_NAMES_KEY = "process-names";
  @VisibleForTesting static final String METRIC_NAME_INDEX = "metric-name-index";
  @VisibleForTesting static final String DROP_CACHE_KEY = "drop-cache";
  @VisibleForTesting static final String OUTPUT_DIR_KEY = "test-output-dir";

  private ShowmapSnapshotHelper mShowmapSnapshotHelper = new ShowmapSnapshotHelper();
  private final Map<String, Integer> dropCacheValues = new HashMap<String, Integer>() {
    {
      put("pagecache", 1);
      put("slab", 2);
      put("all", 3);
    }
  };

  // Sample output
  // -------- -------- -------- -------- -------- -------- -------- -------- -------- ------ --
  // virtual                     shared   shared  private  private
  //  size      RSS      PSS    clean    dirty    clean    dirty     swap  swapPSS flags object
  // ------- -------- -------- -------- -------- -------- -------- -------- -------- ----- ---
  // 10810272     5400     1585     3800      168      264     1168        0        0      TOTAL

  // Default to collect rss, pss and private dirty.
  private String mMemoryMetricNameIndex = "rss:1,pss:2,privatedirty:6";

  public ShowmapSnapshotListener() {
    createHelperInstance(mShowmapSnapshotHelper);
  }

  /**
   * Constructor to simulate receiving the instrumentation arguments. Should not be used except
   * for testing.
   */
  @VisibleForTesting
  public ShowmapSnapshotListener(Bundle args, ShowmapSnapshotHelper helper) {
    super(args, helper);
    mShowmapSnapshotHelper = helper;
    createHelperInstance(mShowmapSnapshotHelper);
  }

  /**
   * Adds the options for showmap snapshot collector.
   */
  @Override
  public void setupAdditionalArgs() {
    Bundle args = getArgsBundle();
    String testOutputDir = args.getString(OUTPUT_DIR_KEY, DEFAULT_OUTPUT_DIR);
    // Collect for all processes if process list is empty or null.
    String procsString = args.getString(PROCESS_NAMES_KEY);

    // Metric name and corresponding index in the output of showmap summary.
    String metricNameIndexArg = args.getString(METRIC_NAME_INDEX);
    if (metricNameIndexArg != null && !metricNameIndexArg.isEmpty()) {
        mMemoryMetricNameIndex = metricNameIndexArg;
    }
    mShowmapSnapshotHelper.setMetricNameIndex(mMemoryMetricNameIndex);

    String[] procs = null;
    if (procsString == null || procsString.isEmpty()) {
      mShowmapSnapshotHelper.setAllProcesses();
    } else {
      procs = procsString.split(PROCESS_SEPARATOR);
    }


    mShowmapSnapshotHelper.setUp(testOutputDir, procs);

    String dropCacheValue = args.getString(DROP_CACHE_KEY);
    if (dropCacheValue != null) {
      if (dropCacheValues.containsKey(dropCacheValue)) {
        mShowmapSnapshotHelper.setDropCacheOption(dropCacheValues.get(dropCacheValue));
      } else {
        Log.e(TAG, "Value for \"" + DROP_CACHE_KEY + "\" parameter is invalid");
        return;
      }
    }
  }
}
