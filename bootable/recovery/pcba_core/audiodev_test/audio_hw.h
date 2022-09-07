#ifndef __AUDIO_HW_H_
#define __AUDIO_HW_H_
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"{
#endif
/* note: those codec copy from hardware/rockchip/audio/tinyalsa_hal 
 * So if error, pls update from audiohal then try again
 */
struct dev_proc_info
{
    const char *cid; /* cardX/id match */
    const char *did; /* dai id match */
};

struct dev_info
{
    const char *id;
    struct dev_proc_info *info;
    int card;
    int device;
};

enum snd_out_sound_cards {
    SND_OUT_SOUND_CARD_UNKNOWN = -1,
    SND_OUT_SOUND_CARD_SPEAKER = 0,
    SND_OUT_SOUND_CARD_HDMI,
    SND_OUT_SOUND_CARD_SPDIF,
    SND_OUT_SOUND_CARD_BT,
    SND_OUT_SOUND_CARD_MAX,
};

enum snd_in_sound_cards {
    SND_IN_SOUND_CARD_UNKNOWN = -1,
    SND_IN_SOUND_CARD_MIC = 0,
    SND_IN_SOUND_CARD_BT,
    SND_IN_SOUND_CARD_HDMI,
    SND_IN_SOUND_CARD_MAX,
};

struct audio_device {
    int mHeadsetState;
    void *route;
    struct dev_info dev_out[SND_OUT_SOUND_CARD_MAX];
    struct dev_info dev_in[SND_IN_SOUND_CARD_MAX];
};

struct stream_out {
    unsigned int device;
    struct pcm_config *config;
    struct pcm *pcm[SND_OUT_SOUND_CARD_MAX];
    struct audio_device *dev;
};

struct stream_in {
    unsigned int device;
    struct pcm_config *config;
    struct pcm *pcm[SND_IN_SOUND_CARD_MAX];
    struct audio_device *dev;
};

#define BIT_HEADSET (1 << 0)
#define BIT_HEADSET_NO_MIC (1 << 1)
#define BIT_USB_HEADSET_ANLG (1 << 2)
#define BIT_USB_HEADSET_DGTL (1 << 3)
#define BIT_HDMI_AUDIO (1 << 4)
#define BIT_LINEOUT (1 << 5)
#define SUPPORTED_HEADSETS (BIT_HEADSET | BIT_HEADSET_NO_MIC | \
                            BIT_USB_HEADSET_ANLG | BIT_USB_HEADSET_DGTL |\
                            BIT_HDMI_AUDIO | BIT_LINEOUT)

struct StatePair
{
    int mask;
    int state;
};

void adev_open_init(struct audio_device *adev);
void adev_wired_init(struct audio_device *adev);
uint32_t getRouteFromDevice(uint32_t device);
#ifdef __cplusplus
}
#endif
#endif