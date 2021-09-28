/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
*/

#ifndef _WM8960_CONFIG_H_
#define _WM8960_CONFIG_H_

#include "config.h"

const struct config_control wm8960_speaker_normal_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {120, 120},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_speaker_incall_controls[] = {
};

const struct config_control wm8960_speaker_ringtone_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {120, 120},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_speaker_voip_controls[] = {

};

const struct config_control wm8960_earpiece_normal_controls[] = {

};

const struct config_control wm8960_earpiece_incall_controls[] = {
};

const struct config_control wm8960_earpiece_ringtone_controls[] = {
};

const struct config_control wm8960_earpiece_voip_controls[] = {

};

const struct config_control wm8960_headphone_normal_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {116, 116},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_headphone_incall_controls[] = {

};

const struct config_control wm8960_headphone_ringtone_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {116, 116},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
 };

const struct config_control wm8960_speaker_headphone_normal_controls[] = {
};

const struct config_control wm8960_speaker_headphone_ringtone_controls[] = {
};

const struct config_control wm8960_headphone_voip_controls[] = {
};

const struct config_control wm8960_headset_normal_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {116, 116},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_headset_incall_controls[] = {
 };

const struct config_control wm8960_headset_ringtone_controls[] = {
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {255, 255},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {116, 116},
    },
    {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_headset_voip_controls[] = {

};

const struct config_control wm8960_bluetooth_normal_controls[] = {
 };

const struct config_control wm8960_bluetooth_incall_controls[] = { 
};

const struct config_control wm8960_bluetooth_voip_controls[] = {

};

const struct config_control wm8960_main_mic_capture_controls[] = {
    {
        .ctl_name = "Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC High Pass Filter Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINPUT2 To Left Boost Mixer Volume",
        .int_val = {3},
    },
    {
        .ctl_name = "LINPUT3 To Left Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "RINPUT2 To Right Boost Mixer Volume",
        .int_val = {3},
    },
    {
        .ctl_name = "RINPUT3 To Right Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "Lmic Boost",
        .str_val = "0db",
    },
    {
        .ctl_name = "Rmic Boost",
        .str_val = "0db",
    },
    {
        .ctl_name = "Capture Volume",
        .int_val = {63,63},
    }, 
    {
        .ctl_name = "ADC PCM Capture Volume",
        .int_val = {127,127},
    }, 
    {
        .ctl_name = "Capture Volume Switch",
        .int_val = {0,0},
    }, 
    {
        .ctl_name = "Capture Switch",
        .int_val = {on,on},
    }, 
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {on},
    },
};

const struct config_control wm8960_hands_free_mic_capture_controls[] = {
  {
        .ctl_name = "Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC High Pass Filter Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINPUT2 To Left Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "LINPUT3 To Left Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "RINPUT2 To Right Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "RINPUT3 To Right Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Lmic Boost",
        .str_val = "0db",
    },
    {
        .ctl_name = "Rmic Boost",
        .str_val = "13db",
    },
    {
        .ctl_name = "Capture Volume",
        .int_val = {0,63},
    }, 
    {
        .ctl_name = "ADC PCM Capture Volume",
        .int_val = {0,80},
    }, 
    {
        .ctl_name = "Capture Volume Switch",
        .int_val = {0,0},
    }, 
    {
        .ctl_name = "Capture Switch",
        .int_val = {off,on},
    }, 
};

const struct config_control wm8960_bluetooth_sco_mic_capture_controls[] = {
};

const struct config_control wm8960_playback_off_controls[] = {
   {
        .ctl_name = "Right Output Mixer PCM Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer PCM Playback Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Right Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer Boost Bypass Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Output Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Output Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {0, 0},
    }, 
};

const struct config_control wm8960_capture_off_controls[] = {
    {
        .ctl_name = "Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC High Pass Filter Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Boost Mixer RINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Left Boost Mixer LINPUT3 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINPUT2 To Left Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "LINPUT3 To Left Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "RINPUT2 To Right Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "RINPUT3 To Right Boost Mixer Volume",
        .int_val = {0},
    },
    {
        .ctl_name = "Left Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Input Mixer Boost Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Lmic Boost",
        .str_val = "0db",
    },
    {
        .ctl_name = "Rmic Boost",
        .str_val = "0db",
    },
    {
        .ctl_name = "Capture Volume",
        .int_val = {0,0},
    }, 
    {
        .ctl_name = "ADC PCM Capture Volume",
        .int_val = {0,0},
    }, 
    {
        .ctl_name = "Capture Volume Switch",
        .int_val = {0,0},
    }, 
};

const struct config_control wm8960_incall_off_controls[] = { 
};

const struct config_control wm8960_voip_off_controls[] = {

};

