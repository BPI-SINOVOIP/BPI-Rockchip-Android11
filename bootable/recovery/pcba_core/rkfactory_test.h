/*
* Copyright 2020 Rockchip Electronics Co., Ltd
* Kenjc <kenjc.bian@rock-chips.com>
*
* SPDX-License-Identifier:	GPL-2.0+
*/

#ifndef RKFACTORY_TEST_HEADER
#define RKFACTORY_TEST_HEADER
#include <string>
#include <vector>

#include "recovery_ui/device.h"
Device::BuiltinAction StartFactorytest(Device* device);
class RKFactory{
    public:
    RKFactory();
    int StartFactorytest(Device* device);
    bool isRKFactory();
    bool bRKFactory;
};

#endif
