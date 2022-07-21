/*
 * RkAiqConfigTranslatorV3x.h
 *
 *  Copyright (c) 2021 Rockchip Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RK_AIQ_RESOURCE_TRANSLATOR_V3X_H_
#define _RK_AIQ_RESOURCE_TRANSLATOR_V3X_H_

#include "RkAiqResourceTranslatorV21.h"

#define ISP3X_DHAZ_HIST_IIR_MAX     (1023)


namespace RkCam {

class RkAiqResourceTranslatorV3x
    : public RkAiqResourceTranslatorV21
{
public:
    struct Rectangle {
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    };

    RkAiqResourceTranslatorV3x ();
    virtual ~RkAiqResourceTranslatorV3x () = default;

    RkAiqResourceTranslatorV3x& SetMultiIspMode(bool isMultiIsp);
    RkAiqResourceTranslatorV3x& SetPicInfo(RkAiqResourceTranslatorV3x::Rectangle& pic_rect);
    RkAiqResourceTranslatorV3x& SetLeftIspRect(RkAiqResourceTranslatorV3x::Rectangle& left_isp_rect);
    RkAiqResourceTranslatorV3x& SetRightIspRect(RkAiqResourceTranslatorV3x::Rectangle& right_isp_rect);
    RkAiqResourceTranslatorV3x& SetPicInfo(RkAiqResourceTranslatorV3x::Rectangle&& pic_rect);
    RkAiqResourceTranslatorV3x& SetLeftIspRect(RkAiqResourceTranslatorV3x::Rectangle&& left_isp_rect);
    RkAiqResourceTranslatorV3x& SetRightIspRect(RkAiqResourceTranslatorV3x::Rectangle&& right_isp_rect);
    const bool IsMultiIspMode() const;
    const RkAiqResourceTranslatorV3x::Rectangle& GetPicInfo() const;
    const RkAiqResourceTranslatorV3x::Rectangle& GetLeftIspRect() const;
    const RkAiqResourceTranslatorV3x::Rectangle& GetRightIspRect() const;

    virtual XCamReturn translateAecStats (const SmartPtr<VideoBuffer> &from,
                                          SmartPtr<RkAiqAecStatsProxy> &to);
    virtual XCamReturn translateAwbStats (const SmartPtr<VideoBuffer> &from,
                                          SmartPtr<RkAiqAwbStatsProxy> &to);
    virtual XCamReturn translateAfStats (const SmartPtr<VideoBuffer> &from,
                                         SmartPtr<RkAiqAfStatsProxy> &to);
    virtual XCamReturn translateAdehazeStats (const SmartPtr<VideoBuffer> &from,
            SmartPtr<RkAiqAdehazeStatsProxy> &to);

    XCamReturn translateMultiIspStats(const SmartPtr<VideoBuffer>& from,
                                      SmartPtr<RkAiqIspStatsIntProxy>& to) {
        return XCAM_RETURN_NO_ERROR;
    }
    XCamReturn translateMultiAecStats(const SmartPtr<VideoBuffer>& from,
                                      SmartPtr<RkAiqAecStatsProxy>& to);
    XCamReturn translateMultiAwbStats(const SmartPtr<VideoBuffer>& from,
                                      SmartPtr<RkAiqAwbStatsProxy>& to);
    XCamReturn translateMultiAfStats(const SmartPtr<VideoBuffer>& from,
                                     SmartPtr<RkAiqAfStatsProxy>& to);
    XCamReturn translateMultiAdehazeStats(const SmartPtr<VideoBuffer>& from,
                                          SmartPtr<RkAiqAdehazeStatsProxy>& to);

private:
    XCAM_DEAD_COPY (RkAiqResourceTranslatorV3x);

    bool mIsMultiIsp;
    Rectangle pic_rect_;
    Rectangle left_isp_rect_;
    Rectangle right_isp_rect_;
};

}

#endif //_RK_AIQ_CONFIG_TRANSLATOR_H_
