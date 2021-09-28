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
 * limitations under the License
 */

package android.service.resolver.cts;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import android.os.Parcel;
import android.service.resolver.ResolverTarget;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests {@link ResolverTarget}.
 */
@RunWith(AndroidJUnit4.class)
public class ResolverTargetTest {

    @Test
    public void sanityCheckDataConsistency() throws Exception {
        ResolverTarget target = new ResolverTarget();

        target.setChooserScore(1.0f);
        assertThat(target.getChooserScore(), is(1.0f));

        target.setLaunchScore(0.5f);
        assertThat(target.getLaunchScore(), is(0.5f));

        target.setRecencyScore(0.3f);
        assertThat(target.getRecencyScore(), is(0.3f));

        target.setSelectProbability(0.2f);
        assertThat(target.getSelectProbability(), is(0.2f));

        target.setTimeSpentScore(0.1f);
        assertThat(target.getTimeSpentScore(), is(0.1f));
    }

    @Test
    public void sanityCheckParcelability() throws Exception {
        ResolverTarget target = new ResolverTarget();

        target.setChooserScore(1.0f);
        target.setLaunchScore(0.5f);
        target.setRecencyScore(0.3f);
        target.setSelectProbability(0.2f);
        target.setTimeSpentScore(0.1f);

        Parcel parcel = Parcel.obtain();
        target.writeToParcel(parcel, 0 /*flags*/);
        parcel.setDataPosition(0);
        ResolverTarget fromParcel = ResolverTarget.CREATOR.createFromParcel(parcel);

        assertThat(fromParcel.getChooserScore(), is(1.0f));
        assertThat(fromParcel.getLaunchScore(), is(0.5f));
        assertThat(fromParcel.getRecencyScore(), is(0.3f));
        assertThat(fromParcel.getSelectProbability(), is(0.2f));
        assertThat(fromParcel.getTimeSpentScore(), is(0.1f));
    }
}
