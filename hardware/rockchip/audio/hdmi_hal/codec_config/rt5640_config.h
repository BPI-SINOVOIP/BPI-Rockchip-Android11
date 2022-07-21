#ifndef _RT5640_CONFIG_H_
#define _RT5640_CONFIG_H_

#include "config.h"

const struct config_control rt5640_speaker_normal_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPK MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPK MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPOL MIX SPKVOL L Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPOR MIX SPKVOL R Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {30, 33},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {on,on},
    },
};

const struct config_control rt5640_speaker_incall_controls[] = {

};

const struct config_control rt5640_speaker_ringtone_controls[] = {

};

const struct config_control rt5640_speaker_voip_controls[] = {

};

const struct config_control rt5640_earpiece_normal_controls[] = {

};

const struct config_control rt5640_earpiece_incall_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "IN2 Boost",
        .int_val = {4},
    },
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "Headset Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_earpiece_ringtone_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "IN2 Boost",
        .int_val = {4},
    },
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
     },
     {
        .ctl_name = "Headset Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_earpiece_voip_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    //min=0,max=8, bypass=0=0db, 30db=3, 52db=8
    {
        .ctl_name = "IN2 Boost",
        .int_val = {4},
    },
    //dBscale-min=-17.625dB,step=0.375dB,min=0,max=127
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
     },
     {
        .ctl_name = "Headset Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_headphone_normal_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
};

const struct config_control rt5640_headphone_incall_controls[] = {

};

const struct config_control rt5640_headphone_ringtone_controls[] = {

};

const struct config_control rt5640_speaker_headphone_normal_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPK MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPK MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPOL MIX SPKVOL L Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "SPOR MIX SPKVOL R Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Speaker Playback Volume",
        .int_val = {30, 33},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
};

const struct config_control rt5640_speaker_headphone_ringtone_controls[] = {

};

const struct config_control rt5640_headphone_voip_controls[] = {

};


const struct config_control rt5640_headpset_normal_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
};

const struct config_control rt5640_headset_incall_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    //min=0,max=8, bypass=0=0db, 30db=3, 52db=8
    {
        .ctl_name = "IN2 Boost",
        .int_val = {4},
    },
    //dBscale-min=-17.625dB,step=0.375dB,min=0,max=127
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
     },
     {
        .ctl_name = "Headset Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_headset_ringtone_controls[] = {

};

const struct config_control rt5640_headset_voip_controls[] = {

};


const struct config_control rt5640_bluetooth_normal_controls[] = {

};

const struct config_control rt5640_bluetooth_incall_controls[] = {

};

const struct config_control rt5640_bluetooth_voip_controls[] = {
    {
        .ctl_name = "DAC L2 Mux",
        .str_val = "IF2",
    },
    {
        .ctl_name = "DAC R2 Mux",
        .str_val = "IF2",
    },
    {
        .ctl_name = "DIG MIXL DAC L2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIG MIXR DAC R2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "DIG MIX",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
       .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC L1 Mux",
        .str_val = "Mono DAC MIXL",
    },
    {
        .ctl_name = "Mono ADC R1 Mux",
        .str_val = "Mono DAC MIXR",
    },
    {
        .ctl_name = "Mono ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC Capture Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "BT Up Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "BT Down Switch",
        .int_val = {on},
    },
};


