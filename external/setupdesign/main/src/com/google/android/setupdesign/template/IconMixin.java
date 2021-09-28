/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.google.android.setupdesign.template;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import com.google.android.setupcompat.internal.TemplateLayout;
import com.google.android.setupcompat.template.Mixin;
import com.google.android.setupdesign.R;
import com.google.android.setupdesign.util.HeaderAreaStyler;
import com.google.android.setupdesign.util.PartnerStyleHelper;

/**
 * A {@link com.google.android.setupcompat.template.Mixin} for setting an icon on the template
 * layout.
 */
public class IconMixin implements Mixin {

  private final TemplateLayout templateLayout;

  private final int originalHeight;
  private final ImageView.ScaleType originalScaleType;

  /**
   * @param layout The template layout that this Mixin is a part of.
   * @param attrs XML attributes given to the layout.
   * @param defStyleAttr The default style attribute as given to the constructor of the layout.
   */
  public IconMixin(TemplateLayout layout, AttributeSet attrs, int defStyleAttr) {
    templateLayout = layout;
    final Context context = layout.getContext();

    ImageView iconView = getView();
    if (iconView != null) {
      LayoutParams layoutParams = iconView.getLayoutParams();
      originalHeight = layoutParams.height;
      originalScaleType = iconView.getScaleType();
    } else {
      originalHeight = 0;
      originalScaleType = null;
    }

    final TypedArray a =
        context.obtainStyledAttributes(attrs, R.styleable.SudIconMixin, defStyleAttr, 0);

    final @DrawableRes int icon = a.getResourceId(R.styleable.SudIconMixin_android_icon, 0);
    if (icon != 0) {
      setIcon(icon);
    }

    final boolean upscaleIcon = a.getBoolean(R.styleable.SudIconMixin_sudUpscaleIcon, false);
    setUpscaleIcon(upscaleIcon);

    final @ColorInt int iconTint =
        a.getColor(R.styleable.SudIconMixin_sudIconTint, Color.TRANSPARENT);
    if (iconTint != Color.TRANSPARENT) {
      setIconTint(iconTint);
    }

    a.recycle();
  }

  /**
   * Tries to apply the partner customization to the header icon if the layout of this {@link
   * IconMixin} is set to apply partner heavy theme resource.
   */
  public void tryApplyPartnerCustomizationStyle() {
    if (!PartnerStyleHelper.isPartnerHeavyThemeLayout(templateLayout)) {
      return;
    }

    ImageView iconImage = templateLayout.findManagedViewById(R.id.sud_layout_icon);
    if (iconImage != null) {
      HeaderAreaStyler.applyPartnerCustomizationIconStyle(iconImage);
    }
  }

  /**
   * Sets the icon on this layout. The icon can also be set in XML using {@code android:icon}.
   *
   * @param icon A drawable icon.
   */
  public void setIcon(Drawable icon) {
    final ImageView iconView = getView();
    if (iconView != null) {
      iconView.setImageDrawable(icon);
      iconView.setVisibility(icon != null ? View.VISIBLE : View.GONE);
    }
  }

  /**
   * Sets the icon on this layout. The icon can also be set in XML using {@code android:icon}.
   *
   * @param icon A drawable icon resource.
   */
  public void setIcon(@DrawableRes int icon) {
    final ImageView iconView = getView();
    if (iconView != null) {
      // Note: setImageResource on the ImageView is overridden in AppCompatImageView for
      // support lib users, which enables vector drawable compat to work on versions pre-L.
      iconView.setImageResource(icon);
      iconView.setVisibility(icon != 0 ? View.VISIBLE : View.GONE);
    }
  }

  /** @return The icon previously set in {@link #setIcon(Drawable)} or {@code android:icon} */
  public Drawable getIcon() {
    final ImageView iconView = getView();
    return iconView != null ? iconView.getDrawable() : null;
  }

  /** Forces the icon view to be as big as desired in the style. */
  public void setUpscaleIcon(boolean shouldUpscaleIcon) {
    final int maxHeight;
    final ImageView iconView = getView();
    if (iconView != null) {
      LayoutParams layoutParams = iconView.getLayoutParams();
      if (VERSION.SDK_INT >= VERSION_CODES.JELLY_BEAN) {
        maxHeight = iconView.getMaxHeight();
      } else {
        maxHeight = (int) iconView.getResources().getDimension(R.dimen.sud_glif_icon_max_height);
      }
      layoutParams.height = shouldUpscaleIcon ? maxHeight : originalHeight;
      iconView.setLayoutParams(layoutParams);
      iconView.setScaleType(shouldUpscaleIcon ? ImageView.ScaleType.FIT_CENTER : originalScaleType);
    }
  }

  /** Tints the icon on this layout to the given color. */
  public void setIconTint(@ColorInt int tint) {
    final ImageView iconView = getView();
    if (iconView != null) {
      iconView.setColorFilter(tint);
    }
  }

  /** Sets the content description of the icon view */
  public void setContentDescription(CharSequence description) {
    final ImageView iconView = getView();
    if (iconView != null) {
      iconView.setContentDescription(description);
    }
  }

  /** @return The content description of the icon view */
  public CharSequence getContentDescription() {
    final ImageView iconView = getView();
    return iconView != null ? iconView.getContentDescription() : null;
  }

  /** Sets the visibiltiy of the icon view */
  public void setVisibility(int visibility) {
    final ImageView iconView = getView();
    if (iconView != null) {
      iconView.setVisibility(visibility);
    }
  }

  /** @return The ImageView responsible for displaying the icon. */
  protected ImageView getView() {
    return (ImageView) templateLayout.findManagedViewById(R.id.sud_layout_icon);
  }
}
