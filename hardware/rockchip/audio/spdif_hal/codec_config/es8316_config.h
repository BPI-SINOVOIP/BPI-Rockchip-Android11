#ifndef _ES8316_CONFIG_H_
#define _ES8316_CONFIG_H_

#include "config.h"

const struct config_control es8316_speaker_normal_controls[] = {
    {
        .ctl_name = "DAC Playback Volume",
        .int_val = {192, 192},
    },
    {
        .ctl_name = "Left Headphone Mixer Left DAC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Headphone Mixer Right DAC Switch",
	.int_val = {on},
    },
};

const struct config_control es8316_speaker_incall_controls[] = {
};

const struct config_control es8316_speaker_ringtone_controls[] = {
};

const struct config_control es8316_speaker_voip_controls[] = {
};

const struct config_control es8316_earpiece_normal_controls[] = {
};

const struct config_control es8316_earpiece_incall_controls[] = {
};

const struct config_control es8316_earpiece_ringtone_controls[] = {
};

const struct config_control es8316_earpiece_voip_controls[] = {
};

const struct config_control es8316_headphone_normal_controls[] = {
    {
        .ctl_name = "DAC Playback Volume",
        .int_val = {192, 192},
    },
    {
        .ctl_name = "Left Headphone Mixer Left DAC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Headphone Mixer Right DAC Switch",
	.int_val = {on},
    },
};

const struct config_control es8316_headphone_incall_controls[] = {
};

const struct config_control es8316_headphone_ringtone_controls[] = {
};

const struct config_control es8316_speaker_headphone_normal_controls[] = {
};

const struct config_control es8316_speaker_headphone_ringtone_controls[] = {
};

const struct config_control es8316_headphone_voip_controls[] = {
};

const struct config_control es8316_headset_normal_controls[] = {
    {
        .ctl_name = "DAC Playback Volume",
        .int_val = {192, 192},
    },
    {
        .ctl_name = "Left Headphone Mixer Left DAC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Right Headphone Mixer Right DAC Switch",
    },
};

const struct config_control es8316_headset_incall_controls[] = {
};

const struct config_control es8316_headset_ringtone_controls[] = {
};

const struct config_control es8316_headset_voip_controls[] = {
};

const struct config_control es8316_bluetooth_normal_controls[] = {
};

const struct config_control es8316_bluetooth_incall_controls[] = {
};

const struct config_control es8316_bluetooth_voip_controls[] = {
};

const struct config_control es8316_main_mic_capture_controls[] = {
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {192},
    },
    {
        .ctl_name = "Differential Mux",
        .int_val = {0},
    },
};

const struct config_control es8316_hands_free_mic_capture_controls[] = {
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {192},
    },
    {
        .ctl_name = "Differential Mux",
        .int_val = {1},
    },
};

const struct config_control es8316_bluetooth_sco_mic_capture_controls[] = {
};

const struct config_control es8316_playback_off_controls[] = {
    {
        .ctl_name = "DAC Playback Volume",
        .int_val = {0, 0},
    },
    {
        .ctl_name = "Left Headphone Mixer Left DAC Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Right Headphone Mixer Right DAC Switch",
	.int_val = {off},
    },
};

const struct config_control es8316_capture_off_controls[] = {
};

const struct config_control es8316_incall_off_controls[] = {
};

const struct config_control es8316_voip_off_controls[] = {
};

