#ifndef _RT5678_CONFIG_H_
#define _RT5678_CONFIG_H_

#include "config.h"

const struct config_control rt5678_speaker_normal_controls[] = {
	{
		.ctl_name = "DA STO1 ASRC Switch",
		.str_val = "clk_sys3",
	},
	{
		.ctl_name = "DAC1 Mux",
		.str_val = "IF3 DAC",
	},
	{
		.ctl_name = "DAC1 MIXL DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 MIXR DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXL DAC1 L Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXR DAC1 R Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 L Mixer Source Mux",
		.str_val = "Mixer",
	},
	{
		.ctl_name = "DAC1 R Mixer Source Mux",
		.str_val = "Mixer",
	},
	{
		.ctl_name = "DAC3 Source Mux",
		.str_val = "STO1 DAC MIX",
	},
	{
		.ctl_name = "LOUT1 Playback Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "LOUT2 Playback Switch",
		.int_val = {on},
	},
};

const struct config_control rt5678_headphone_normal_controls[] = {
	{
		.ctl_name = "DA STO1 ASRC Switch",
		.str_val = "clk_sys3",
	},
	{
		.ctl_name = "DAC1 Mux",
		.str_val = "IF3 DAC",
	},
	{
		.ctl_name = "DAC1 MIXL DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 MIXR DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXL DAC1 L Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXR DAC1 R Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 L Mixer Source Mux",
		.str_val = "Mixer",
	},
	{
		.ctl_name = "DAC1 R Mixer Source Mux",
		.str_val = "Mixer",
	},
	{
		.ctl_name = "DAC12 Source Mux",
		.str_val = "STO1 DAC MIX",
	},
};

const struct config_control rt5678_mono_normal_controls[] = {
	{
		.ctl_name = "DA STO1 ASRC Switch",
		.str_val = "clk_sys3",
	},
	{
		.ctl_name = "DAC1 Mux",
		.str_val = "IF3 DAC",
	},
	{
		.ctl_name = "DAC1 MIXL DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 MIXR DAC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXL DAC1 L Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Stereo DAC MIXR DAC1 R Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "DAC1 L Mixer Source Mux",
		.str_val = "Mixer",
	},
	{
		.ctl_name = "DAC5 Source Mux",
		.str_val = "STO1 DAC MIXL",
	},
};

const struct config_control rt5678_headset_mic_capture_controls[] = {
	{
		.ctl_name = "AD STO1 ASRC Switch",
		.str_val = "clk_sys3",
	},
	{
		.ctl_name = "Stereo1 ADC Mux",
		.str_val = "ADC12",
	},
	{
		.ctl_name = "IN1 Capture Volume",
		.int_val = {35},
	},
	{
		.ctl_name = "IN2 Capture Volume",
		.int_val = {35},
	},
	{
		.ctl_name = "Stereo1 ADC1 Mux",
		.str_val = "ADC/DMIC",
	},
	{
		.ctl_name = "Sto1 ADC MIXL ADC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "IF3 ADC Mux",
		.str_val =  "STO1 ADC MIX",
	},
	{
		.ctl_name = "IF1 ADC1 Swap Mux",
		.str_val =  "L/L",
	},
};

const struct config_control rt5678_main_mic_capture_controls[] = {
	{
		.ctl_name = "AD STO1 ASRC Switch",
		.str_val = "clk_sys3",
	},
	{
		.ctl_name = "Stereo1 ADC Mux",
		.str_val = "ADC34",
	},
	{
		.ctl_name = "IN3 Capture Volume",
		.int_val = {35},
	},
	{
		.ctl_name = "IN4 Capture Volume",
		.int_val = {35},
	},
	{
		.ctl_name = "Stereo1 ADC1 Mux",
		.str_val = "ADC/DMIC",
	},
	{
		.ctl_name = "Sto1 ADC MIXL ADC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "Sto1 ADC MIXR ADC1 Switch",
		.int_val = {on},
	},
	{
		.ctl_name = "IF3 ADC Mux",
		.str_val =  "STO1 ADC MIX",
	},
	{
		.ctl_name = "IF3 ADC Swap Mux",
		.str_val =  "R/R",
	},
};

const struct config_control rt5678_playback_off_controls[] = {
	{
		.ctl_name = "DAC1 MIXL DAC1 Switch",
		.int_val = {off},
	},
	{
		.ctl_name = "DAC1 MIXR DAC1 Switch",
		.int_val = {off},
	},
	{
		.ctl_name = "Stereo DAC MIXL DAC1 L Switch",
		.int_val = {off},
	},
	{
		.ctl_name = "Stereo DAC MIXR DAC1 R Switch",
		.int_val = {off},
	},
};

const struct config_control rt5678_capture_off_controls[] = {
	{
		.ctl_name = "Sto1 ADC MIXL ADC1 Switch",
		.int_val = {off},
	},
	{
		.ctl_name = "Sto1 ADC MIXR ADC1 Switch",
		.int_val = {off},
	},
	{
		.ctl_name = "IF1 ADC1 Swap Mux",
		.str_val =  "L/R",
	},
};

const struct config_route_table rt5678_config_table = {
	//speaker
	.speaker_normal = {
		.sound_card = 0,
		.devices = DEVICES_0,
		.controls = rt5678_speaker_normal_controls,
		.controls_count = sizeof(rt5678_speaker_normal_controls) / sizeof(struct config_control),
	},

	//headphone
	.headphone_normal = {
		.sound_card = 0,
		.devices = DEVICES_0,
		.controls = rt5678_headphone_normal_controls,
		.controls_count = sizeof(rt5678_headphone_normal_controls) / sizeof(struct config_control),
	},

#if 0
	//mono
	.mono_normal = {
		.sound_card = 0,
		.devices = DEVICES_0,
		.controls = rt5678_mono_normal_controls,
		.controls_count = sizeof(rt5678_mono_normal_controls) / sizeof(struct config_control),
	},
#endif

	//capture
	.hands_free_mic_capture = {
		.sound_card = 0,
		.devices = DEVICES_0,
		.controls = rt5678_headset_mic_capture_controls,
		.controls_count = sizeof(rt5678_headset_mic_capture_controls) / sizeof(struct config_control),
	},

	//capture
	.main_mic_capture = {
		.sound_card = 0,
		.devices = DEVICES_0,
		.controls = rt5678_main_mic_capture_controls,
		.controls_count = sizeof(rt5678_main_mic_capture_controls) / sizeof(struct config_control),
	},

	//off
	.playback_off = {
		.controls = rt5678_playback_off_controls,
		.controls_count = sizeof(rt5678_playback_off_controls) / sizeof(struct config_control),
	},
	.capture_off = {
		.controls = rt5678_capture_off_controls,
		.controls_count = sizeof(rt5678_capture_off_controls) / sizeof(struct config_control),
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

#endif //_RT5678_CONFIG_H_