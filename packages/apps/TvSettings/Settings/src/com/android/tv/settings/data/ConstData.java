package com.android.tv.settings.data;

/**
 * @author GaoFei
 *         常量数据
 */
public class ConstData {
    public interface IntentKey {
        String DISPLAY_INFO = "display_info";
        String PLATFORM = "platform";
        String VPN_PROFILE = "vpn_profile";
        String VPN_EXIST = "vpn_exist";
        String VPN_EDITING = "vpn_editing";
        String DISPLAY_ID = "display_id";
        String DISPLAY_NAME = "display_name";
    }

    public interface SharedKey {
        String BCSH_VALUES = "bcsh_vlaues";
        String BCSH_BRIGHTNESS = "bcsh_brightness";
        String BCSH_CONTRAST = "bcsh_contrast";
        String BCSH_STAURATION = "bcsh_stauration";
        String BCSH_TONE = "bcsh_tone";
        String HDR_VALUES = "hdr_values";
        String MAX_BRIGHTNESS = "hdr_max_brightness";
        String MIN_BRIGHTNESS = "hdr_min_brightness";
        String BRIGHTNESS = "hdr_brightness";
        String STATURATION = "hdr_staturation";
        String MAX_SDR_BIRHTNESS = "max_sdr_brightness";
        String MIN_SDR_BRIGHTNESS = "min_sdr_brightness";
    }

    public interface ScaleDirection {
        int LEFT = 0;
        int RIGHT = 1;
        int TOP = 2;
        int BOTTOM = 3;
    }
}
