package com.android.tv.settings.display;

import androidx.annotation.Keep;

/**
 * HdmiDeviceFragment.
 */

@Keep
public class HdmiDeviceFragment extends DeviceFragment {

    public HdmiDeviceFragment() {
        super();
    }

    @Override
    protected DisplayInfo getDisplayInfo() {
        return DrmDisplaySetting.getHdmiDisplayInfo();
    }
}
