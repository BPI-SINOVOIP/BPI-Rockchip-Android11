package com.android.tv.settings.display;

import java.util.List;

/**
 * FB Display Setting.
 */

public class FbDisplaySetting extends DisplaySetting {

    @Override
    public List<DisplayInfo> getDisplayInfoList() {
        return null;
    }

    @Override
    public List<String> getDisplayModes(DisplayInfo di) {
        return null;
    }

    @Override
    public String getCurDisplayMode(DisplayInfo di) {
        return null;
    }

    @Override
    public String getCurHdmiMode() {
        return null;
    }

    @Override
    public void setDisplayModeTemp(DisplayInfo di, int index) {

    }

    @Override
    public void setDisplayModeTemp(DisplayInfo di, String mode) {

    }

    @Override
    public void confirmSaveDisplayMode(DisplayInfo di, boolean isSave) {

    }
}