const struct config_route_table wm8960_config_table = {
    //speaker
    .speaker_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_normal_controls,
        .controls_count = sizeof(wm8960_speaker_normal_controls) / sizeof(struct config_control),
    },
    .speaker_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_incall_controls,
        .controls_count = sizeof(wm8960_speaker_incall_controls) / sizeof(struct config_control),
    },
    .speaker_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_ringtone_controls,
        .controls_count = sizeof(wm8960_speaker_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_voip_controls,
        .controls_count = sizeof(wm8960_speaker_voip_controls) / sizeof(struct config_control),
    },

    //earpiece
    .earpiece_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_earpiece_normal_controls,
        .controls_count = sizeof(wm8960_earpiece_normal_controls) / sizeof(struct config_control),
    },
    .earpiece_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_earpiece_incall_controls,
        .controls_count = sizeof(wm8960_earpiece_incall_controls) / sizeof(struct config_control),
    },
    .earpiece_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_earpiece_ringtone_controls,
        .controls_count = sizeof(wm8960_earpiece_ringtone_controls) / sizeof(struct config_control),
    },
    .earpiece_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_earpiece_voip_controls,
        .controls_count = sizeof(wm8960_earpiece_voip_controls) / sizeof(struct config_control),
    },

    //headphone
    .headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headphone_normal_controls,
        .controls_count = sizeof(wm8960_headphone_normal_controls) / sizeof(struct config_control),
    },
    .headphone_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headphone_incall_controls,
        .controls_count = sizeof(wm8960_headphone_incall_controls) / sizeof(struct config_control),
    },
    .headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headphone_ringtone_controls,
        .controls_count = sizeof(wm8960_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_headphone_normal_controls,
        .controls_count = sizeof(wm8960_speaker_headphone_normal_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_speaker_headphone_ringtone_controls,
        .controls_count = sizeof(wm8960_speaker_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .headphone_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headphone_voip_controls,
        .controls_count = sizeof(wm8960_headphone_voip_controls) / sizeof(struct config_control),
    },

    //headset
    .headset_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headset_normal_controls,
        .controls_count = sizeof(wm8960_headset_normal_controls) / sizeof(struct config_control),
    },
    .headset_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headset_incall_controls,
        .controls_count = sizeof(wm8960_headset_incall_controls) / sizeof(struct config_control),
    },
    .headset_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headset_ringtone_controls,
        .controls_count = sizeof(wm8960_headset_ringtone_controls) / sizeof(struct config_control),
    },
    .headset_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_headset_voip_controls,
        .controls_count = sizeof(wm8960_headset_voip_controls) / sizeof(struct config_control),
    },

    //bluetooth
    .bluetooth_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_bluetooth_normal_controls,
        .controls_count = sizeof(wm8960_bluetooth_normal_controls) / sizeof(struct config_control),
    },
    .bluetooth_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_bluetooth_incall_controls,
        .controls_count = sizeof(wm8960_bluetooth_incall_controls) / sizeof(struct config_control),
    },
    .bluetooth_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_bluetooth_voip_controls,
        .controls_count = sizeof(wm8960_bluetooth_voip_controls) / sizeof(struct config_control),
    },

    //capture
    .main_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_main_mic_capture_controls,
        .controls_count = sizeof(wm8960_main_mic_capture_controls) / sizeof(struct config_control),
    },
    .hands_free_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_hands_free_mic_capture_controls,
        .controls_count = sizeof(wm8960_hands_free_mic_capture_controls) / sizeof(struct config_control),
    },
    .bluetooth_sco_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = wm8960_bluetooth_sco_mic_capture_controls,
        .controls_count = sizeof(wm8960_bluetooth_sco_mic_capture_controls) / sizeof(struct config_control),
    },

    //off
    .playback_off = {
        .controls = wm8960_playback_off_controls,
        .controls_count = sizeof(wm8960_playback_off_controls) / sizeof(struct config_control),
    },
    .capture_off = {
        .controls = wm8960_capture_off_controls,
        .controls_count = sizeof(wm8960_capture_off_controls) / sizeof(struct config_control),
    },
    .incall_off = {
        .controls = wm8960_incall_off_controls,
        .controls_count = sizeof(wm8960_incall_off_controls) / sizeof(struct config_control),
    },
    .voip_off = {
        .controls = wm8960_voip_off_controls,
        .controls_count = sizeof(wm8960_voip_off_controls) / sizeof(struct config_control),
    },

    //hdmi
    .hdmi_normal = {
        .sound_card = 1,
        .devices = DEVICES_0,
        .controls_count = 0,
    },
    //usb audio
    .usb_normal = {
        .sound_card = 2,
        .devices = DEVICES_0,
        .controls_count = 0,
    },
    .usb_capture = {
        .sound_card = 2,
        .devices = DEVICES_0,
        .controls_count = 0,
    },
};

#endif //_wm8960_CONFIG_H_