const struct config_route_table es8316_config_table = {
    //speaker
    .speaker_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_normal_controls,
        .controls_count = sizeof(es8316_speaker_normal_controls) / sizeof(struct config_control),
    },
    .speaker_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_incall_controls,
        .controls_count = sizeof(es8316_speaker_incall_controls) / sizeof(struct config_control),
    },
    .speaker_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_ringtone_controls,
        .controls_count = sizeof(es8316_speaker_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_voip_controls,
        .controls_count = sizeof(es8316_speaker_voip_controls) / sizeof(struct config_control),
    },

    //earpiece
    .earpiece_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_earpiece_normal_controls,
        .controls_count = sizeof(es8316_earpiece_normal_controls) / sizeof(struct config_control),
    },
    .earpiece_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_earpiece_incall_controls,
        .controls_count = sizeof(es8316_earpiece_incall_controls) / sizeof(struct config_control),
    },
    .earpiece_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_earpiece_ringtone_controls,
        .controls_count = sizeof(es8316_earpiece_ringtone_controls) / sizeof(struct config_control),
    },
    .earpiece_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_earpiece_voip_controls,
        .controls_count = sizeof(es8316_earpiece_voip_controls) / sizeof(struct config_control),
    },

    //headphone
    .headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headphone_normal_controls,
        .controls_count = sizeof(es8316_headphone_normal_controls) / sizeof(struct config_control),
    },
    .headphone_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headphone_incall_controls,
        .controls_count = sizeof(es8316_headphone_incall_controls) / sizeof(struct config_control),
    },
    .headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headphone_ringtone_controls,
        .controls_count = sizeof(es8316_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_headphone_normal_controls,
        .controls_count = sizeof(es8316_speaker_headphone_normal_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_speaker_headphone_ringtone_controls,
        .controls_count = sizeof(es8316_speaker_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .headphone_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headphone_voip_controls,
        .controls_count = sizeof(es8316_headphone_voip_controls) / sizeof(struct config_control),
    },

    //headset
    .headset_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headset_normal_controls,
        .controls_count = sizeof(es8316_headset_normal_controls) / sizeof(struct config_control),
    },
    .headset_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headset_incall_controls,
        .controls_count = sizeof(es8316_headset_incall_controls) / sizeof(struct config_control),
    },
    .headset_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headset_ringtone_controls,
        .controls_count = sizeof(es8316_headset_ringtone_controls) / sizeof(struct config_control),
    },
    .headset_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_headset_voip_controls,
        .controls_count = sizeof(es8316_headset_voip_controls) / sizeof(struct config_control),
    },

    //bluetooth
    .bluetooth_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_bluetooth_normal_controls,
        .controls_count = sizeof(es8316_bluetooth_normal_controls) / sizeof(struct config_control),
    },
    .bluetooth_incall = {
        .sound_card = 0,
        .devices = DEVICES_0_1,
        .controls = es8316_bluetooth_incall_controls,
        .controls_count = sizeof(es8316_bluetooth_incall_controls) / sizeof(struct config_control),
    },
    .bluetooth_voip = {
        .sound_card = 0,
        .devices = DEVICES_0_1,
        .controls = es8316_bluetooth_voip_controls,
        .controls_count = sizeof(es8316_bluetooth_voip_controls) / sizeof(struct config_control),
    },

    //capture
    .main_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_main_mic_capture_controls,
        .controls_count = sizeof(es8316_main_mic_capture_controls) / sizeof(struct config_control),
    },
    .hands_free_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = es8316_hands_free_mic_capture_controls,
        .controls_count = sizeof(es8316_hands_free_mic_capture_controls) / sizeof(struct config_control),
    },
    .bluetooth_sco_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0_1,
        .controls = es8316_bluetooth_sco_mic_capture_controls,
        .controls_count = sizeof(es8316_bluetooth_sco_mic_capture_controls) / sizeof(struct config_control),
    },

    //off
    .playback_off = {
        .controls = es8316_playback_off_controls,
        .controls_count = sizeof(es8316_playback_off_controls) / sizeof(struct config_control),
    },
    .capture_off = {
        .controls = es8316_capture_off_controls,
        .controls_count = sizeof(es8316_capture_off_controls) / sizeof(struct config_control),
    },
    .incall_off = {
        .controls = es8316_incall_off_controls,
        .controls_count = sizeof(es8316_incall_off_controls) / sizeof(struct config_control),
    },
    .voip_off = {
        .controls = es8316_voip_off_controls,
        .controls_count = sizeof(es8316_voip_off_controls) / sizeof(struct config_control),
    },
#ifdef BOX_HAL
    //hdmi
    .hdmi_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls_count = 0,
    },
#else
    //hdmi
    .hdmi_normal = {
        .sound_card = 1,
        .devices = DEVICES_0,
        .controls_count = 0,
    },
#endif
    //spdif
    .spdif_normal = {
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


#endif //_ES8316_CONFIG_H_
