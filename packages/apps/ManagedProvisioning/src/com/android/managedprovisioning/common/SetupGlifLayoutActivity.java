/*
 * Copyright 2016, The Android Open Source Project
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

package com.android.managedprovisioning.common;

import android.annotation.Nullable;
import android.content.res.ColorStateList;
import android.content.res.Resources.Theme;
import android.os.Bundle;
import android.sysprop.SetupWizardProperties;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.model.CustomizationParams;
import com.google.android.setupdesign.GlifLayout;
import com.google.android.setupdesign.util.ThemeResolver;


/**
 * Base class for setting up the layout.
 */
public abstract class SetupGlifLayoutActivity extends SetupLayoutActivity {

    /**
     * Number of characters in the header needed before adding an extra line of text.
     */
    private static final int CHAR_THRESHOLD_FOR_ADDITIONAL_LINE = 70;

    public SetupGlifLayoutActivity() {
        super();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setDefaultTheme();
    }

    @VisibleForTesting
    protected SetupGlifLayoutActivity(Utils utils) {
        super(utils);
    }

    @Override
    protected void onApplyThemeResource(Theme theme, int resid, boolean first) {
        theme.applyStyle(R.style.SetupWizardPartnerResource, true);
        super.onApplyThemeResource(theme, resid, first);
    }

    protected void initializeLayoutParams(int layoutResourceId, @Nullable Integer headerResourceId,
            CustomizationParams params) {
        setContentView(layoutResourceId);
        GlifLayout layout = findViewById(R.id.setup_wizard_layout);

        // ManagedProvisioning's customization has prioritization than stencil theme currently. If
        // there is no status bar color customized by ManagedProvisioning, it can apply status bar
        // color from stencil theme.
        if (!params.useSetupStatusBarColor) {
            setStatusBarColor(params.statusBarColor);
        }
        layout.setPrimaryColor(ColorStateList.valueOf(params.mainColor));

        if (headerResourceId != null) {
            layout.setHeaderText(headerResourceId);
            layout.setHeaderColor(
                    getResources().getColorStateList(R.color.header_text_color, getTheme()));
        }

        TextView header = findViewById(R.id.suc_layout_title);
        if (header != null) {
            header.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                }

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {
                }

                @Override
                public void afterTextChanged(Editable s) {
                    adjustHeaderMaxLines();
                }
            });

            adjustHeaderMaxLines();
        }

        layout.setIcon(LogoUtils.getOrganisationLogo(this, params.mainColor));
    }

    private void adjustHeaderMaxLines() {
        TextView header = findViewById(R.id.suc_layout_title);
        int maxLines = 3;
        if (header.getText().length() > CHAR_THRESHOLD_FOR_ADDITIONAL_LINE) {
            maxLines++;
        }
        if (header.getMaxLines() != maxLines) {
            header.setMaxLines(maxLines);
        }
    }

    private void setDefaultTheme() {
        setTheme(new ThemeResolver.Builder(ThemeResolver.getDefault())
            .setDefaultTheme(R.style.SudThemeGlifV3_Light)
            .setUseDayNight(false)
            .build()
            .resolve(SetupWizardProperties.theme().orElse("")));
    }

}
