package com.android.customization.testing;

import android.content.Context;

import com.android.customization.module.DefaultCustomizationPreferences;

import java.util.HashSet;
import java.util.Set;

/**
 * Test implementation of {@link DefaultCustomizationPreferences}.
 */
public class TestDefaultCustomizationPreferences extends DefaultCustomizationPreferences {

    private String mCustomThemes;
    private final Set<String> mTabVisited = new HashSet<>();

    public TestDefaultCustomizationPreferences(Context context) {
        super(context);
    }

    @Override
    public String getSerializedCustomThemes() {
        return mCustomThemes;
    }

    @Override
    public void storeCustomThemes(String serializedCustomThemes) {
        mCustomThemes = serializedCustomThemes;
    }

    @Override
    public boolean getTabVisited(String id) {
        return mTabVisited.contains(id);
    }

    @Override
    public void setTabVisited(String id) {
        mTabVisited.add(id);
    }
}
