package com.android.tv.settings.display;

import java.util.List;

/**
 * Display Setting.
 */

public abstract class DisplaySetting {

    public abstract List<DisplayInfo> getDisplayInfoList();

    public abstract List<String> getDisplayModes(DisplayInfo di);

    public abstract String getCurDisplayMode(DisplayInfo di);

    public abstract String getCurHdmiMode();

    public abstract void setDisplayModeTemp(DisplayInfo di, int index);

    public abstract void setDisplayModeTemp(DisplayInfo di, String mode);

    public abstract void confirmSaveDisplayMode(DisplayInfo di, boolean isSave);
}
