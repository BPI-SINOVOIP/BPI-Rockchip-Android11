/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
*/

#ifndef _RK616_CONFIG_H_
#define _RK616_CONFIG_H_

#include "config.h"

const struct config_control rk616_speaker_normal_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {22, 22},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_speaker_incall_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    //mic1-->line1/2
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTL",
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Micbias1 Voltage",
        .int_val = {7},
    },
    {
        .ctl_name = "BST_L Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "Main Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {29},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEMIX PGAL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT1 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT2 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {on},
    },
    //IN1N/P ---> SPK
    {
        .ctl_name = "HPMix Mux",
        .str_val = "DIFFIN",
    },
    {
        .ctl_name = "HPMIXR HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPMIXL HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIFFIN Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "DIFFIN Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "DIFFIN Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXR Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_speaker_ringtone_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_speaker_voip_controls[] = {

};

const struct config_control rk616_earpiece_normal_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {22, 22},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_earpiece_incall_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    //mic1-->line1/2
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTL",
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Micbias1 Voltage",
        .int_val = {7},
    },
    {
        .ctl_name = "BST_L Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "Main Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {29},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEMIX PGAL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT1 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT2 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {on},
    },
    //IN1N/P ---> SPK
    {
        .ctl_name = "HPMix Mux",
        .str_val = "DIFFIN",
    },
    {
        .ctl_name = "HPMIXR HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPMIXL HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIFFIN Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "DIFFIN Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "DIFFIN Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXR Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_earpiece_ringtone_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_earpiece_voip_controls[] = {

};

const struct config_control rk616_headphone_normal_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_headphone_incall_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    //mic1-->line1/2
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTL",
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Micbias1 Voltage",
        .int_val = {7},
    },
    {
        .ctl_name = "BST_L Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "Main Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {29},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEMIX PGAL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT1 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT2 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {on},
    },
    //IN1N/P ---> HP
    {
        .ctl_name = "HPMix Mux",
        .str_val = "DIFFIN",
    },
    {
        .ctl_name = "HPMIXR HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPMIXL HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIFFIN Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "DIFFIN Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "DIFFIN Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXR Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_headphone_ringtone_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_speaker_headphone_normal_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    },
};

const struct config_control rk616_speaker_headphone_ringtone_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {on, on},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {on},
    }, 
};

const struct config_control rk616_headphone_voip_controls[] = {

};

const struct config_control rk616_headset_normal_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_headset_incall_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    //mic2-->line1/2
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTR",
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Micbias2 Voltage",
        .int_val = {0},
    },
    {
        .ctl_name = "BST_R Mode",
        .int_val = {1},
    },
    {
        .ctl_name = "Headset Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "Headset Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {29},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEMIX PGAL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT1 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT2 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {on},
    },
    //IN1N/P ---> HP
    {
        .ctl_name = "HPMix Mux",
        .str_val = "DIFFIN",
    },
    {
        .ctl_name = "HPMIXR HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPMIXL HPMix Mux Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIFFIN Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "DIFFIN Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "DIFFIN Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "HPMIX MUX to HPMIXR Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_headset_ringtone_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "High",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPMIXR DACR Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPR Mux",
        .str_val = "HPMIXR",
    },
    {
        .ctl_name = "HPL Mux",
        .str_val = "HPMIXL",
    },
    {
        .ctl_name = "Headphone Playback Volume",
        .int_val = {31, 31},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {on, on},
    },
};

const struct config_control rk616_headset_voip_controls[] = {

};

const struct config_control rk616_bluetooth_normal_controls[] = {

};

const struct config_control rk616_bluetooth_incall_controls[] = {
    //DACL --> line1/2
    {
        .ctl_name = "LINEMIX DACL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT1 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT2 Playback Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {on},
    },
    //IN1N/P-->ADCL
    {
        .ctl_name = "MIXINL IN1P Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "IN1P to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    }, 
};

const struct config_control rk616_bluetooth_voip_controls[] = {

};

const struct config_control rk616_main_mic_capture_controls[] = {
    {
        .ctl_name = "Headset Mic Capture Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTL",
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Main Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "BST_L Mode",
        .int_val = {0},
    },
    {
        .ctl_name = "Micbias1 Voltage",
        .int_val = {7},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {31},
    },
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    }, 
};

const struct config_control rk616_hands_free_mic_capture_controls[] = {
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Micbias1 Voltage",
        .int_val = {0},
    },

    {
        .ctl_name = "Headset Mic Capture Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mic Mux",
        .str_val = "BSTR",
    },
    {
        .ctl_name = "MUXMIC to MIXINL Volume",
        .int_val = {7},
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Headset Mic Capture Volume",
        .int_val = {1},
    },
    {
        .ctl_name = "BST_R Mode",
        .int_val = {1},
    },
    {
        .ctl_name = "Micbias2 Voltage",
        .int_val = {7},
    },
    {
        .ctl_name = "PGAL Capture Volume",
        .int_val = {25},
    },
    {
        .ctl_name = "Headset Jack Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {on},
    }, 
};

const struct config_control rk616_bluetooth_sco_mic_capture_controls[] = {

};

const struct config_control rk616_playback_off_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },

    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "HPMIXL DACL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Headphone Jack Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Headphone Playback Switch",
        .int_val = {off, off},
    },
};

const struct config_control rk616_capture_off_controls[] = {
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Headset Mic Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Headset Jack Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {off},
    }, 
};

