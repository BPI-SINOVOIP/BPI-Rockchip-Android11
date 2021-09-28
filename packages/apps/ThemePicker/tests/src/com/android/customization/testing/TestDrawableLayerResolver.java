package com.android.customization.testing;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import com.android.wallpaper.module.DrawableLayerResolver;

/**
 * Test implementation of {@link DrawableLayerResolver}.
 */
public class TestDrawableLayerResolver implements DrawableLayerResolver {
    @Override
    public Drawable resolveLayer(LayerDrawable layerDrawable) {
        return layerDrawable.getDrawable(0);
    }
}
