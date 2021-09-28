/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
*/
/**
 * @file config_list.h
 * @brief 
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#ifndef _CONFIG_LIST_H_
#define _CONFIG_LIST_H_

#include "config.h"
#include "default_config.h"
#include "rk616_config.h"
#include "rt3261_config.h"
#include "rt5616_config.h"
#include "rt5640_config.h"
#include "rt5678_config.h"
#include "rt3224_config.h"
#include "wm8960_config.h"
#include "es8396_config.h"
#include "rt5651_config.h"
#include "es8316_config.h"
#include "cx2072_config.h"
#include "hdmi_analog_config.h"

struct alsa_sound_card_config
{
    const char *sound_card_name;
    const struct config_route_table *route_table;
};

/*
* List of sound card name and config table.
* Audio will get config_route_table and set route 
* according to the name of sound card 0 and sound_card_name.
*/
struct alsa_sound_card_config sound_card_config_list[] = {
    {
        .sound_card_name = "RKRK616",
        .route_table = &rk616_config_table,
    },
    {
        .sound_card_name = "RK29RT3224",
        .route_table = &rt3224_config_table,
    },
    {
        .sound_card_name = "rockchiprt5640c",
        .route_table = &rt5640_config_table,
    },
    {
        .sound_card_name = "realtekrt5678co",
        .route_table = &rt5678_config_table,
    },
    {
        .sound_card_name = "rockchipes8396c",
        .route_table = &es8396_config_table,
    },
    {
        .sound_card_name = "RK29RT3261",
        .route_table = &rt3261_config_table,
    },
    {
        .sound_card_name = "RK29WM8960",
        .route_table = &wm8960_config_table,
    },
    {
        .sound_card_name = "RKRT3224",
        .route_table = &rt3224_config_table,
    },
    {
        .sound_card_name = "RKRT3261",
        .route_table = &rt3261_config_table,
    },
    {
        .sound_card_name = "RKWM8960",
        .route_table = &wm8960_config_table,
    },
    {
        .sound_card_name = "RKRT5616",
        .route_table = &rt5616_config_table,
    },
    {
        .sound_card_name = "realtekrt5651co",
        .route_table = &rt5651_config_table,
    },
    {
        .sound_card_name = "rockchipes8316c",
        .route_table = &es8316_config_table,
    },
    {
        .sound_card_name = "rockchipcx2072x",
        .route_table = &cx2072_config_table,
    },
    {
        .sound_card_name = "rkhdmianalogsnd",
        .route_table = &hdmi_analog_config_table,
    }
};

#endif //_CONFIG_LIST_H_