const struct config_control rk616_incall_off_controls[] = {
    {
        .ctl_name = "SPK GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "HP GPIO Control",
        .str_val = "Low",
    },
    {
        .ctl_name = "RCV GPIO Control",
        .str_val = "Low",
    },

    //close mic1-->line1/2
    {
        .ctl_name = "Mic Jack Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "MIXINL MUXMIC Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Main Mic Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "MIXINL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "PGAL Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINEMIX PGAL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINEOUT1 Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "LINEOUT2 Playback Switch",
        .int_val = {off},
    },
    //clsoe IN1N/P ---> SPK
    {
        .ctl_name = "HPMIXR HPMix Mux Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "HPMIXL HPMix Mux Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Ext Spk Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "DIFFIN Capture Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Playback Switch",
        .int_val = {off, off},
    },
};

const struct config_control rk616_voip_off_controls[] = {

};

const struct config_route_table rk616_config_table = {
    //speaker
    .speaker_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_normal_controls,
        .controls_count = sizeof(rk616_speaker_normal_controls) / sizeof(struct config_control),
    },
    .speaker_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_incall_controls,
        .controls_count = sizeof(rk616_speaker_incall_controls) / sizeof(struct config_control),
    },
    .speaker_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_ringtone_controls,
        .controls_count = sizeof(rk616_speaker_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_voip_controls,
        .controls_count = sizeof(rk616_speaker_voip_controls) / sizeof(struct config_control),
    },

    //earpiece
    .earpiece_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_earpiece_normal_controls,
        .controls_count = sizeof(rk616_earpiece_normal_controls) / sizeof(struct config_control),
    },
    .earpiece_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_earpiece_incall_controls,
        .controls_count = sizeof(rk616_earpiece_incall_controls) / sizeof(struct config_control),
    },
    .earpiece_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_earpiece_ringtone_controls,
        .controls_count = sizeof(rk616_earpiece_ringtone_controls) / sizeof(struct config_control),
    },
    .earpiece_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_earpiece_voip_controls,
        .controls_count = sizeof(rk616_earpiece_voip_controls) / sizeof(struct config_control),
    },

    //headphone
    .headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headphone_normal_controls,
        .controls_count = sizeof(rk616_headphone_normal_controls) / sizeof(struct config_control),
    },
    .headphone_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headphone_incall_controls,
        .controls_count = sizeof(rk616_headphone_incall_controls) / sizeof(struct config_control),
    },
    .headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headphone_ringtone_controls,
        .controls_count = sizeof(rk616_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_headphone_normal_controls,
        .controls_count = sizeof(rk616_speaker_headphone_normal_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_speaker_headphone_ringtone_controls,
        .controls_count = sizeof(rk616_speaker_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .headphone_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headphone_voip_controls,
        .controls_count = sizeof(rk616_headphone_voip_controls) / sizeof(struct config_control),
    },

    //headset
    .headset_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headset_normal_controls,
        .controls_count = sizeof(rk616_headset_normal_controls) / sizeof(struct config_control),
    },
    .headset_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headset_incall_controls,
        .controls_count = sizeof(rk616_headset_incall_controls) / sizeof(struct config_control),
    },
    .headset_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headset_ringtone_controls,
        .controls_count = sizeof(rk616_headset_ringtone_controls) / sizeof(struct config_control),
    },
    .headset_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_headset_voip_controls,
        .controls_count = sizeof(rk616_headset_voip_controls) / sizeof(struct config_control),
    },

    //bluetooth
    .bluetooth_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_bluetooth_normal_controls,
        .controls_count = sizeof(rk616_bluetooth_normal_controls) / sizeof(struct config_control),
    },
    .bluetooth_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_bluetooth_incall_controls,
        .controls_count = sizeof(rk616_bluetooth_incall_controls) / sizeof(struct config_control),
    },
    .bluetooth_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_bluetooth_voip_controls,
        .controls_count = sizeof(rk616_bluetooth_voip_controls) / sizeof(struct config_control),
    },

    //capture
    .main_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_main_mic_capture_controls,
        .controls_count = sizeof(rk616_main_mic_capture_controls) / sizeof(struct config_control),
    },
    .hands_free_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_hands_free_mic_capture_controls,
        .controls_count = sizeof(rk616_hands_free_mic_capture_controls) / sizeof(struct config_control),
    },
    .bluetooth_sco_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rk616_bluetooth_sco_mic_capture_controls,
        .controls_count = sizeof(rk616_bluetooth_sco_mic_capture_controls) / sizeof(struct config_control),
    },

    //off
    .playback_off = {
        .controls = rk616_playback_off_controls,
        .controls_count = sizeof(rk616_playback_off_controls) / sizeof(struct config_control),
    },
    .capture_off = {
        .controls = rk616_capture_off_controls,
        .controls_count = sizeof(rk616_capture_off_controls) / sizeof(struct config_control),
    },
    .incall_off = {
        .controls = rk616_incall_off_controls,
        .controls_count = sizeof(rk616_incall_off_controls) / sizeof(struct config_control),
    },
    .voip_off = {
        .controls = rk616_voip_off_controls,
        .controls_count = sizeof(rk616_voip_off_controls) / sizeof(struct config_control),
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

#endif //_RK616_CONFIG_H_
