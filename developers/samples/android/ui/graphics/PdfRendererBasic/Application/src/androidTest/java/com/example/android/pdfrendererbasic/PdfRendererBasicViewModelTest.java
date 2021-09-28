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
 * limitations under the License.
 */

package com.example.android.pdfrendererbasic;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.arch.core.executor.testing.InstantTaskExecutorRule;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import static com.google.common.truth.Truth.assertThat;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class PdfRendererBasicViewModelTest {

    @Rule
    public final InstantTaskExecutorRule rule = new InstantTaskExecutorRule();

    private PdfRendererBasicViewModel mViewModel = new PdfRendererBasicViewModel(
            ApplicationProvider.getApplicationContext(), true);

    @Test
    public void allPages() {
        assertThat(mViewModel).isNotNull();
        final PdfRendererBasicViewModel.PageInfo firstInfo = mViewModel.getPageInfo().getValue();
        assertThat(firstInfo).isNotNull();
        assertThat(firstInfo.index).isEqualTo(0);
        assertThat(firstInfo.count).isEqualTo(10);
        assertThat(mViewModel.getPreviousEnabled().getValue()).isFalse();
        assertThat(mViewModel.getNextEnabled().getValue()).isTrue();
        assertThat(mViewModel.getPageBitmap().getValue()).isNotNull();
        for (int i = 0; i < 9; i++) {
            mViewModel.showNext();
        }
        final PdfRendererBasicViewModel.PageInfo lastInfo = mViewModel.getPageInfo().getValue();
        assertThat(lastInfo).isNotNull();
        assertThat(lastInfo.index).isEqualTo(9);
        assertThat(lastInfo.count).isEqualTo(10);
        assertThat(mViewModel.getPreviousEnabled().getValue()).isTrue();
        assertThat(mViewModel.getNextEnabled().getValue()).isFalse();
        assertThat(mViewModel.getPageBitmap().getValue()).isNotNull();
    }

}
