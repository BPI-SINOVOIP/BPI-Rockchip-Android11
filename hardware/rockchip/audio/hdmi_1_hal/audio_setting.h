#ifndef _RK_AUDIO_HAL_SETTING_
#define _RK_AUDIO_HAL_SETTING_


#include <stdbool.h>

#define DEFAULT_MODE 0
#define HDMI_BITSTREAM_MODE 6
#define SPDIF_PASSTHROUGH_MODE 8

extern bool isBypass();
extern bool isMultiPcm();
extern int getOutputDevice();

#endif