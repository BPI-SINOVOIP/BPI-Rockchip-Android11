/*
 * Copyright 2019 The Android Open Source Project
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

package com.google.android.setupcompat.internal;

import com.google.android.setupcompat.partnerconfig.PartnerConfig;
import com.google.android.setupcompat.template.FooterButton;

/** Keep the partner configuration of a footer button. Used when the button is inflated. */
public class FooterButtonPartnerConfig {
  private final PartnerConfig buttonBackgroundConfig;
  private final PartnerConfig buttonDisableAlphaConfig;
  private final PartnerConfig buttonDisableBackgroundConfig;
  private final PartnerConfig buttonIconConfig;
  private final PartnerConfig buttonTextColorConfig;
  private final PartnerConfig buttonTextSizeConfig;
  private final PartnerConfig buttonTextTypeFaceConfig;
  private final PartnerConfig buttonRadiusConfig;
  private final PartnerConfig buttonRippleColorAlphaConfig;
  private final int partnerTheme;

  private FooterButtonPartnerConfig(
      int partnerTheme,
      PartnerConfig buttonBackgroundConfig,
      PartnerConfig buttonDisableAlphaConfig,
      PartnerConfig buttonDisableBackgroundConfig,
      PartnerConfig buttonIconConfig,
      PartnerConfig buttonTextColorConfig,
      PartnerConfig buttonTextSizeConfig,
      PartnerConfig buttonTextTypeFaceConfig,
      PartnerConfig buttonRadiusConfig,
      PartnerConfig buttonRippleColorAlphaConfig) {
    this.partnerTheme = partnerTheme;

    this.buttonTextColorConfig = buttonTextColorConfig;
    this.buttonTextSizeConfig = buttonTextSizeConfig;
    this.buttonTextTypeFaceConfig = buttonTextTypeFaceConfig;
    this.buttonBackgroundConfig = buttonBackgroundConfig;
    this.buttonDisableAlphaConfig = buttonDisableAlphaConfig;
    this.buttonDisableBackgroundConfig = buttonDisableBackgroundConfig;
    this.buttonRadiusConfig = buttonRadiusConfig;
    this.buttonIconConfig = buttonIconConfig;
    this.buttonRippleColorAlphaConfig = buttonRippleColorAlphaConfig;
  }

  public int getPartnerTheme() {
    return partnerTheme;
  }

  public PartnerConfig getButtonBackgroundConfig() {
    return buttonBackgroundConfig;
  }

  public PartnerConfig getButtonDisableAlphaConfig() {
    return buttonDisableAlphaConfig;
  }

  public PartnerConfig getButtonDisableBackgroundConfig() {
    return buttonDisableBackgroundConfig;
  }

  public PartnerConfig getButtonIconConfig() {
    return buttonIconConfig;
  }

  public PartnerConfig getButtonTextColorConfig() {
    return buttonTextColorConfig;
  }

  public PartnerConfig getButtonTextSizeConfig() {
    return buttonTextSizeConfig;
  }

  public PartnerConfig getButtonTextTypeFaceConfig() {
    return buttonTextTypeFaceConfig;
  }

  public PartnerConfig getButtonRadiusConfig() {
    return buttonRadiusConfig;
  }

  public PartnerConfig getButtonRippleColorAlphaConfig() {
    return buttonRippleColorAlphaConfig;
  }

  /** Builder class for constructing {@code FooterButtonPartnerConfig} objects. */
  public static class Builder {
    private final FooterButton footerButton;
    private PartnerConfig buttonBackgroundConfig = null;
    private PartnerConfig buttonDisableAlphaConfig = null;
    private PartnerConfig buttonDisableBackgroundConfig = null;
    private PartnerConfig buttonIconConfig = null;
    private PartnerConfig buttonTextColorConfig = null;
    private PartnerConfig buttonTextSizeConfig = null;
    private PartnerConfig buttonTextTypeFaceConfig = null;
    private PartnerConfig buttonRadiusConfig = null;
    private PartnerConfig buttonRippleColorAlphaConfig = null;
    private int partnerTheme;

    public Builder(FooterButton footerButton) {
      this.footerButton = footerButton;
      // default partnerTheme should be the same as footerButton.getTheme();
      this.partnerTheme = this.footerButton.getTheme();
    }

    public Builder setButtonBackgroundConfig(PartnerConfig buttonBackgroundConfig) {
      this.buttonBackgroundConfig = buttonBackgroundConfig;
      return this;
    }

    public Builder setButtonDisableAlphaConfig(PartnerConfig buttonDisableAlphaConfig) {
      this.buttonDisableAlphaConfig = buttonDisableAlphaConfig;
      return this;
    }

    public Builder setButtonDisableBackgroundConfig(PartnerConfig buttonDisableBackgroundConfig) {
      this.buttonDisableBackgroundConfig = buttonDisableBackgroundConfig;
      return this;
    }

    public Builder setButtonIconConfig(PartnerConfig buttonIconConfig) {
      this.buttonIconConfig = buttonIconConfig;
      return this;
    }

    public Builder setTextColorConfig(PartnerConfig buttonTextColorConfig) {
      this.buttonTextColorConfig = buttonTextColorConfig;
      return this;
    }

    public Builder setTextSizeConfig(PartnerConfig buttonTextSizeConfig) {
      this.buttonTextSizeConfig = buttonTextSizeConfig;
      return this;
    }

    public Builder setTextTypeFaceConfig(PartnerConfig buttonTextTypeFaceConfig) {
      this.buttonTextTypeFaceConfig = buttonTextTypeFaceConfig;
      return this;
    }

    public Builder setButtonRadiusConfig(PartnerConfig buttonRadiusConfig) {
      this.buttonRadiusConfig = buttonRadiusConfig;
      return this;
    }

    public Builder setButtonRippleColorAlphaConfig(PartnerConfig buttonRippleColorAlphaConfig) {
      this.buttonRippleColorAlphaConfig = buttonRippleColorAlphaConfig;
      return this;
    }

    public Builder setPartnerTheme(int partnerTheme) {
      this.partnerTheme = partnerTheme;
      return this;
    }

    public FooterButtonPartnerConfig build() {
      return new FooterButtonPartnerConfig(
          partnerTheme,
          buttonBackgroundConfig,
          buttonDisableAlphaConfig,
          buttonDisableBackgroundConfig,
          buttonIconConfig,
          buttonTextColorConfig,
          buttonTextSizeConfig,
          buttonTextTypeFaceConfig,
          buttonRadiusConfig,
          buttonRippleColorAlphaConfig);
    }
  }
}
