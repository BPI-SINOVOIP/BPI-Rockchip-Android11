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
package android.sharesheet.cts;

import android.content.ComponentName;
import android.content.IntentFilter;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.service.chooser.ChooserTarget;
import android.service.chooser.ChooserTargetService;

import java.util.Arrays;
import java.util.List;

public class CtsSharesheetChooserTargetService extends ChooserTargetService {

	@Override
	public List<ChooserTarget> onGetChooserTargets(ComponentName componentName,
			IntentFilter intentFilter) {

    	ChooserTarget ct = new ChooserTarget(
    			getString(R.string.test_chooser_target_service_label),
				Icon.createWithResource(this, R.drawable.black_64x64),
				1f,
				componentName,
				new Bundle());

    	ChooserTarget[] ret = {ct};
    	return Arrays.asList(ret);
    }
}