const struct config_control rt5640_main_mic_capture_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    //min=0,max=8, bypass=0=0db, 30db=3, 52db=8
    {
        .ctl_name = "IN1 Boost",
        .int_val = {4},
    },
    //dBscale-min=-17.625dB,step=0.375dB,min=0,max=127
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
     },
     {
        .ctl_name = "Int Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_hands_free_mic_capture_controls[] = {
    {
        .ctl_name = "DAI select",
        .str_val = "1:1|2:2",
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker Channel Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "OUT MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off}
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {on}
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "ADC",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    //min=0,max=8, bypass=0=0db, 30db=3, 52db=8
    {
        .ctl_name = "IN2 Boost",
        .int_val = {4},
    },
    //dBscale-min=-17.625dB,step=0.375dB,min=0,max=127
    {
        .ctl_name = "ADC Capture Volume",
        .int_val = {100, 100},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
     },
     {
        .ctl_name = "Headset Mic Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_bluetooth_sco_mic_capture_controls[] = {
    {
        .ctl_name = "DAC L2 Mux",
        .str_val = "IF2",
    },
    {
        .ctl_name = "DAC R2 Mux",
        .str_val = "IF2",
    },
    {
        .ctl_name = "DIG MIXL DAC L2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DIG MIXR DAC R2 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC1 Mux",
        .str_val = "DIG MIX",
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "DAC MIXL INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "DAC MIXR INF1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono DAC MIXL DAC L1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono DAC MIXR DAC R1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC L1 Mux",
        .str_val = "Mono DAC MIXL",
    },
    {
        .ctl_name = "Mono ADC R1 Mux",
        .str_val = "Mono DAC MIXR",
    },
    {
        .ctl_name = "Mono ADC MIXL ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC MIXR ADC1 Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "Mono ADC Capture Switch",
        .int_val = {on,on},
    },
    {
        .ctl_name = "BT Up Switch",
        .int_val = {on},
    },
    {
        .ctl_name = "BT Down Switch",
        .int_val = {on},
    },
};

const struct config_control rt5640_playback_off_controls[] = {
    {
        .ctl_name = "HP L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "HP R Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "HP Channel Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "HPO MIX HPVOL Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker L Playback Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Speaker R Playback Switch",
        .int_val = {off},
    },
};

const struct config_control rt5640_capture_off_controls[] = {
    {
        .ctl_name = "RECMIXL BST1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "RECMIXR BST1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "RECMIXL BST2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "RECMIXR BST2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Headset Mic Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Int Mic Switch",
        .int_val = {off},
    },
};

const struct config_control rt5640_incall_off_controls[] = {
    {
        .ctl_name = "DIG MIXL DAC L2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "DIG MIXR DAC R2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Mono DAC MIXL DAC L1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono DAC MIXR DAC R1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC MIXL ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC MIXR ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Headset Mic Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Int Mic Switch",
        .int_val = {off},
    },
};

const struct config_control rt5640_voip_off_controls[] = {
    {
        .ctl_name = "DIG MIXL DAC L2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "DIG MIXR DAC R2 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXL ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Stereo ADC MIXR ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "ADC Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Mono DAC MIXL DAC L1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono DAC MIXR DAC R1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC MIXL ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC MIXR ADC1 Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Mono ADC Capture Switch",
        .int_val = {off,off},
    },
    {
        .ctl_name = "Headset Mic Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "Int Mic Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "BT Up Switch",
        .int_val = {off},
    },
    {
        .ctl_name = "BT Down Switch",
        .int_val = {off},
    },
};


const struct config_route_table rt5640_config_table = {
    //speaker
    .speaker_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_normal_controls,
        .controls_count = sizeof(rt5640_speaker_normal_controls) / sizeof(struct config_control),
    },
    .speaker_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_incall_controls,
        .controls_count = sizeof(rt5640_speaker_incall_controls) / sizeof(struct config_control),
    },
    .speaker_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_ringtone_controls,
        .controls_count = sizeof(rt5640_speaker_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_voip_controls,
        .controls_count = sizeof(rt5640_speaker_voip_controls) / sizeof(struct config_control),
    },

    //earpiece
    .earpiece_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_earpiece_normal_controls,
        .controls_count = sizeof(rt5640_earpiece_normal_controls) / sizeof(struct config_control),
    },
    .earpiece_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_earpiece_incall_controls,
        .controls_count = sizeof(rt5640_earpiece_incall_controls) / sizeof(struct config_control),
    },
    .earpiece_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_earpiece_ringtone_controls,
        .controls_count = sizeof(rt5640_earpiece_ringtone_controls) / sizeof(struct config_control),
    },
    .earpiece_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_earpiece_voip_controls,
        .controls_count = sizeof(rt5640_earpiece_voip_controls) / sizeof(struct config_control),
    },

    //headphone
    .headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headphone_normal_controls,
        .controls_count = sizeof(rt5640_headphone_normal_controls) / sizeof(struct config_control),
    },
    .headphone_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headphone_incall_controls,
        .controls_count = sizeof(rt5640_headphone_incall_controls) / sizeof(struct config_control),
    },
    .headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headphone_ringtone_controls,
        .controls_count = sizeof(rt5640_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_headphone_normal_controls,
        .controls_count = sizeof(rt5640_speaker_headphone_normal_controls) / sizeof(struct config_control),
    },
    .speaker_headphone_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_speaker_headphone_ringtone_controls,
        .controls_count = sizeof(rt5640_speaker_headphone_ringtone_controls) / sizeof(struct config_control),
    },
    .headphone_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headphone_voip_controls,
        .controls_count = sizeof(rt5640_headphone_voip_controls) / sizeof(struct config_control),
    },

    .headset_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headpset_normal_controls,
        .controls_count = sizeof(rt5640_headpset_normal_controls) / sizeof(struct config_control),
    },
    .headset_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headset_incall_controls,
        .controls_count = sizeof(rt5640_headset_incall_controls) / sizeof(struct config_control),
    },
    .headset_ringtone = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headset_ringtone_controls,
        .controls_count = sizeof(rt5640_headset_ringtone_controls) / sizeof(struct config_control),
    },
    .headset_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_headset_voip_controls,
        .controls_count = sizeof(rt5640_headset_voip_controls) / sizeof(struct config_control),
    },

    //bluetooth
    .bluetooth_normal = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_bluetooth_normal_controls,
        .controls_count = sizeof(rt5640_bluetooth_normal_controls) / sizeof(struct config_control),
    },
    .bluetooth_incall = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_bluetooth_incall_controls,
        .controls_count = sizeof(rt5640_bluetooth_incall_controls) / sizeof(struct config_control),
    },
    .bluetooth_voip = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_bluetooth_voip_controls,
        .controls_count = sizeof(rt5640_bluetooth_voip_controls) / sizeof(struct config_control),
    },

    //capture
    .main_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_main_mic_capture_controls,
        .controls_count = sizeof(rt5640_main_mic_capture_controls) / sizeof(struct config_control),
    },
    .hands_free_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_hands_free_mic_capture_controls,
        .controls_count = sizeof(rt5640_hands_free_mic_capture_controls) / sizeof(struct config_control),
    },
    .bluetooth_sco_mic_capture = {
        .sound_card = 0,
        .devices = DEVICES_0,
        .controls = rt5640_bluetooth_sco_mic_capture_controls,
        .controls_count = sizeof(rt5640_bluetooth_sco_mic_capture_controls) / sizeof(struct config_control),
    },

    //off
    .playback_off = {
        .controls = rt5640_playback_off_controls,
        .controls_count = sizeof(rt5640_playback_off_controls) / sizeof(struct config_control),
    },
    .capture_off = {
        .controls = rt5640_capture_off_controls,
        .controls_count = sizeof(rt5640_capture_off_controls) / sizeof(struct config_control),
    },
    .incall_off = {
        .controls = rt5640_incall_off_controls,
        .controls_count = sizeof(rt5640_incall_off_controls) / sizeof(struct config_control),
    },
    .voip_off = {
        .controls = rt5640_voip_off_controls,
        .controls_count = sizeof(rt5640_voip_off_controls) / sizeof(struct config_control),
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

#endif //_RT5640_CONFIG_H_
