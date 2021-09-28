/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd. 
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

struct config_control
{
    const char *ctl_name; //name of control.
    const char *str_val; //value of control, which type is stream.
    const int int_val[2]; //left and right value of control, which type are int.
};

struct config_route
{
    const int sound_card;
    const int devices;
    const struct config_control *controls;
    const unsigned controls_count;
};

struct config_route_table
{
    const struct config_route speaker_normal;
    const struct config_route speaker_incall;
    const struct config_route speaker_ringtone;
    const struct config_route speaker_voip;

    const struct config_route earpiece_normal;
    const struct config_route earpiece_incall;
    const struct config_route earpiece_ringtone;
    const struct config_route earpiece_voip;

    const struct config_route headphone_normal;
    const struct config_route headphone_incall;
    const struct config_route headphone_ringtone;
    const struct config_route speaker_headphone_normal;
    const struct config_route speaker_headphone_ringtone;
    const struct config_route headphone_voip;

    const struct config_route headset_normal;
    const struct config_route headset_incall;
    const struct config_route headset_ringtone;
    const struct config_route headset_voip;

    const struct config_route bluetooth_normal;
    const struct config_route bluetooth_incall;
    const struct config_route bluetooth_voip;

    const struct config_route main_mic_capture;
    const struct config_route hands_free_mic_capture;
    const struct config_route bluetooth_sco_mic_capture;

    const struct config_route playback_off;
    const struct config_route capture_off;
    const struct config_route incall_off;
    const struct config_route voip_off;

    const struct config_route hdmi_normal;

    const struct config_route usb_normal;
    const struct config_route usb_capture;
};

#define on 1
#define off 0

#define DEVICES_0 0
#define DEVICES_0_1 1
#define DEVICES_0_2 2
#define DEVICES_0_1_2 3

#endif //_CONFIG_H_
