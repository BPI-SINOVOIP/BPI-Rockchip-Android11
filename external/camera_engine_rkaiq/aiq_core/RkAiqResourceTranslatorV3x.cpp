/*
 * RkAiqConfigTranslatorV3x.cpp
 *
 *  Copyright (c) 2019 Rockchip Corporation
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

#include "isp20/Isp20Evts.h"
#include "isp20/Isp20StatsBuffer.h"
#include "common/rkisp2-config.h"
#include "common/rkisp21-config.h"
#include "common/rkisp3-config.h"
#include "RkAiqResourceTranslatorV3x.h"
//#define AE_STATS_DEBUG

namespace RkCam {

RkAiqResourceTranslatorV3x::RkAiqResourceTranslatorV3x() : mIsMultiIsp(false) {}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetMultiIspMode(bool isMultiIsp) {
    mIsMultiIsp = isMultiIsp;
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetPicInfo(RkAiqResourceTranslatorV3x::Rectangle& pic_rect) {
    pic_rect_ = pic_rect;
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetLeftIspRect(RkAiqResourceTranslatorV3x::Rectangle& left_isp_rect) {
    left_isp_rect_ = left_isp_rect;
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetRightIspRect(
    RkAiqResourceTranslatorV3x::Rectangle& right_isp_rect) {
    right_isp_rect_ = right_isp_rect;
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetPicInfo(RkAiqResourceTranslatorV3x::Rectangle&& pic_rect) {
    pic_rect_ = std::move(pic_rect);
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetLeftIspRect(RkAiqResourceTranslatorV3x::Rectangle&& left_isp_rect) {
    left_isp_rect_ = std::move(left_isp_rect);
    return *this;
}

RkAiqResourceTranslatorV3x& RkAiqResourceTranslatorV3x::SetRightIspRect(
    RkAiqResourceTranslatorV3x::Rectangle&& right_isp_rect) {
    right_isp_rect_ = std::move(right_isp_rect);
    return *this;
}

const bool RkAiqResourceTranslatorV3x::IsMultiIspMode() const {
    return mIsMultiIsp;
}

const RkAiqResourceTranslatorV3x::Rectangle& RkAiqResourceTranslatorV3x::GetPicInfo() const {
    return pic_rect_;
}

const RkAiqResourceTranslatorV3x::Rectangle& RkAiqResourceTranslatorV3x::GetLeftIspRect() const {
    return left_isp_rect_;
}

const RkAiqResourceTranslatorV3x::Rectangle& RkAiqResourceTranslatorV3x::GetRightIspRect() const {
    return right_isp_rect_;
}

void JudgeWinLocation(
    struct isp2x_window* ori_win,
    WinSplitMode& mode,
    RkAiqResourceTranslatorV3x::Rectangle left_isp_rect_,
    RkAiqResourceTranslatorV3x::Rectangle right_isp_rect_
) {

    if (ori_win->h_offs + ori_win->h_size <= left_isp_rect_.w) {
        mode = LEFT_MODE;
    } else if(ori_win->h_offs >= right_isp_rect_.x) {
        mode = RIGHT_MODE;
    } else {
        if ((ori_win->h_offs + ori_win->h_size / 2) <= left_isp_rect_.w
                && right_isp_rect_.x <= (ori_win->h_offs + ori_win->h_size / 2)) {
            mode = LEFT_AND_RIGHT_MODE;
        }
        else {

            if ((ori_win->h_offs + ori_win->h_size / 2) < right_isp_rect_.x) {

                u16 h_size_tmp1 = left_isp_rect_.w - ori_win->h_offs;
                u16 h_size_tmp2 = (right_isp_rect_.x - ori_win->h_offs) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2))
                    mode = LEFT_MODE;
                else
                    mode = LEFT_AND_RIGHT_MODE;
            }
            else {

                u16 h_size_tmp1 = ori_win->h_offs + ori_win->h_size - right_isp_rect_.x;
                u16 h_size_tmp2 = (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2))
                    mode = RIGHT_MODE;
                else
                    mode = LEFT_AND_RIGHT_MODE;
            }
        }
    }
}

void MergeAecWinLiteStats(
    rawaelite_stat_t *merge_stats,
    struct isp2x_rawaelite_stat *left_stats,
    struct isp2x_rawaelite_stat *right_stats,
    WinSplitMode mode,
    struct isp2x_bls_fixed_val bls1_val,
    float* bls_ratio
) {
    u8 wnd_num = sqrt(ISP3X_RAWAELITE_MEAN_NUM);

    for(int i = 0; i < wnd_num; i++) {
        for(int j = 0; j < wnd_num; j++) {

            // step1 copy stats
            switch(mode) {
            case LEFT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case RIGHT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case LEFT_AND_RIGHT_MODE:

                if(j < wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelr_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelg_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelb_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelb_xy) / 2;
                } else if(j > wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelr_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelg_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelb_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelb_xy) / 2;
                } else {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelr_xy + right_stats->data[i * wnd_num + 0].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelg_xy + right_stats->data[i * wnd_num + 0].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelb_xy + right_stats->data[i * wnd_num + 0].channelb_xy) / 2;
                }
                break;
            default:
                break;
            }

            // step2 subtract bls1
            merge_stats->channelr_xy[i * wnd_num + j] = (u16)((merge_stats->channelr_xy[i * wnd_num + j] - bls1_val.r) * bls_ratio[0]);
            merge_stats->channelg_xy[i * wnd_num + j] = (u16)((merge_stats->channelg_xy[i * wnd_num + j] - bls1_val.gr) * bls_ratio[1]);
            merge_stats->channelb_xy[i * wnd_num + j] = (u16)((merge_stats->channelb_xy[i * wnd_num + j] - bls1_val.b) * bls_ratio[2]);

        }
    }
}
void MergeAecWinBigStats(
    rawaebig_stat_t *merge_stats,
    struct isp2x_rawaebig_stat *left_stats,
    struct isp2x_rawaebig_stat *right_stats,
    WinSplitMode mode,
    struct isp2x_bls_fixed_val bls1_val,
    float* bls_ratio
) {
    u8 wnd_num = sqrt(ISP3X_RAWAEBIG_MEAN_NUM);
    for(int i = 0; i < wnd_num; i++) {
        for(int j = 0; j < wnd_num; j++) {

            // step1 copy stats
            switch(mode) {
            case LEFT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case RIGHT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case LEFT_AND_RIGHT_MODE:
                if(j < wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelr_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelg_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelb_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelb_xy) / 2;
                } else if(j > wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelr_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelg_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelb_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelb_xy) / 2;
                } else {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelr_xy + right_stats->data[i * wnd_num + 0].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelg_xy + right_stats->data[i * wnd_num + 0].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelb_xy + right_stats->data[i * wnd_num + 0].channelb_xy) / 2;
                }
                break;
            default:
                break;
            }

            // step2 subtract bls1
            merge_stats->channelr_xy[i * wnd_num + j] = (u16)((merge_stats->channelr_xy[i * wnd_num + j] - bls1_val.r) * bls_ratio[0]);
            merge_stats->channelg_xy[i * wnd_num + j] = (u16)((merge_stats->channelg_xy[i * wnd_num + j] - bls1_val.gr) * bls_ratio[1]);
            merge_stats->channelb_xy[i * wnd_num + j] = (u16)((merge_stats->channelb_xy[i * wnd_num + j] - bls1_val.b) * bls_ratio[2]);

        }
    }

}
void MergeAecSubWinStats(
    rawaebig_stat_t *merge_stats,
    struct isp2x_rawaebig_stat *left_stats,
    struct isp2x_rawaebig_stat *right_stats,
    u8 *left_en,
    u8 *right_en,
    struct isp2x_bls_fixed_val bls1_val,
    float* bls_ratio,
    u32* pixel_num
) {
    for(int i = 0; i < ISP3X_RAWAEBIG_SUBWIN_NUM; i++) {
        // step1 copy stats
        merge_stats->wndx_sumr[i] = ((left_en[i]) ? left_stats->sumr[i] : 0) + ((right_en[i]) ? right_stats->sumr[i] : 0);
        merge_stats->wndx_sumg[i] = ((left_en[i]) ? left_stats->sumg[i] : 0) + ((right_en[i]) ? right_stats->sumg[i] : 0);
        merge_stats->wndx_sumb[i] = ((left_en[i]) ? left_stats->sumb[i] : 0) + ((right_en[i]) ? right_stats->sumb[i] : 0);

        // step2 subtract bls1
        if(left_en[i] == 1 || right_en[i] == 1) {
            merge_stats->wndx_sumr[i] = (u32)((merge_stats->wndx_sumr[i] - (pixel_num[i] >> 2) * bls1_val.r) * bls_ratio[0]);
            merge_stats->wndx_sumg[i] = (u32)((merge_stats->wndx_sumg[i] - (pixel_num[i] >> 1) * bls1_val.gr) * bls_ratio[1]);
            merge_stats->wndx_sumb[i] = (u32)((merge_stats->wndx_sumb[i] - (pixel_num[i] >> 2) * bls1_val.b) * bls_ratio[2]);
        }
    }
}
void MergeAecHistBinStats(
    u32 *merge_stats,
    u32 *left_stats,
    u32 *right_stats,
    WinSplitMode mode,
    s16   bls1_val,
    float bls_ratio
) {
    int tmp;
    memset(merge_stats, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));

    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
            tmp = (i - bls1_val >= 0) ? (i - bls1_val) * bls_ratio + 0.5 : 0;
            if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                tmp = ISP3X_HIST_BIN_N_MAX - 1;
            merge_stats[tmp] += left_stats[i];
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
            tmp = (i - bls1_val >= 0) ? (i - bls1_val) * bls_ratio + 0.5 : 0;
            if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                tmp = ISP3X_HIST_BIN_N_MAX - 1;
            merge_stats[tmp] += right_stats[i];
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
            tmp = (i - bls1_val >= 0) ? (i - bls1_val) * bls_ratio + 0.5 : 0;
            if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                tmp = ISP3X_HIST_BIN_N_MAX - 1;
            merge_stats[tmp] += left_stats[i] + right_stats[i];
        }
        break;
    }

}

void MergeAwbWinStats(
    rk_aiq_awb_stat_wp_res_light_v201_t *merge_stats,
    struct isp3x_rawawb_meas_stat *left_stats,
    struct isp3x_rawawb_meas_stat *right_stats,
    int lightNum,
    WinSplitMode mode
) {
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue = left_stats->ro_rawawb_sum_rgain_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue = left_stats->ro_rawawb_sum_bgain_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo = left_stats->ro_rawawb_wp_num_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue = left_stats->ro_rawawb_sum_rgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue = left_stats->ro_rawawb_sum_bgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo = left_stats->ro_rawawb_wp_num_big[i];
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue = right_stats->ro_rawawb_sum_rgain_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue = right_stats->ro_rawawb_sum_bgain_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo = right_stats->ro_rawawb_wp_num_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue = right_stats->ro_rawawb_sum_rgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue = right_stats->ro_rawawb_sum_bgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo = right_stats->ro_rawawb_wp_num_big[i];
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
                left_stats->ro_rawawb_sum_rgain_nor[i] + right_stats->ro_rawawb_sum_rgain_nor[i] ;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
                left_stats->ro_rawawb_sum_bgain_nor[i] +  right_stats->ro_rawawb_sum_bgain_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
                left_stats->ro_rawawb_wp_num_nor[i] +  right_stats->ro_rawawb_wp_num_nor[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
                left_stats->ro_rawawb_sum_rgain_big[i] +  right_stats->ro_rawawb_sum_rgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
                left_stats->ro_rawawb_sum_bgain_big[i] +  right_stats->ro_rawawb_sum_bgain_big[i];
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
                left_stats->ro_rawawb_wp_num_big[i] +  right_stats->ro_rawawb_wp_num_big[i];
        }
        break;
    default:
        break;
    }
}

void AwbStatOverflowCheckandFixed(struct isp2x_window* win, rk_aiq_awb_blk_stat_mode_v201_t blkMeasureMode, bool blkStatisticsWithLumaWeightEn, rk_aiq_awb_xy_type_v201_t xyRangeTypeForWpHist,
                                  int lightNum, struct isp3x_rawawb_meas_stat *stats)
{
    int w, h;
    w = win->h_size;
    h = win->v_size;
    float factor1 = (float)((1 << (RK_AIQ_AWB_WP_WEIGHT_BIS_V201 + 1)) - 1) / ((1 << RK_AIQ_AWB_WP_WEIGHT_BIS_V201) - 1);
    if(w * h > RK_AIQ_AWB_STAT_MAX_AREA) {
        LOGD_AWB("%s ramdata and ro_wp_num2 is fixed", __FUNCTION__);
        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
            stats->ramdata[i].wp = (float)stats->ramdata[i].wp * factor1 + 0.5 ;
            stats->ramdata[i].r = (float)stats->ramdata[i].r * factor1 + 0.5 ;
            stats->ramdata[i].g = (float)stats->ramdata[i].g * factor1 + 0.5 ;
            stats->ramdata[i].b = (float)stats->ramdata[i].b * factor1 + 0.5 ;
        }
        if(xyRangeTypeForWpHist == RK_AIQ_AWB_XY_TYPE_BIG_V201) {
            for(int i = 0; i < lightNum; i++) {
                stats->ro_wp_num2[i] = stats->ro_rawawb_wp_num_big[i] >> RK_AIQ_WP_GAIN_FRAC_BIS;
            }
        } else {
            for(int i = 0; i < lightNum; i++) {
                stats->ro_wp_num2[i] = stats->ro_rawawb_wp_num_nor[i] >> RK_AIQ_WP_GAIN_FRAC_BIS;
            }
        }
    } else {
        if(RK_AIQ_AWB_BLK_STAT_MODE_REALWP_V201 == blkMeasureMode && blkStatisticsWithLumaWeightEn) {
            for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
                stats->ramdata[i].wp = (float)stats->ramdata[i].wp * factor1 + 0.5 ;
                stats->ramdata[i].r = (float)stats->ramdata[i].r * factor1 + 0.5 ;
                stats->ramdata[i].g = (float)stats->ramdata[i].g * factor1 + 0.5 ;
                stats->ramdata[i].b = (float)stats->ramdata[i].b * factor1 + 0.5 ;
            }
        }
    }
}

void MergeAwbBlkStats(
    struct isp2x_window* ori_win,
    struct isp2x_window* left_win,
    struct isp2x_window* right_win,
    rk_aiq_awb_stat_blk_res_v201_t *merge_stats,
    struct isp3x_rawawb_meas_stat *left_stats,
    struct isp3x_rawawb_meas_stat *right_stats,
    WinSplitMode mode
) {
    u8 wnd_num = sqrt(RK_AIQ_AWB_GRID_NUM_TOTAL);

    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + j].r;
                merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + j].g;
                merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + j].b;
                merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + j].wp;
            }
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                merge_stats[i * wnd_num + j].Rvalue = right_stats->ramdata[i * wnd_num + j].r;
                merge_stats[i * wnd_num + j].Gvalue = right_stats->ramdata[i * wnd_num + j].g;
                merge_stats[i * wnd_num + j].Bvalue = right_stats->ramdata[i * wnd_num + j].b;
                merge_stats[i * wnd_num + j].WpNo = right_stats->ramdata[i * wnd_num + j].wp;
            }
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                if(j < wnd_num / 2) {
                    merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + j * 2].r + left_stats->ramdata[i * wnd_num + j * 2 + 1].r;
                    merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + j * 2].g + left_stats->ramdata[i * wnd_num + j * 2 + 1].g;
                    merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + j * 2].b +  left_stats->ramdata[i * wnd_num + j * 2 + 1].b;
                    merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + j * 2].wp + left_stats->ramdata[i * wnd_num + j * 2 + 1].wp;
                } else if(j > wnd_num / 2) {
                    merge_stats[i * wnd_num + j].Rvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].r + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].r;
                    merge_stats[i * wnd_num + j].Gvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].g + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].g;
                    merge_stats[i * wnd_num + j].Bvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].b + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].b;
                    merge_stats[i * wnd_num + j].WpNo = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].wp + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].wp;
                } else {
                    merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].r + right_stats->ramdata[i * wnd_num + 0].r;
                    merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].g + right_stats->ramdata[i * wnd_num + 0].g;
                    merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].b + right_stats->ramdata[i * wnd_num + 0].b;
                    merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + wnd_num - 1].wp + right_stats->ramdata[i * wnd_num + 0].wp;
                }
            }
        }
        break;
    default:
        break;
    }

}

void MergeAwbHistBinStats(
    unsigned int *merge_stats,
    u16 *left_stats,
    u16 *right_stats,
    WinSplitMode mode
) {
    u32 tmp1, tmp2;
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp1 = left_stats[i];
            if(left_stats[i] & 0x8000) {
                tmp1 = left_stats[i] & 0x7FFF;
                tmp1 *=    (1 << 3);
            }
            merge_stats[i] = tmp1;
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp2 = right_stats[i];
            if(right_stats[i] & 0x8000) {
                tmp2 = right_stats[i] & 0x7FFF;
                tmp2 *=    (1 << 3);
            }
            merge_stats[i] = tmp2;
        }

        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp1 = left_stats[i];
            if(left_stats[i] & 0x8000) {
                tmp1 = left_stats[i] & 0x7FFF;
                tmp1 *=    (1 << 3);
            }
            tmp2 = right_stats[i];
            if(right_stats[i] & 0x8000) {
                tmp2 = right_stats[i] & 0x7FFF;
                tmp2 *=    (1 << 3);
            }
            merge_stats[i] = tmp1 + tmp2;

        }
        break;
    }
}

#if defined(ISP_HW_V30)

void MergeAwbMultiWinStats(
    rk_aiq_isp_awb_stats_v3x_t *merge_stats,
    struct isp3x_rawawb_meas_stat *left_stats,
    struct isp3x_rawawb_meas_stat *right_stats
) {
    for(int i = 0; i < RK_AIQ_AWB_MULTIWINDOW_NUM_V201; i++) {

        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
            left_stats->ro_sum_r_nor_multiwindow[i] + right_stats->ro_sum_r_nor_multiwindow[i];
        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
            left_stats->ro_sum_b_nor_multiwindow[i] + right_stats->ro_sum_b_nor_multiwindow[i];
        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
            left_stats->ro_wp_nm_nor_multiwindow[i] + right_stats->ro_wp_nm_nor_multiwindow[i];
        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
            left_stats->ro_sum_r_big_multiwindow[i] + right_stats->ro_sum_r_big_multiwindow[i];
        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
            left_stats->ro_sum_b_big_multiwindow[i] + right_stats->ro_sum_b_big_multiwindow[i];
        merge_stats->multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
            left_stats->ro_wp_nm_big_multiwindow[i] + right_stats->ro_wp_nm_big_multiwindow[i];

    }
}

void MergeAwbExcWpStats(
    rk_aiq_awb_stat_wp_res_v201_t*merge_stats,
    isp3x_rawawb_meas_stat *left_stats,
    isp3x_rawawb_meas_stat *right_stats,
    WinSplitMode mode
) {
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = left_stats->ro_sum_r_exc[i];
            merge_stats[i].RgainValue = left_stats->ro_sum_b_exc[i];
            merge_stats[i].RgainValue = left_stats->ro_wp_nm_exc[i];
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = right_stats->ro_sum_r_exc[i];
            merge_stats[i].RgainValue = right_stats->ro_sum_b_exc[i];
            merge_stats[i].RgainValue = right_stats->ro_wp_nm_exc[i];
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = left_stats->ro_sum_r_exc[i] + right_stats->ro_sum_r_exc[i];
            merge_stats[i].RgainValue = left_stats->ro_sum_b_exc[i] + right_stats->ro_sum_b_exc[i];
            merge_stats[i].RgainValue = left_stats->ro_wp_nm_exc[i] + right_stats->ro_sum_b_exc[i];
        }
        break;
    }
}
#endif

XCamReturn
RkAiqResourceTranslatorV3x::translateMultiAecStats(const SmartPtr<VideoBuffer>& from, SmartPtr<RkAiqAecStatsProxy>& to)
{

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();

    SmartPtr<RkAiqAecStats> statsInt = to->data();

    struct rkisp3x_isp_stat_buffer *left_stats;
    struct rkisp3x_isp_stat_buffer *right_stats;

    left_stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(left_stats == NULL) {
        LOGE("fail to get left stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    right_stats = left_stats + 1;
    if(right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type)
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
    else
        LOGD_ANALYZER("camId: %d, stats: frame_id: %d,  meas_type; 0x%x", mCamPhyId, left_stats->frame_id, left_stats->meas_type);

    SmartPtr<RkAiqIrisParamsProxy> irisParams = buf->get_iris_params();
    SmartPtr<RkAiqExpParamsProxy> expParams = nullptr;
    rkisp_effect_params_v20 ispParams = {0};
    if (buf->getEffectiveExpParams(left_stats->frame_id, expParams) < 0)
        LOGE("fail to get expParams");
    if (buf->getEffectiveIspParams(left_stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    //ae stats v3.x

    statsInt->frame_id = left_stats->frame_id;

    /*rawae stats*/
    uint8_t AeSwapMode, AeSelMode, AfUseAeBig;
    AeSwapMode = ispParams.isp_params_v3x[0].meas.rawae0.rawae_sel;
    AeSelMode = ispParams.isp_params_v3x[0].meas.rawae3.rawae_sel;
    AfUseAeBig = ispParams.isp_params_v3x[0].meas.rawaf.ae_mode;
    unsigned int meas_type = 0;

    WinSplitMode AeWinSplitMode[4] = {LEFT_AND_RIGHT_MODE}; //0:rawae0 1:rawae1 2:rawae2 3:rawae3
    WinSplitMode HistWinSplitMode[4] = {LEFT_AND_RIGHT_MODE}; //0:rawhist0 1:rawhist1 2:rawhist2 3:rawhist3

    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawae0.win, AeWinSplitMode[0], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawae1.win, AeWinSplitMode[1], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawae2.win, AeWinSplitMode[2], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawae3.win, AeWinSplitMode[3], left_isp_rect_, right_isp_rect_);

    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawhist0.win, HistWinSplitMode[0], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawhist1.win, HistWinSplitMode[1], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawhist2.win, HistWinSplitMode[2], left_isp_rect_, right_isp_rect_);
    JudgeWinLocation(&ispParams.isp_params_v3x[0].meas.rawhist3.win, HistWinSplitMode[3], left_isp_rect_, right_isp_rect_);

    struct isp21_bls_cfg *bls_cfg = &ispParams.isp_params_v3x[0].others.bls_cfg;
    struct isp2x_bls_fixed_val bls1_val; //bls1_val = blc1_ori_val * awb * range_ratio
    float bls_ratio[3] = {1, 1, 1};
    u32 pixel_num[ISP3X_RAWAEBIG_SUBWIN_NUM] = {0};

    if(bls_cfg->bls1_en) {

        bls1_val.r = bls_cfg->bls1_val.r >> 2;
        bls1_val.gr = bls_cfg->bls1_val.gr;
        bls1_val.gb = bls_cfg->bls1_val.gb;
        bls1_val.b = bls_cfg->bls1_val.b >> 2;

        bls_ratio[0] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.r);
        bls_ratio[1] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.gr);
        bls_ratio[2] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.b);

    } else {

        bls1_val.r = 0;
        bls1_val.gr = 0;
        bls1_val.gb = 0;
        bls1_val.b = 0;

    }

#ifdef AE_STATS_DEBUG
    LOGE("bls1[%d-%d-%d-%d]", bls1_val.r, bls1_val.gr, bls1_val.gb, bls1_val.b);
    LOGE("bls_ratio[%f-%f-%f]", bls_ratio[0], bls_ratio[1], bls_ratio[2]);
#endif

    s16   hist_bls1;
    float hist_bls_ratio;

    switch(AeSwapMode)
    {
    case AEC_RAWSWAP_MODE_S_LITE:

        switch(ispParams.isp_params_v3x[0].meas.rawhist0.mode) {
        case 2:
            hist_bls1 = (bls1_val.r >> 2);
            hist_bls_ratio = bls_ratio[0];
            break;
        case 3:
            hist_bls1 = (bls1_val.gr >> 4);
            hist_bls_ratio = bls_ratio[1];
            break;
        case 4:
            hist_bls1 = (bls1_val.b >> 2);
            hist_bls_ratio = bls_ratio[2];
            break;
        case 5:
        default:
            hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
            hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
            break;
        }

        meas_type = ((left_stats->meas_type >> 7) & (0x01)) & ((left_stats->meas_type >> 11) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        //chn 0 => rawae0 rawhist0
        MergeAecWinLiteStats(&statsInt->aec_stats.ae_data.chn[0].rawae_lite, &left_stats->params.rawae0, &right_stats->params.rawae0, AeWinSplitMode[0], bls1_val, bls_ratio);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins, left_stats->params.rawhist0.hist_bin,
                             right_stats->params.rawhist0.hist_bin, HistWinSplitMode[0], hist_bls1, hist_bls_ratio);

        //chn 1 => rawae1 rawhist1
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[1].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1, AeWinSplitMode[1], bls1_val, bls_ratio);
        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].v_size;
        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[1].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1,
                            ispParams.isp_params_v3x[1].meas.rawae1.subwin_en, ispParams.isp_params_v3x[2].meas.rawae1.subwin_en,
                            bls1_val, bls_ratio, pixel_num);

        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, left_stats->params.rawhist1.hist_bin,
                             right_stats->params.rawhist1.hist_bin, HistWinSplitMode[1], hist_bls1, hist_bls_ratio);


        //chn 2 => rawae2 rawhist2
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[2].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2, AeWinSplitMode[2], bls1_val, bls_ratio);
        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].v_size;

        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[2].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2,
                            ispParams.isp_params_v3x[1].meas.rawae2.subwin_en, ispParams.isp_params_v3x[2].meas.rawae2.subwin_en,
                            bls1_val, bls_ratio, pixel_num);


        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins, left_stats->params.rawhist2.hist_bin,
                             right_stats->params.rawhist2.hist_bin, HistWinSplitMode[2], hist_bls1, hist_bls_ratio);

        break;

    case AEC_RAWSWAP_MODE_M_LITE:
        switch(ispParams.isp_params_v3x[0].meas.rawhist1.mode) {
        case 2:
            hist_bls1 = (bls1_val.r >> 2);
            hist_bls_ratio = bls_ratio[0];
            break;
        case 3:
            hist_bls1 = (bls1_val.gr >> 4);
            hist_bls_ratio = bls_ratio[1];
            break;
        case 4:
            hist_bls1 = (bls1_val.b >> 2);
            hist_bls_ratio = bls_ratio[2];
            break;
        case 5:
        default:
            hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
            hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
            break;
        }

        meas_type = ((left_stats->meas_type >> 8) & (0x01)) & ((left_stats->meas_type >> 12) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        //chn 0 => rawae1 rawhist1
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[0].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1, AeWinSplitMode[1], bls1_val, bls_ratio);

        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].v_size;

        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[0].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1,
                            ispParams.isp_params_v3x[1].meas.rawae1.subwin_en, ispParams.isp_params_v3x[2].meas.rawae1.subwin_en,
                            bls1_val, bls_ratio, pixel_num);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, left_stats->params.rawhist1.hist_bin,
                             right_stats->params.rawhist1.hist_bin, HistWinSplitMode[1], hist_bls1, hist_bls_ratio);

        //chn 1 => rawae0 rawhist0
        MergeAecWinLiteStats(&statsInt->aec_stats.ae_data.chn[1].rawae_lite, &left_stats->params.rawae0, &right_stats->params.rawae0, AeWinSplitMode[0], bls1_val, bls_ratio);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[1].rawhist_lite.bins, left_stats->params.rawhist0.hist_bin,
                             right_stats->params.rawhist0.hist_bin, HistWinSplitMode[0], hist_bls1, hist_bls_ratio);

        //chn 2 => rawae2 rawhist2
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[2].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2, AeWinSplitMode[2], bls1_val, bls_ratio);

        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].v_size;

        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[2].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2,
                            ispParams.isp_params_v3x[1].meas.rawae2.subwin_en, ispParams.isp_params_v3x[2].meas.rawae2.subwin_en,
                            bls1_val, bls_ratio, pixel_num);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins, left_stats->params.rawhist2.hist_bin,
                             right_stats->params.rawhist2.hist_bin, HistWinSplitMode[2], hist_bls1, hist_bls_ratio);

        break;

    case AEC_RAWSWAP_MODE_L_LITE:
        switch(ispParams.isp_params_v3x[0].meas.rawhist2.mode) {
        case 2:
            hist_bls1 = (bls1_val.r >> 2);
            hist_bls_ratio = bls_ratio[0];
            break;
        case 3:
            hist_bls1 = (bls1_val.gr >> 4);
            hist_bls_ratio = bls_ratio[1];
            break;
        case 4:
            hist_bls1 = (bls1_val.b >> 2);
            hist_bls_ratio = bls_ratio[2];
            break;
        case 5:
        default:
            hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
            hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
            break;
        }

        meas_type = ((left_stats->meas_type >> 9) & (0x01)) & ((left_stats->meas_type >> 13) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        //chn 0 => rawae2 rawhist2
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[0].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2, AeWinSplitMode[2], bls1_val, bls_ratio);
        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae2.subwin[3].v_size;
        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[0].rawae_big, &left_stats->params.rawae2, &right_stats->params.rawae2,
                            ispParams.isp_params_v3x[1].meas.rawae2.subwin_en, ispParams.isp_params_v3x[2].meas.rawae2.subwin_en,
                            bls1_val, bls_ratio, pixel_num);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, left_stats->params.rawhist2.hist_bin,
                             right_stats->params.rawhist2.hist_bin, HistWinSplitMode[2], hist_bls1, hist_bls_ratio);

        //chn 1 => rawae1 rawhist1
        MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[1].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1, AeWinSplitMode[1], bls1_val, bls_ratio);

        pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[0].v_size;
        pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[1].v_size;
        pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[2].v_size;
        pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae1.subwin[3].v_size;
        MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[1].rawae_big, &left_stats->params.rawae1, &right_stats->params.rawae1,
                            ispParams.isp_params_v3x[1].meas.rawae1.subwin_en, ispParams.isp_params_v3x[2].meas.rawae1.subwin_en,
                            bls1_val, bls_ratio, pixel_num);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, left_stats->params.rawhist1.hist_bin,
                             right_stats->params.rawhist1.hist_bin, HistWinSplitMode[1], hist_bls1, hist_bls_ratio);

        //chn 2 => rawae0 rawhist0
        MergeAecWinLiteStats(&statsInt->aec_stats.ae_data.chn[2].rawae_lite, &left_stats->params.rawae0, &right_stats->params.rawae0, AeWinSplitMode[0], bls1_val, bls_ratio);
        MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[2].rawhist_lite.bins, left_stats->params.rawhist0.hist_bin,
                             right_stats->params.rawhist0.hist_bin, HistWinSplitMode[0], hist_bls1, hist_bls_ratio);

        break;

    default:
        LOGE("wrong AeSwapMode=%d\n", AeSwapMode);
        return XCAM_RETURN_ERROR_PARAM;
        break;
    }

    statsInt->af_prior = (AfUseAeBig == 0) ? false : true;

    if(!AfUseAeBig) {
        switch(AeSelMode) {
        case AEC_RAWSEL_MODE_CHN_0:
        case AEC_RAWSEL_MODE_CHN_1:
        case AEC_RAWSEL_MODE_CHN_2:
            //chn 0 => rawae3 rawhist3
            switch(ispParams.isp_params_v3x[0].meas.rawhist3.mode) {
            case 2:
                hist_bls1 = (bls1_val.r >> 2);
                hist_bls_ratio = bls_ratio[0];
                break;
            case 3:
                hist_bls1 = (bls1_val.gr >> 4);
                hist_bls_ratio = bls_ratio[1];
                break;
            case 4:
                hist_bls1 = (bls1_val.b >> 2);
                hist_bls_ratio = bls_ratio[2];
                break;
            case 5:
            default:
                hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
                hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
                break;
            }

            MergeAecWinBigStats(&statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big, &left_stats->params.rawae3, &right_stats->params.rawae3, AeWinSplitMode[3], bls1_val, bls_ratio);
            pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[0].v_size;
            pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[1].v_size;
            pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[2].v_size;
            pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[3].v_size;
            MergeAecSubWinStats(&statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big, &left_stats->params.rawae3, &right_stats->params.rawae3,
                                ispParams.isp_params_v3x[1].meas.rawae3.subwin_en, ispParams.isp_params_v3x[2].meas.rawae3.subwin_en,
                                bls1_val, bls_ratio, pixel_num);

            MergeAecHistBinStats(statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins, left_stats->params.rawhist3.hist_bin,
                                 right_stats->params.rawhist3.hist_bin, HistWinSplitMode[3], hist_bls1, hist_bls_ratio);

            break;
        case AEC_RAWSEL_MODE_TMO:

            bls1_val.r = 0;
            bls1_val.gr = 0;
            bls1_val.gb = 0;
            bls1_val.b = 0;

            bls_ratio[0] = 1;
            bls_ratio[1] = 1;
            bls_ratio[2] = 1;

            //extra => rawae3 rawhist3
            MergeAecWinBigStats(&statsInt->aec_stats.ae_data.extra.rawae_big, &left_stats->params.rawae3, &right_stats->params.rawae3, AeWinSplitMode[3], bls1_val, bls_ratio);
            pixel_num[0] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[0].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[0].v_size;
            pixel_num[1] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[1].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[1].v_size;
            pixel_num[2] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[2].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[2].v_size;
            pixel_num[3] = ispParams.isp_params_v3x[0].meas.rawae3.subwin[3].h_size * ispParams.isp_params_v3x[0].meas.rawae3.subwin[3].v_size;
            MergeAecSubWinStats(&statsInt->aec_stats.ae_data.extra.rawae_big, &left_stats->params.rawae3, &right_stats->params.rawae3,
                                ispParams.isp_params_v3x[1].meas.rawae3.subwin_en, ispParams.isp_params_v3x[2].meas.rawae3.subwin_en,
                                bls1_val, bls_ratio, pixel_num);
            hist_bls1 = 0;
            hist_bls_ratio = 1;

            MergeAecHistBinStats(statsInt->aec_stats.ae_data.extra.rawhist_big.bins, left_stats->params.rawhist3.hist_bin,
                                 right_stats->params.rawhist3.hist_bin, HistWinSplitMode[3], hist_bls1, hist_bls_ratio);

            break;

        default:
            LOGE("wrong AeSelMode=%d\n", AeSelMode);
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

#ifdef AE_STATS_DEBUG
    if(AeSwapMode != 0) {
        for(int i = 0; i < 15; i++) {
            for(int j = 0; j < 15; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i * 15 + j]);
            }
        }
        printf("====================sub-win-result======================\n");

        for(int i = 0; i < 4; i++)
            printf("chn0_subwin[%d]:sumr 0x%08lx, sumg 0x%08lx, sumb 0x%08lx\n", i,
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i]);

        printf("====================hist_result========================\n");
        for(int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[i]);

    } else {
        for(int i = 0; i < 5; i++) {
            for(int j = 0; j < 5; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i * 5 + j]);
            }
        }

        printf("====================hist_result========================\n");
        for(int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[i]);

    }
#endif

    //expsoure params
    if (expParams.ptr()) {

        statsInt->aec_stats.ae_exp = expParams->data()->aecExpInfo;

        /*
         * printf("%s: L: [0x%x-0x%x], M: [0x%x-0x%x], S: [0x%x-0x%x]\n",
         *        __func__,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global);
         */
    }

    //iris params
    if (irisParams.ptr()) {

        float sof_time = (float)irisParams->data()->sofTime / 1000000000.0f;
        float start_time = (float)irisParams->data()->PIris.StartTim.tv_sec + (float)irisParams->data()->PIris.StartTim.tv_usec / 1000000.0f;
        float end_time = (float)irisParams->data()->PIris.EndTim.tv_sec + (float)irisParams->data()->PIris.EndTim.tv_usec / 1000000.0f;
        float frm_intval = 1 / (statsInt->aec_stats.ae_exp.pixel_clock_freq_mhz * 1000000.0f /
                                (float)statsInt->aec_stats.ae_exp.line_length_pixels / (float)statsInt->aec_stats.ae_exp.frame_length_lines);

        /*printf("%s: step=%d,last-step=%d,start-tim=%f,end-tim=%f,sof_tim=%f\n",
            __func__,
            statsInt->aec_stats.ae_exp.Iris.PIris.step,
            irisParams->data()->PIris.laststep,start_time,end_time,sof_time);
        */

        if(sof_time < end_time + frm_intval)
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.laststep;
        else
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.step;
    }

    to->set_sequence(left_stats->frame_id);

    return ret;

}

XCamReturn RkAiqResourceTranslatorV3x::translateMultiAwbStats(const SmartPtr<VideoBuffer>& from, SmartPtr<RkAiqAwbStatsProxy>& to)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();

    SmartPtr<RkAiqAwbStats> statsInt = to->data();

    struct rkisp3x_isp_stat_buffer *left_stats;
    struct rkisp3x_isp_stat_buffer *right_stats;

    left_stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(left_stats == NULL) {
        LOGE("fail to get left stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    right_stats = left_stats + 1;
    if(right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type)
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
    else
        LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", left_stats->frame_id, left_stats->meas_type);


    statsInt->awb_stats_valid = left_stats->meas_type >> 5 & 1;
    if (!statsInt->awb_stats_valid) {
        LOGE_ANALYZER("AWB stats invalid, ignore");
        return XCAM_RETURN_BYPASS;
    }

    rkisp_effect_params_v20 ispParams = {0};
    if (buf->getEffectiveIspParams(left_stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    rk_aiq_isp_blc_t *bls_cfg = &ispParams.blc_cfg.v0;
    statsInt->blc_cfg_effect = ispParams.blc_cfg.v0;
    //awb30
    statsInt->awb_stats_v3x.awb_cfg_effect_v201 = ispParams.awb_cfg_v3x;
    statsInt->awb_cfg_effect_valid = true;
    statsInt->frame_id = left_stats->frame_id;

    WinSplitMode AwbWinSplitMode = LEFT_AND_RIGHT_MODE;

    struct isp2x_window ori_win;
    ori_win.h_offs = ispParams.isp_params_v3x[0].meas.rawawb.sw_rawawb_h_offs;
    ori_win.h_size = ispParams.isp_params_v3x[0].meas.rawawb.sw_rawawb_h_size;
    ori_win.v_offs = ispParams.isp_params_v3x[0].meas.rawawb.sw_rawawb_v_offs;
    ori_win.v_size = ispParams.isp_params_v3x[0].meas.rawawb.sw_rawawb_v_size;

    JudgeWinLocation(&ori_win, AwbWinSplitMode, left_isp_rect_, right_isp_rect_);

    MergeAwbWinStats(statsInt->awb_stats_v3x.light, &left_stats->params.rawawb, &right_stats->params.rawawb,
                     statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum, AwbWinSplitMode);

    struct isp2x_window left_win;
    left_win.h_offs = ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_h_offs;
    left_win.h_size = ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_h_size;
    left_win.v_offs = ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_v_offs;
    left_win.v_size = ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_v_size;

    struct isp2x_window right_win;
    right_win.h_offs = ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_h_offs;
    right_win.h_size = ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_h_size;
    right_win.v_offs = ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_v_offs;
    right_win.v_size = ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_v_size;

    // to fixed the bug in ic design that some egisters maybe overflow
    AwbStatOverflowCheckandFixed(&left_win,
                                 (rk_aiq_awb_blk_stat_mode_v201_t)ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_blk_measure_mode,
                                 ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_blk_with_luma_wei_en,
                                 (rk_aiq_awb_xy_type_v201_t)ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_wp_hist_xytype,
                                 ispParams.isp_params_v3x[1].meas.rawawb.sw_rawawb_light_num, &left_stats->params.rawawb);
    AwbStatOverflowCheckandFixed(&right_win,
                                 (rk_aiq_awb_blk_stat_mode_v201_t)ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_blk_measure_mode,
                                 ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_blk_with_luma_wei_en,
                                 (rk_aiq_awb_xy_type_v201_t)ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_wp_hist_xytype,
                                 ispParams.isp_params_v3x[2].meas.rawawb.sw_rawawb_light_num, &right_stats->params.rawawb);


    MergeAwbBlkStats(&ori_win, &left_win, &right_win, statsInt->awb_stats_v3x.blockResult, &left_stats->params.rawawb, &right_stats->params.rawawb, AwbWinSplitMode);

    MergeAwbHistBinStats(statsInt->awb_stats_v3x.WpNoHist, left_stats->params.rawawb.ro_yhist_bin, right_stats->params.rawawb.ro_yhist_bin, AwbWinSplitMode);

#if defined(ISP_HW_V30)
    switch(AwbWinSplitMode) {
    case LEFT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++)
            statsInt->awb_stats_v3x.WpNo2[i] = left_stats->params.rawawb.ro_wp_num2[i];
        break;
    case RIGHT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++)
            statsInt->awb_stats_v3x.WpNo2[i] = right_stats->params.rawawb.ro_wp_num2[i];
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++)
            statsInt->awb_stats_v3x.WpNo2[i] = left_stats->params.rawawb.ro_wp_num2[i] + right_stats->params.rawawb.ro_wp_num2[i];
        break;
    default:
        break;
    }

    MergeAwbMultiWinStats(&statsInt->awb_stats_v3x, &left_stats->params.rawawb, &right_stats->params.rawawb);
    MergeAwbExcWpStats( statsInt->awb_stats_v3x.excWpRangeResult, &left_stats->params.rawawb, &right_stats->params.rawawb, AwbWinSplitMode);

#endif

    LOG1_AWB("bls_cfg %p", bls_cfg);
    if(bls_cfg) {
        LOG1_AWB("bls1_enalbe: %d, b r gb gr:[ %d %d %d %d]", bls_cfg->blc1_enable, bls_cfg->blc1_b,
                 bls_cfg->blc1_r, bls_cfg->blc1_gb, bls_cfg->blc1_gr);
    }
    if(bls_cfg && bls_cfg->blc1_enable && (bls_cfg->blc1_b > 0 || bls_cfg->blc1_r > 0 || bls_cfg->blc1_gb > 0 || bls_cfg->blc1_gr > 0)) {

        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
            statsInt->awb_stats_v3x.blockResult[i].Rvalue -=  (statsInt->awb_stats_v3x.blockResult[i].WpNo * bls_cfg->blc1_r + 8) >> 4 ;
            statsInt->awb_stats_v3x.blockResult[i].Gvalue -=  (statsInt->awb_stats_v3x.blockResult[i].WpNo * (bls_cfg->blc1_gr + bls_cfg->blc1_gb) + 16) >> 5 ;
            statsInt->awb_stats_v3x.blockResult[i].Bvalue -= (statsInt->awb_stats_v3x.blockResult[i].WpNo * bls_cfg->blc1_b + 8) >> 4 ; ;
        }
    }

    //statsInt->awb_stats_valid = ISP2X_STAT_RAWAWB(left_stats->meas_type)? true:false;
    statsInt->awb_stats_valid = left_stats->meas_type >> 5 & 1;
    to->set_sequence(left_stats->frame_id);
    return ret;

}

XCamReturn
RkAiqResourceTranslatorV3x::translateMultiAdehazeStats(const SmartPtr<VideoBuffer>& from, SmartPtr<RkAiqAdehazeStatsProxy>& to)
{

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

#if 0
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();

    SmartPtr<RkAiqAdehazeStats> statsInt = to->data();

    struct rkisp3x_isp_stat_buffer *left_stats;
    struct rkisp3x_isp_stat_buffer *right_stats;

    left_stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(left_stats == NULL) {
        LOGE("fail to get left stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    right_stats = left_stats + 1;
    if(right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type) {
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
        return XCAM_RETURN_ERROR_PARAM;
    }
    else
        LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", left_stats->frame_id, left_stats->meas_type);

    //adehaze stats v3.x
    statsInt->adehaze_stats_valid = left_stats->meas_type >> 17 & 1;
    statsInt->frame_id = left_stats->frame_id;

    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_air_base = (left_stats->params.dhaz.dhaz_adp_air_base + right_stats->params.dhaz.dhaz_adp_air_base) / 2;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_wt = (left_stats->params.dhaz.dhaz_adp_wt + right_stats->params.dhaz.dhaz_adp_wt) / 2;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_gratio = (left_stats->params.dhaz.dhaz_adp_gratio + right_stats->params.dhaz.dhaz_adp_gratio) / 2;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_tmax = (left_stats->params.dhaz.dhaz_adp_tmax + right_stats->params.dhaz.dhaz_adp_tmax) / 2;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_pic_sumh_left = left_stats->params.dhaz.dhaz_adp_tmax;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_pic_sumh_right = right_stats->params.dhaz.dhaz_adp_tmax;

    unsigned int ro_pic_sumh_left = left_stats->params.dhaz.dhaz_pic_sumh;
    unsigned int ro_pic_sumh_right = right_stats->params.dhaz.dhaz_pic_sumh;
    unsigned int tmp = 0;
    for (int i = 0; i < ISP3X_DHAZ_HIST_IIR_NUM; i++) {
        tmp = (left_stats->params.dhaz.h_rgb_iir[i] * ro_pic_sumh_left + right_stats->params.dhaz.h_rgb_iir[i] * ro_pic_sumh_right)
              / (ro_pic_sumh_left + ro_pic_sumh_right);
        statsInt->adehaze_stats.dehaze_stats_v30.h_rgb_iir[i] = tmp > ISP3X_DHAZ_HIST_IIR_MAX ? ISP3X_DHAZ_HIST_IIR_MAX : tmp;
    }
#endif

    return ret;
}

XCamReturn
RkAiqResourceTranslatorV3x::translateAecStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAecStatsProxy> &to)
{

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp3x_isp_stat_buffer *stats;
    SmartPtr<RkAiqAecStats> statsInt = to->data();

    if (mIsMultiIsp) {
        return translateMultiAecStats(from, to);
    }

    stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("camId: %d, stats: frame_id: %d,  meas_type; 0x%x",
                  mCamPhyId, stats->frame_id, stats->meas_type);

    SmartPtr<RkAiqIrisParamsProxy> irisParams = buf->get_iris_params();
    SmartPtr<RkAiqExpParamsProxy> expParams = nullptr;
    rkisp_effect_params_v20 ispParams = {0};
    if (buf->getEffectiveExpParams(stats->frame_id, expParams) < 0)
        LOGE("fail to get expParams");
    if (buf->getEffectiveIspParams(stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    //ae stats v3.x

    statsInt->frame_id = stats->frame_id;

    /*rawae stats*/
    struct isp3x_isp_meas_cfg* isp_params = &ispParams.isp_params_v3x[0].meas;
    uint8_t AeSwapMode, AeSelMode, AfUseAeBig;
    AeSwapMode = isp_params->rawae0.rawae_sel;
    AeSelMode = isp_params->rawae3.rawae_sel;
    AfUseAeBig = isp_params->rawaf.ae_mode;
    unsigned int meas_type = 0;

    struct isp21_bls_cfg *bls_cfg = &ispParams.isp_params_v3x[0].others.bls_cfg;
    struct isp2x_bls_fixed_val bls1_val; //bls1_val = blc1_ori_val * awb * range_ratio
    float bls_ratio[3] = {1, 1, 1};
    u32 pixel_num = 0;

    if(bls_cfg->bls1_en) {

        bls1_val.r = bls_cfg->bls1_val.r >> 2;
        bls1_val.gr = bls_cfg->bls1_val.gr;
        bls1_val.gb = bls_cfg->bls1_val.gb;
        bls1_val.b = bls_cfg->bls1_val.b >> 2;

        bls_ratio[0] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.r);
        bls_ratio[1] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.gr);
        bls_ratio[2] = (float)((1 << 12) - 1) / ((1 << 12) - 1 - bls_cfg->bls1_val.b);

    } else {

        bls1_val.r = 0;
        bls1_val.gr = 0;
        bls1_val.gb = 0;
        bls1_val.b = 0;

    }

#ifdef AE_STATS_DEBUG
    LOGE("bls1[%d-%d-%d-%d]", bls1_val.r, bls1_val.gr, bls1_val.gb, bls1_val.b);
    LOGE("bls_ratio[%f-%f-%f]", bls_ratio[0], bls_ratio[1], bls_ratio[2]);
#endif

    switch(AeSwapMode) {
    case AEC_RAWSWAP_MODE_S_LITE:
        meas_type = ((stats->meas_type >> 7) & (0x01)) & ((stats->meas_type >> 11) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
            if(i < ISP3X_RAWAELITE_MEAN_NUM) {
                statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i] = (u16)((stats->params.rawae0.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i] = (u16)((stats->params.rawae0.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i] = (u16)((stats->params.rawae0.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            }
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae1.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae1.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae1.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae2.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae2.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae2.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);

            if(i < ISP3X_RAWAEBIG_SUBWIN_NUM) {
                pixel_num = isp_params->rawae1.subwin[i].h_size * isp_params->rawae1.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae1.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae1.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae1.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);
                pixel_num = isp_params->rawae2.subwin[i].h_size * isp_params->rawae2.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae2.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae2.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae2.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);
            }
        }

        if(bls_cfg->bls1_en) {

            memset(statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[2].rawhist_lite.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
                int tmp;
                switch(isp_params->rawhist0.mode) {
                case 2:
                    tmp = (i - (bls1_val.r >> 2) > 0) ? (i - (bls1_val.r >> 2)) * bls_ratio[0] + 0.5 : 0;
                    break;
                case 3:
                    tmp = (i - (bls1_val.gr >> 4) > 0) ? (i - (bls1_val.gr >> 4)) * bls_ratio[1] + 0.5 : 0;
                    break;
                case 4:
                    tmp = (i - (bls1_val.b >> 2) > 0) ? (i - (bls1_val.b >> 2)) * bls_ratio[2] + 0.5 : 0;
                    break;
                case 5:
                default:
                    s16 hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
                    float hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
                    tmp = (i - hist_bls1) ? (i - hist_bls1) * hist_bls_ratio + 0.5 : 0;
                    break;
                }
                if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                    tmp = ISP3X_HIST_BIN_N_MAX - 1;
                statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[tmp] += stats->params.rawhist0.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins[tmp] += stats->params.rawhist1.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins[tmp] += stats->params.rawhist2.hist_bin[i];
            }
        } else {
            memcpy(statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins, stats->params.rawhist0.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, stats->params.rawhist1.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins, stats->params.rawhist2.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
        }

        break;

    case AEC_RAWSWAP_MODE_M_LITE:
        meas_type = ((stats->meas_type >> 8) & (0x01)) & ((stats->meas_type >> 12) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
            if(i < ISP3X_RAWAELITE_MEAN_NUM) {
                statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelr_xy[i] = (u16)((stats->params.rawae0.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelg_xy[i] = (u16)((stats->params.rawae0.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelb_xy[i] = (u16)((stats->params.rawae0.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            }
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae1.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae1.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae1.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae2.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae2.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[2].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae2.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);

            if(i < ISP3X_RAWAEBIG_SUBWIN_NUM) {
                pixel_num = isp_params->rawae1.subwin[i].h_size * isp_params->rawae1.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae1.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae1.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae1.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);

                pixel_num = isp_params->rawae2.subwin[i].h_size * isp_params->rawae2.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae2.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae2.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[2].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae2.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);
            }
        }

        if(bls_cfg->bls1_en) {

            memset(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[1].rawhist_lite.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));

            for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
                int tmp;
                switch(isp_params->rawhist1.mode) {
                case 2:
                    tmp = (i - (bls1_val.r >> 2) > 0) ? (i - (bls1_val.r >> 2)) * bls_ratio[0] + 0.5 : 0;
                    break;
                case 3:
                    tmp = (i - (bls1_val.gr >> 4) > 0) ? (i - (bls1_val.gr >> 4)) * bls_ratio[1] + 0.5 : 0;
                    break;
                case 4:
                    tmp = (i - (bls1_val.b >> 2) > 0) ? (i - (bls1_val.b >> 2)) * bls_ratio[2] + 0.5 : 0;
                    break;
                case 5:
                default:
                    s16 hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
                    float hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
                    tmp = (i - hist_bls1) ? (i - hist_bls1) * hist_bls_ratio + 0.5 : 0;
                    break;
                }
                if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                    tmp = ISP3X_HIST_BIN_N_MAX - 1;
                statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[tmp] += stats->params.rawhist1.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[1].rawhist_lite.bins[tmp] += stats->params.rawhist0.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins[tmp] += stats->params.rawhist2.hist_bin[i];
            }
        } else {
            memcpy(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, stats->params.rawhist1.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[1].rawhist_lite.bins, stats->params.rawhist0.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[2].rawhist_big.bins, stats->params.rawhist2.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
        }

        break;

    case AEC_RAWSWAP_MODE_L_LITE:
        meas_type = ((stats->meas_type >> 9) & (0x01)) & ((stats->meas_type >> 13) & (0x01));
        statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;

        for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
            if(i < ISP3X_RAWAELITE_MEAN_NUM) {
                statsInt->aec_stats.ae_data.chn[2].rawae_lite.channelr_xy[i] = (u16)((stats->params.rawae0.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[2].rawae_lite.channelg_xy[i] = (u16)((stats->params.rawae0.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[2].rawae_lite.channelb_xy[i] = (u16)((stats->params.rawae0.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            }
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae2.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae2.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae2.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae1.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae1.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
            statsInt->aec_stats.ae_data.chn[1].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae1.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);

            if(i < ISP3X_RAWAEBIG_SUBWIN_NUM) {
                pixel_num = isp_params->rawae2.subwin[i].h_size * isp_params->rawae2.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae2.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae2.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae2.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);

                pixel_num = isp_params->rawae1.subwin[i].h_size * isp_params->rawae1.subwin[i].v_size;
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae1.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae1.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae1.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);
            }
        }

        if(bls_cfg->bls1_en) {

            memset(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memset(statsInt->aec_stats.ae_data.chn[2].rawhist_lite.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));

            for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
                int tmp;
                switch(isp_params->rawhist2.mode) {
                case 2:
                    tmp = (i - (bls1_val.r >> 2) > 0) ? (i - (bls1_val.r >> 2)) * bls_ratio[0] + 0.5 : 0;
                    break;
                case 3:
                    tmp = (i - (bls1_val.gr >> 4) > 0) ? (i - (bls1_val.gr >> 4)) * bls_ratio[1] + 0.5 : 0;
                    break;
                case 4:
                    tmp = (i - (bls1_val.b >> 2) > 0) ? (i - (bls1_val.b >> 2)) * bls_ratio[2] + 0.5 : 0;
                    break;
                case 5:
                default:
                    s16 hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
                    float hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
                    tmp = (i - hist_bls1) ? (i - hist_bls1) * hist_bls_ratio + 0.5 : 0;
                    break;
                }
                if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                    tmp = ISP3X_HIST_BIN_N_MAX - 1;
                statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[tmp] += stats->params.rawhist2.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins[tmp] += stats->params.rawhist1.hist_bin[i];
                statsInt->aec_stats.ae_data.chn[2].rawhist_lite.bins[tmp] += stats->params.rawhist0.hist_bin[i];
            }
        } else {
            memcpy(statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins, stats->params.rawhist2.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins, stats->params.rawhist1.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            memcpy(statsInt->aec_stats.ae_data.chn[2].rawhist_lite.bins, stats->params.rawhist0.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
        }
        break;

    default:
        LOGE("wrong AeSwapMode=%d\n", AeSwapMode);
        return XCAM_RETURN_ERROR_PARAM;
        break;
    }

    statsInt->af_prior = (AfUseAeBig == 0) ? false : true;

    if(!AfUseAeBig) {
        switch(AeSelMode) {
        case AEC_RAWSEL_MODE_CHN_0:
        case AEC_RAWSEL_MODE_CHN_1:
        case AEC_RAWSEL_MODE_CHN_2:
            for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {

                statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.channelr_xy[i] = (u16)((stats->params.rawae3.data[i].channelr_xy - bls1_val.r) * bls_ratio[0]);
                statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.channelg_xy[i] = (u16)((stats->params.rawae3.data[i].channelg_xy - bls1_val.gr) * bls_ratio[1]);
                statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.channelb_xy[i] = (u16)((stats->params.rawae3.data[i].channelb_xy - bls1_val.b) * bls_ratio[2]);

                if(i < ISP3X_RAWAEBIG_SUBWIN_NUM) {
                    pixel_num = isp_params->rawae3.subwin[i].h_size * isp_params->rawae3.subwin[i].v_size;
                    statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.wndx_sumr[i] = (u32)((stats->params.rawae3.sumr[i] - (pixel_num >> 2) * bls1_val.r) * bls_ratio[0]);
                    statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.wndx_sumg[i] = (u32)((stats->params.rawae3.sumg[i] - (pixel_num >> 1) * bls1_val.gr) * bls_ratio[1]);
                    statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big.wndx_sumb[i] = (u32)((stats->params.rawae3.sumb[i] - (pixel_num >> 2) * bls1_val.b) * bls_ratio[2]);
                }
            }

            if(bls_cfg->bls1_en) {
                memset(statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins, 0, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
                for(int i = 0; i < ISP3X_HIST_BIN_N_MAX; i++) {
                    int tmp;
                    switch(isp_params->rawhist3.mode) {
                    case 2:
                        tmp = (i - (bls1_val.r >> 2) > 0) ? (i - (bls1_val.r >> 2)) * bls_ratio[0] + 0.5 : 0;
                        break;
                    case 3:
                        tmp = (i - (bls1_val.gr >> 4) > 0) ? (i - (bls1_val.gr >> 4)) * bls_ratio[1] + 0.5 : 0;
                        break;
                    case 4:
                        tmp = (i - (bls1_val.b >> 2) > 0) ? (i - (bls1_val.b >> 2)) * bls_ratio[2] + 0.5 : 0;
                        break;
                    case 5:
                    default:
                        s16 hist_bls1 = (s16)((bls1_val.gr >> 4) * 0.587 + (bls1_val.r >> 2) * 0.299 + (bls1_val.b >> 2) * 0.144 + 0.5);
                        float hist_bls_ratio = (float)((1 << 8) - 1) / ((1 << 8) - 1 - hist_bls1);
                        tmp = (i - hist_bls1) ? (i - hist_bls1) * hist_bls_ratio + 0.5 : 0;
                        break;
                    }
                    if(tmp > ISP3X_HIST_BIN_N_MAX - 1)
                        tmp = ISP3X_HIST_BIN_N_MAX - 1;
                    statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins[tmp] += stats->params.rawhist3.hist_bin[i];
                }
            } else {
                memcpy(statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins, stats->params.rawhist3.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            }
            break;
        case AEC_RAWSEL_MODE_TMO:
            for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {

                statsInt->aec_stats.ae_data.extra.rawae_big.channelr_xy[i] = stats->params.rawae3.data[i].channelr_xy;
                statsInt->aec_stats.ae_data.extra.rawae_big.channelg_xy[i] = stats->params.rawae3.data[i].channelg_xy;
                statsInt->aec_stats.ae_data.extra.rawae_big.channelb_xy[i] = stats->params.rawae3.data[i].channelb_xy;

                if(i < ISP3X_RAWAEBIG_SUBWIN_NUM) {
                    statsInt->aec_stats.ae_data.extra.rawae_big.wndx_sumr[i] = stats->params.rawae3.sumr[i];
                    statsInt->aec_stats.ae_data.extra.rawae_big.wndx_sumg[i] = stats->params.rawae3.sumg[i];
                    statsInt->aec_stats.ae_data.extra.rawae_big.wndx_sumb[i] = stats->params.rawae3.sumb[i];
                }
            }
            memcpy(statsInt->aec_stats.ae_data.extra.rawhist_big.bins, stats->params.rawhist3.hist_bin, ISP3X_HIST_BIN_N_MAX * sizeof(u32));
            break;

        default:
            LOGE("wrong AeSelMode=%d\n", AeSelMode);
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

#ifdef AE_STATS_DEBUG
    if(AeSwapMode != 0) {
        for(int i = 0; i < 15; i++) {
            for(int j = 0; j < 15; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i * 15 + j]);
            }
        }
        printf("====================sub-win-result======================\n");

        for(int i = 0; i < 4; i++)
            printf("chn0_subwin[%d]:sumr 0x%08lx, sumg 0x%08lx, sumb 0x%08lx\n", i,
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i]);

        printf("====================hist_result========================\n");

        for(int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[i]);

    } else {
        for(int i = 0; i < 5; i++) {
            for(int j = 0; j < 5; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i * 5 + j]);
            }
        }
        printf("====================hist_result========================\n");
        for(int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[i]);
    }

#endif

    //rotate stats for group ae
    if(mIsGroupMode) {
        RkAiqAecHwStatsRes_t* temp = (RkAiqAecHwStatsRes_t*)calloc(1, sizeof(RkAiqAecHwStatsRes_t));
        if(mModuleRotation == 1/*RK_PS_SrcOverlapPosition_90*/) {
            //1) clockwise 90
            for(int i = 0; i < 3; i++) {
                //BIG
                uint8_t colnum = sqrt(ISP3X_RAWAEBIG_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->chn[i].rawae_big.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelr_xy[(colnum - 1 - col) * colnum + row];
                        temp->chn[i].rawae_big.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelg_xy[(colnum - 1 - col) * colnum + row];
                        temp->chn[i].rawae_big.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelb_xy[(colnum - 1 - col) * colnum + row];
                    }
                }
                //LITE
                colnum = sqrt(ISP3X_RAWAELITE_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->chn[i].rawae_lite.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelr_xy[(colnum - 1 - col) * colnum + row];
                        temp->chn[i].rawae_lite.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelg_xy[(colnum - 1 - col) * colnum + row];
                        temp->chn[i].rawae_lite.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelb_xy[(colnum - 1 - col) * colnum + row];
                    }
                }
                memcpy(&statsInt->aec_stats.ae_data.chn[i].rawae_lite, &temp->chn[i].rawae_lite, sizeof(rawaelite_stat_t));
                memcpy(&statsInt->aec_stats.ae_data.chn[i].rawae_big, &temp->chn[i].rawae_big, sizeof(rawaebig_stat_t));
            }

            if(!AfUseAeBig) {
                uint8_t colnum = sqrt(ISP3X_RAWAEBIG_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->extra.rawae_big.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelr_xy[(colnum - 1 - col) * colnum + row];
                        temp->extra.rawae_big.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelg_xy[(colnum - 1 - col) * colnum + row];
                        temp->extra.rawae_big.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelb_xy[(colnum - 1 - col) * colnum + row];
                    }
                }
                memcpy(&statsInt->aec_stats.ae_data.extra.rawae_big, &temp->extra.rawae_big, sizeof(rawaebig_stat_t));
            }
        } else if(mModuleRotation == 3/*RK_PS_SrcOverlapPosition_270*/) {
            //2) counter clockwise 90
            for(int i = 0; i < 3; i++) {
                //BIG
                uint8_t colnum = sqrt(ISP3X_RAWAEBIG_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->chn[i].rawae_big.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelr_xy[col * colnum + (colnum - 1 - row)];
                        temp->chn[i].rawae_big.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelg_xy[col * colnum + (colnum - 1 - row)];
                        temp->chn[i].rawae_big.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_big.channelb_xy[col * colnum + (colnum - 1 - row)];
                    }
                }
                //LITE
                colnum = sqrt(ISP3X_RAWAELITE_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->chn[i].rawae_lite.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelr_xy[col * colnum + (colnum - 1 - row)];
                        temp->chn[i].rawae_lite.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelg_xy[col * colnum + (colnum - 1 - row)];
                        temp->chn[i].rawae_lite.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.chn[i].rawae_lite.channelb_xy[col * colnum + (colnum - 1 - row)];
                    }
                }

                memcpy(&statsInt->aec_stats.ae_data.chn[i].rawae_lite, &temp->chn[i].rawae_lite, sizeof(rawaelite_stat_t));
                memcpy(&statsInt->aec_stats.ae_data.chn[i].rawae_big, &temp->chn[i].rawae_big, sizeof(rawaebig_stat_t));
            }

            if(!AfUseAeBig) {
                uint8_t colnum = sqrt(ISP3X_RAWAEBIG_MEAN_NUM);
                for(int row = 0; row < colnum; row++) {
                    for(int col = 0; col < colnum; col++) {
                        temp->extra.rawae_big.channelr_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelr_xy[col * colnum + (colnum - 1 - row)];
                        temp->extra.rawae_big.channelg_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelg_xy[col * colnum + (colnum - 1 - row)];
                        temp->extra.rawae_big.channelb_xy[row * colnum + col] = \
                                statsInt->aec_stats.ae_data.extra.rawae_big.channelb_xy[col * colnum + (colnum - 1 - row)];
                    }
                }
                memcpy(&statsInt->aec_stats.ae_data.extra.rawae_big, &temp->extra.rawae_big, sizeof(rawaebig_stat_t));
            }
        } else {
            LOGW("not support mModuleRotation %d", mModuleRotation);
        }

        if(temp) free(temp);
    }


    /*
     *         unsigned long chn0_mean = 0, chn1_mean = 0;
     *         for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
     *             chn0_mean += stats->params.rawae1.data[i].channelg_xy;
     *             chn1_mean += stats->params.rawae3.data[i].channelg_xy;
     *         }
     *
     *
     *         printf("frame[%d]: chn[0-1]_g_mean_xy: %ld-%ld\n",
     *                 stats->frame_id, chn0_mean/ISP3X_RAWAEBIG_MEAN_NUM,
     *                 chn1_mean/ISP3X_RAWAEBIG_MEAN_NUM);
     */

    //expsoure params
    if (expParams.ptr()) {

        statsInt->aec_stats.ae_exp = expParams->data()->aecExpInfo;
        /*printf("frame[%d],gain=%d,time=%d\n", stats->frame_id,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time);*/


        /*
         * printf("%s: L: [0x%x-0x%x], M: [0x%x-0x%x], S: [0x%x-0x%x]\n",
         *        __func__,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global);
         */
    }

    //iris params
    if (irisParams.ptr()) {

        float sof_time = (float)irisParams->data()->sofTime / 1000000000.0f;
        float start_time = (float)irisParams->data()->PIris.StartTim.tv_sec + (float)irisParams->data()->PIris.StartTim.tv_usec / 1000000.0f;
        float end_time = (float)irisParams->data()->PIris.EndTim.tv_sec + (float)irisParams->data()->PIris.EndTim.tv_usec / 1000000.0f;
        float frm_intval = 1 / (statsInt->aec_stats.ae_exp.pixel_clock_freq_mhz * 1000000.0f /
                                (float)statsInt->aec_stats.ae_exp.line_length_pixels / (float)statsInt->aec_stats.ae_exp.frame_length_lines);

        /*printf("%s: step=%d,last-step=%d,start-tim=%f,end-tim=%f,sof_tim=%f\n",
            __func__,
            statsInt->aec_stats.ae_exp.Iris.PIris.step,
            irisParams->data()->PIris.laststep,start_time,end_time,sof_time);
        */

        if(sof_time < end_time + frm_intval)
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.laststep;
        else
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.step;
    }

    to->set_sequence(stats->frame_id);

    return ret;
}


void RotationDegAwbBlkStas(rk_aiq_awb_stat_blk_res_v201_t *blockResult, int degree)
{

    rk_aiq_awb_stat_blk_res_v201_t blockResult_old[RK_AIQ_AWB_GRID_NUM_TOTAL];
    if(degree == 3 /*RK_PS_SrcOverlapPosition_270*/) {
        memcpy(blockResult_old, blockResult, sizeof(rk_aiq_awb_stat_blk_res_v201_t)*RK_AIQ_AWB_GRID_NUM_TOTAL);
        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_VERHOR; i++) {
            for(int j = 0; j < RK_AIQ_AWB_GRID_NUM_VERHOR; j++) {
                memcpy(&blockResult[(RK_AIQ_AWB_GRID_NUM_VERHOR - j - 1)*RK_AIQ_AWB_GRID_NUM_VERHOR + i], &blockResult_old[i * RK_AIQ_AWB_GRID_NUM_VERHOR + j], sizeof(rk_aiq_awb_stat_blk_res_v201_t));
            }
        }
    } else if(degree == 1 /*RK_PS_SrcOverlapPosition_90*/) {
        memcpy(blockResult_old, blockResult, sizeof(rk_aiq_awb_stat_blk_res_v201_t)*RK_AIQ_AWB_GRID_NUM_TOTAL);
        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_VERHOR; i++) {
            for(int j = 0; j < RK_AIQ_AWB_GRID_NUM_VERHOR; j++) {
                memcpy(&blockResult[j * RK_AIQ_AWB_GRID_NUM_VERHOR + (RK_AIQ_AWB_GRID_NUM_VERHOR - i - 1)], &blockResult_old[i * RK_AIQ_AWB_GRID_NUM_VERHOR + j], sizeof(rk_aiq_awb_stat_blk_res_v201_t));
            }
        }
    } else {
        LOGW_AWBGROUP("not support mModuleRotation %d, abandon to rotate awb blk stas !!!!", degree);
    }
}


XCamReturn
RkAiqResourceTranslatorV3x::translateAwbStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAwbStatsProxy> &to)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp3x_isp_stat_buffer *stats;
    SmartPtr<RkAiqAwbStats> statsInt = to->data();

    if (mIsMultiIsp) {
        return translateMultiAwbStats(from, to);
    }

    stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("awb stats: camId:%d, frame_id: %d,  meas_type; 0x%x",
                  mCamPhyId, stats->frame_id, stats->meas_type);

    statsInt->awb_stats_valid = stats->meas_type >> 5 & 1;
    if (!statsInt->awb_stats_valid) {
        LOGE_ANALYZER("AWB stats invalid, ignore");
        return XCAM_RETURN_BYPASS;
    }

    rkisp_effect_params_v20 ispParams = {0};
    if (buf->getEffectiveIspParams(stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    //awb30
    statsInt->awb_stats_v3x.awb_cfg_effect_v201 = ispParams.awb_cfg_v3x;
    rk_aiq_isp_blc_t *bls_cfg = &ispParams.blc_cfg.v0;
    statsInt->blc_cfg_effect = ispParams.blc_cfg.v0;
    statsInt->awb_cfg_effect_valid = true;
    statsInt->frame_id = stats->frame_id;
    for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++) {
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
            stats->params.rawawb.ro_rawawb_sum_rgain_nor[i];
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
            stats->params.rawawb.ro_rawawb_sum_bgain_nor[i];
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
            stats->params.rawawb.ro_rawawb_wp_num_nor[i];
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
            stats->params.rawawb.ro_rawawb_sum_rgain_big[i];
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
            stats->params.rawawb.ro_rawawb_sum_bgain_big[i];
        statsInt->awb_stats_v3x.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
            stats->params.rawawb.ro_rawawb_wp_num_big[i];

    }

    for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
        statsInt->awb_stats_v3x.blockResult[i].Rvalue = stats->params.rawawb.ramdata[i].r;
        statsInt->awb_stats_v3x.blockResult[i].Gvalue = stats->params.rawawb.ramdata[i].g;
        statsInt->awb_stats_v3x.blockResult[i].Bvalue = stats->params.rawawb.ramdata[i].b;
        statsInt->awb_stats_v3x.blockResult[i].WpNo = stats->params.rawawb.ramdata[i].wp;
    }

    for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
        statsInt->awb_stats_v3x.WpNoHist[i] = stats->params.rawawb.ro_yhist_bin[i];
        // move the shift code here to make WpNoHist merged by several cameras easily
        if( stats->params.rawawb.ro_yhist_bin[i]  & 0x8000 ) {
            statsInt->awb_stats_v3x.WpNoHist[i] = stats->params.rawawb.ro_yhist_bin[i] & 0x7FFF;
            statsInt->awb_stats_v3x.WpNoHist[i] *=    (1 << 3);
        }
    }

#if defined(ISP_HW_V30)
    for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++) {
        statsInt->awb_stats_v3x.WpNo2[i] =  stats->params.rawawb.ro_wp_num2[i];
    }
    for(int i = 0; i < RK_AIQ_AWB_MULTIWINDOW_NUM_V201; i++) {
        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
            stats->params.rawawb.ro_sum_r_nor_multiwindow[i];

        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
            stats->params.rawawb.ro_sum_b_nor_multiwindow[i];
        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
            stats->params.rawawb.ro_wp_nm_nor_multiwindow[i];
        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
            stats->params.rawawb.ro_sum_r_big_multiwindow[i];
        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
            stats->params.rawawb.ro_sum_b_big_multiwindow[i];
        statsInt->awb_stats_v3x.multiwindowLightResult[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
            stats->params.rawawb.ro_wp_nm_big_multiwindow[i];
    }

    for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
        statsInt->awb_stats_v3x.excWpRangeResult[i].RgainValue = stats->params.rawawb.ro_sum_r_exc[i];
        statsInt->awb_stats_v3x.excWpRangeResult[i].BgainValue = stats->params.rawawb.ro_sum_b_exc[i];
        statsInt->awb_stats_v3x.excWpRangeResult[i].WpNo =    stats->params.rawawb.ro_wp_nm_exc[i];

    }

    // to fixed the bug in ic design that some egisters maybe overflow
    if(!mIsMultiIsp) {
        int w, h;
        w = statsInt->awb_stats_v3x.awb_cfg_effect_v201.windowSet[2];
        h = statsInt->awb_stats_v3x.awb_cfg_effect_v201.windowSet[3];
        float factor1 = (float)((1 << (RK_AIQ_AWB_WP_WEIGHT_BIS_V201 + 1)) - 1) / ((1 << RK_AIQ_AWB_WP_WEIGHT_BIS_V201) - 1);
        //float factor1 = (float)(1<<(RK_AIQ_AWB_WP_WEIGHT_BIS_V201+1))/((1<<RK_AIQ_AWB_WP_WEIGHT_BIS_V201)-1);
        if(w * h > RK_AIQ_AWB_STAT_MAX_AREA) {
            LOGD_AWB("%s ramdata and ro_wp_num2 is fixed", __FUNCTION__);
            for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
                statsInt->awb_stats_v3x.blockResult[i].WpNo = (float)statsInt->awb_stats_v3x.blockResult[i].WpNo * factor1 + 0.5 ;
                statsInt->awb_stats_v3x.blockResult[i].Rvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Rvalue * factor1 + 0.5 ;
                statsInt->awb_stats_v3x.blockResult[i].Gvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Gvalue * factor1 + 0.5 ;
                statsInt->awb_stats_v3x.blockResult[i].Bvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Bvalue * factor1 + 0.5 ;
            }
            rk_aiq_awb_xy_type_v201_t typ = statsInt->awb_stats_v3x.awb_cfg_effect_v201.xyRangeTypeForWpHist;
            for(int i = 0; i < statsInt->awb_stats_v3x.awb_cfg_effect_v201.lightNum; i++) {
                statsInt->awb_stats_v3x.WpNo2[i] = statsInt->awb_stats_v3x.light[i].xYType[typ].WpNo >> RK_AIQ_WP_GAIN_FRAC_BIS;
            }
        } else {
            if(statsInt->awb_stats_v3x.awb_cfg_effect_v201.blkMeasureMode == RK_AIQ_AWB_BLK_STAT_MODE_REALWP_V201
                    && statsInt->awb_stats_v3x.awb_cfg_effect_v201.blkStatisticsWithLumaWeightEn) {
                for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
                    statsInt->awb_stats_v3x.blockResult[i].WpNo = (float)statsInt->awb_stats_v3x.blockResult[i].WpNo * factor1 + 0.5 ;
                    statsInt->awb_stats_v3x.blockResult[i].Rvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Rvalue * factor1 + 0.5 ;
                    statsInt->awb_stats_v3x.blockResult[i].Gvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Gvalue * factor1 + 0.5 ;
                    statsInt->awb_stats_v3x.blockResult[i].Bvalue = (float)statsInt->awb_stats_v3x.blockResult[i].Bvalue * factor1 + 0.5 ;
                }
            }
        }
    }

#endif
    LOG1_AWB("bls_cfg %p", bls_cfg);
    if(bls_cfg) {
        LOG1_AWB("bls1_enalbe: %d, b r gb gr:[ %d %d %d %d]", bls_cfg->blc1_enable, bls_cfg->blc1_b,
                 bls_cfg->blc1_r, bls_cfg->blc1_gb, bls_cfg->blc1_gr);
    }
    if(bls_cfg && bls_cfg->blc1_enable && (bls_cfg->blc1_b > 0 || bls_cfg->blc1_r > 0 || bls_cfg->blc1_gb > 0 || bls_cfg->blc1_gr > 0)) {

        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
            statsInt->awb_stats_v3x.blockResult[i].Rvalue -=  (statsInt->awb_stats_v3x.blockResult[i].WpNo * bls_cfg->blc1_r + 8) >> 4 ;
            statsInt->awb_stats_v3x.blockResult[i].Gvalue -=  (statsInt->awb_stats_v3x.blockResult[i].WpNo * (bls_cfg->blc1_gr + bls_cfg->blc1_gb) + 16) >> 5 ;
            statsInt->awb_stats_v3x.blockResult[i].Bvalue -= (statsInt->awb_stats_v3x.blockResult[i].WpNo * bls_cfg->blc1_b + 8) >> 4 ; ;
        }
    }
    LOGV_AWBGROUP("mIsGroupMode %d, mCamPhyId %d,mModuleRotation %d", mIsGroupMode, mCamPhyId, mModuleRotation);
    if(mIsGroupMode) {
        RotationDegAwbBlkStas(statsInt->awb_stats_v3x.blockResult, mModuleRotation);
    }
    //statsInt->awb_stats_valid = ISP2X_STAT_RAWAWB(stats->meas_type)? true:false;
    statsInt->awb_stats_valid = stats->meas_type >> 5 & 1;
    to->set_sequence(stats->frame_id);

    return ret;
}

XCamReturn
RkAiqResourceTranslatorV3x::translateMultiAfStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAfStatsProxy> &to)
{
    typedef enum WinSplitMode_s {
        LEFT_AND_RIGHT_MODE = 0,
        LEFT_MODE,
        RIGHT_MODE,
        FULL_MODE,
    } SplitMode;

    struct AfSplitInfo {
        SplitMode wina_side_info;
        int32_t wina_l_blknum;
        int32_t wina_r_blknum;
        int32_t wina_r_skip_blknum;
        float wina_l_ratio;
        float wina_r_ratio;

        SplitMode winb_side_info;
        float winb_l_ratio;
        float winb_r_ratio;
    };

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp3x_isp_stat_buffer *left_stats, *right_stats;
    SmartPtr<RkAiqAfStats> statsInt = to->data();
    SmartPtr<RkAiqAfInfoProxy> afParams = buf->get_af_params();

    left_stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (left_stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    right_stats = left_stats + 1;
    if (right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if (left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type)
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
    else
        LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", left_stats->frame_id, left_stats->meas_type);

    rkisp_effect_params_v20 ispParams = {0};
    if (buf->getEffectiveIspParams(left_stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    statsInt->frame_id = left_stats->frame_id;

    struct isp3x_rawaf_meas_cfg &org_af = ispParams.isp_params_v3x[0].meas.rawaf;
    int32_t l_isp_st, l_isp_ed, r_isp_st, r_isp_ed;
    int32_t l_win_st, l_win_ed, r_win_st, r_win_ed;
    int32_t x_st, x_ed, l_blknum, r_blknum, ov_w, blk_w, r_skip_blknum;
    struct AfSplitInfo af_split_info;
    int32_t i, j, k, dst_idx, l_idx, r_idx, l_lht, r_lht, lht0, lht1;

    memset(&af_split_info, 0, sizeof(af_split_info));
    ov_w = left_isp_rect_.w + left_isp_rect_.x - right_isp_rect_.x;
    x_st = org_af.win[0].h_offs;
    x_ed = x_st + org_af.win[0].h_size;
    l_isp_st = left_isp_rect_.x;
    l_isp_ed = left_isp_rect_.x + left_isp_rect_.w;
    r_isp_st = right_isp_rect_.x;
    r_isp_ed = right_isp_rect_.x + right_isp_rect_.w;
    LOGD_AF("wina.x_st %d, wina.x_ed %d, l_isp_st %d, l_isp_ed %d, r_isp_st %d, r_isp_ed %d",
            x_st, x_ed, l_isp_st, l_isp_ed, r_isp_st, r_isp_ed);

    //// winA ////
    af_split_info.wina_l_ratio = 0;
    af_split_info.wina_r_ratio = 0;
    // af win in both side
    if ((x_st < r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.wina_side_info = LEFT_AND_RIGHT_MODE;
        // af win < one isp width
        if (org_af.win[0].h_size < left_isp_rect_.w) {
            blk_w = org_af.win[0].h_size / ISP2X_RAWAF_SUMDATA_ROW;
            l_blknum = (l_isp_ed - x_st + blk_w - 1) / blk_w;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW - l_blknum;
            l_win_ed = l_isp_ed - 2;
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            if (blk_w < ov_w) {
                r_skip_blknum = ov_w / blk_w;
                r_win_st = ov_w - r_skip_blknum * blk_w;
                r_win_ed = ov_w + (ISP2X_RAWAF_SUMDATA_ROW - r_skip_blknum) * blk_w;
                af_split_info.wina_r_skip_blknum = r_skip_blknum;
            }
            else {
                r_skip_blknum = 0;
                r_win_st = 2;
                r_win_ed = r_win_st + ISP2X_RAWAF_SUMDATA_ROW * blk_w;

                // blend last block of left isp and first block of right isp
                af_split_info.wina_r_skip_blknum = 0;
                af_split_info.wina_l_ratio = (float)ov_w / (float)blk_w;
                af_split_info.wina_r_ratio = 1 - af_split_info.wina_l_ratio;
            }
        }
        // af win <= one isp width * 1.5
        else if (org_af.win[0].h_size < left_isp_rect_.w * 3 / 2) {
            l_win_st = x_st;
            l_win_ed = l_isp_ed - 2;
            blk_w = (l_win_ed - l_win_st) / (ISP2X_RAWAF_SUMDATA_ROW + 1);
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            l_blknum = ((l_win_ed - l_win_st) * ISP2X_RAWAF_SUMDATA_ROW + org_af.win[0].h_size - 1) / org_af.win[0].h_size;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW - l_blknum;
            if (blk_w < ov_w) {
                r_skip_blknum = ov_w / blk_w;
                r_win_st = ov_w - r_skip_blknum * blk_w;
                r_win_ed = ov_w + (ISP2X_RAWAF_SUMDATA_ROW - r_skip_blknum) * blk_w;
                af_split_info.wina_r_skip_blknum = r_skip_blknum;
            }
            else {
                r_skip_blknum = 0;
                r_win_st = 2;
                r_win_ed = r_win_st + ISP2X_RAWAF_SUMDATA_ROW * blk_w;
                // blend last block of left isp and first block of right isp
                af_split_info.wina_r_skip_blknum = 0;
                af_split_info.wina_l_ratio = (float)ov_w / (float)blk_w;
                af_split_info.wina_r_ratio = 1 - af_split_info.wina_l_ratio;
            }
        }
        else {
            l_win_st = x_st;
            l_win_ed = l_isp_ed - 2;
            blk_w = (l_win_ed - l_win_st) / ISP2X_RAWAF_SUMDATA_ROW;
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            r_win_st = 2;
            r_win_ed = r_win_st + blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            af_split_info.wina_side_info = FULL_MODE;
            l_blknum = ISP2X_RAWAF_SUMDATA_ROW;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        }
    }
    // af win in right side
    else if ((x_st >= r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.wina_side_info = RIGHT_MODE;
        l_blknum = 0;
        r_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        r_win_st = x_st - right_isp_rect_.x;
        r_win_ed = x_ed - right_isp_rect_.x;
        l_win_st = r_win_st;
        l_win_ed = r_win_ed;
    }
    // af win in left side
    else {
        af_split_info.wina_side_info = LEFT_MODE;
        l_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        r_blknum = 0;
        l_win_st = x_st;
        l_win_ed = x_ed;
        r_win_st = l_win_st;
        r_win_ed = l_win_ed;
    }

    af_split_info.wina_l_blknum = l_blknum;
    af_split_info.wina_r_blknum = r_blknum;

    //// winB ////
    af_split_info.winb_l_ratio = 0;
    af_split_info.winb_r_ratio = 0;
    x_st = org_af.win[1].h_offs;
    x_ed = x_st + org_af.win[1].h_size;
    LOGD_AF("winb.x_st %d, winb.x_ed %d, l_isp_st %d, l_isp_ed %d, r_isp_st %d, r_isp_ed %d",
            x_st, x_ed, l_isp_st, l_isp_ed, r_isp_st, r_isp_ed);

    // af win in both side
    if ((x_st < r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.winb_side_info = LEFT_AND_RIGHT_MODE;
        l_win_st = x_st;
        l_win_ed = l_isp_ed - 2;
        r_win_st = ov_w - 2;
        r_win_ed = x_ed - right_isp_rect_.x;
        // blend winB by width of left isp winB and right isp winB
        af_split_info.winb_l_ratio = (float)(l_win_ed - l_win_st) / (float)(x_ed - x_st);
        af_split_info.winb_r_ratio = 1 - af_split_info.winb_l_ratio;
    }
    // af win in right side
    else if ((x_st >= r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.winb_side_info = RIGHT_MODE;
        af_split_info.winb_l_ratio = 0;
        af_split_info.winb_r_ratio = 1;
        r_win_st = x_st - right_isp_rect_.x;
        r_win_ed = x_ed - right_isp_rect_.x;
        l_win_st = r_win_st;
        l_win_ed = r_win_ed;
    }
    // af win in left side
    else {
        af_split_info.winb_side_info = LEFT_MODE;
        af_split_info.winb_l_ratio = 1;
        af_split_info.winb_r_ratio = 0;
        l_win_st = x_st;
        l_win_ed = x_ed;
        r_win_st = l_win_st;
        r_win_ed = l_win_ed;
    }

    //af
    {
        if (((left_stats->meas_type >> 6) & (0x01)) && ((left_stats->meas_type >> 6) & (0x01)))
            statsInt->af_stats_valid = true;
        else
            statsInt->af_stats_valid = false;

        statsInt->af_stats_v3x.int_state = left_stats->params.rawaf.int_state | right_stats->params.rawaf.int_state;
        if (af_split_info.winb_side_info == LEFT_AND_RIGHT_MODE) {
            statsInt->af_stats_v3x.wndb_luma = left_stats->params.rawaf.afm_lum_b * af_split_info.winb_l_ratio +
                                               right_stats->params.rawaf.afm_lum_b * af_split_info.winb_r_ratio;
            statsInt->af_stats_v3x.wndb_sharpness = left_stats->params.rawaf.afm_sum_b * af_split_info.winb_l_ratio +
                                                    right_stats->params.rawaf.afm_sum_b * af_split_info.winb_r_ratio;
            statsInt->af_stats_v3x.winb_highlit_cnt = left_stats->params.rawaf.highlit_cnt_winb * af_split_info.winb_l_ratio +
                    right_stats->params.rawaf.highlit_cnt_winb * af_split_info.winb_r_ratio;
        } else if (af_split_info.winb_side_info == LEFT_MODE) {
            statsInt->af_stats_v3x.wndb_luma = left_stats->params.rawaf.afm_lum_b;
            statsInt->af_stats_v3x.wndb_sharpness = left_stats->params.rawaf.afm_sum_b;
            statsInt->af_stats_v3x.winb_highlit_cnt = left_stats->params.rawaf.highlit_cnt_winb;
        } else {
            statsInt->af_stats_v3x.wndb_luma = right_stats->params.rawaf.afm_lum_b;
            statsInt->af_stats_v3x.wndb_sharpness = right_stats->params.rawaf.afm_sum_b;
            statsInt->af_stats_v3x.winb_highlit_cnt = right_stats->params.rawaf.highlit_cnt_winb;
        }

        if (af_split_info.wina_side_info == FULL_MODE) {
            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                for (j = 0; j < ISP2X_RAWAF_SUMDATA_COLUMN; j++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    if (j == 0) {
                        l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h2;
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] = left_stats->params.rawae3.data[l_idx].channelg_xy;
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] =
                            ((left_stats->params.rawae3.data[l_idx].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[l_idx].channelb_xy;
                    } else if (j >= 1 && j <= 7) {
                        l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + 2 * (j - 1) + 1;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].v1 + left_stats->params.rawaf.ramdata[l_idx + 1].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].v2 + left_stats->params.rawaf.ramdata[l_idx + 1].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].h1 + left_stats->params.rawaf.ramdata[l_idx + 1].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].h2 + left_stats->params.rawaf.ramdata[l_idx + 1].h2;
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] =
                            left_stats->params.rawae3.data[l_idx].channelg_xy + left_stats->params.rawae3.data[l_idx + 1].channelg_xy;
                        lht0 = ((left_stats->params.rawae3.data[l_idx].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[l_idx].channelb_xy;
                        lht1 = ((left_stats->params.rawae3.data[l_idx + 1].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[l_idx + 1].channelb_xy;
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = lht0 + lht1;
                    } else {
                        r_idx = i * ISP2X_RAWAF_SUMDATA_ROW + 2 * (j - 8) + 1;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].v1 + right_stats->params.rawaf.ramdata[r_idx + 1].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].v2 + right_stats->params.rawaf.ramdata[r_idx + 1].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].h1 + right_stats->params.rawaf.ramdata[r_idx + 1].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].h2 + right_stats->params.rawaf.ramdata[r_idx + 1].h2;
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] =
                            right_stats->params.rawae3.data[r_idx].channelg_xy + right_stats->params.rawae3.data[r_idx + 1].channelg_xy;
                        lht0 = ((right_stats->params.rawae3.data[r_idx].channelr_xy & 0x3F) << 10) | right_stats->params.rawae3.data[r_idx].channelb_xy;
                        lht1 = ((right_stats->params.rawae3.data[r_idx + 1].channelr_xy & 0x3F) << 10) | right_stats->params.rawae3.data[r_idx + 1].channelb_xy;
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = lht0 + lht1;
                    }
                }
            }
        }
        else if (af_split_info.wina_side_info == LEFT_AND_RIGHT_MODE) {
            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                j = ISP2X_RAWAF_SUMDATA_ROW - af_split_info.wina_l_blknum;
                for (k = 0; k < af_split_info.wina_l_blknum; j++, k++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + k;
                    l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v1;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v2;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h1;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h2;
                    statsInt->af_stats_v3x.wnda_luma[dst_idx] = left_stats->params.rawae3.data[l_idx].channelg_xy;
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] =
                        ((left_stats->params.rawae3.data[l_idx].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[l_idx].channelb_xy;
                }
            }

            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                j = af_split_info.wina_r_skip_blknum;
                for (k = 0; k < af_split_info.wina_r_blknum; j++, k++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + k + af_split_info.wina_l_blknum;
                    r_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].v1;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].v2;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].h1;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].h2;

                    statsInt->af_stats_v3x.wnda_luma[dst_idx] = right_stats->params.rawae3.data[r_idx].channelg_xy;
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] =
                        ((right_stats->params.rawae3.data[r_idx].channelr_xy & 0x3F) << 10) | right_stats->params.rawae3.data[r_idx].channelb_xy;
                }
            }

            if (af_split_info.wina_r_skip_blknum == 0) {
                for (j = 0; j < ISP2X_RAWAF_SUMDATA_COLUMN; j++) {
                    dst_idx = j * ISP2X_RAWAF_SUMDATA_ROW + (af_split_info.wina_l_blknum - 1);
                    l_idx = j * ISP2X_RAWAF_SUMDATA_ROW + (ISP2X_RAWAF_SUMDATA_COLUMN - 1);
                    r_idx = j * ISP2X_RAWAF_SUMDATA_ROW;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].v1 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].v1 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].v2 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].v2 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].h1 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].h1 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].h2 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].h2 * af_split_info.wina_r_ratio;

                    statsInt->af_stats_v3x.wnda_luma[dst_idx] =
                        left_stats->params.rawae3.data[l_idx].channelg_xy * af_split_info.wina_l_ratio +
                        right_stats->params.rawae3.data[r_idx].channelg_xy * af_split_info.wina_r_ratio;
                    l_lht = ((left_stats->params.rawae3.data[l_idx].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[l_idx].channelb_xy;
                    r_lht = ((right_stats->params.rawae3.data[r_idx].channelr_xy & 0x3F) << 10) | right_stats->params.rawae3.data[r_idx].channelb_xy;
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] =
                        l_lht * af_split_info.wina_l_ratio + r_lht * af_split_info.wina_r_ratio;
                }
            }
        } else if (af_split_info.wina_side_info == LEFT_MODE) {
            for (i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
                statsInt->af_stats_v3x.wnda_fv_v1[i] = left_stats->params.rawaf.ramdata[i].v1;
                statsInt->af_stats_v3x.wnda_fv_v2[i] = left_stats->params.rawaf.ramdata[i].v2;
                statsInt->af_stats_v3x.wnda_fv_h1[i] = left_stats->params.rawaf.ramdata[i].h1;
                statsInt->af_stats_v3x.wnda_fv_h2[i] = left_stats->params.rawaf.ramdata[i].h2;

                statsInt->af_stats_v3x.wnda_luma[i] = left_stats->params.rawae3.data[i].channelg_xy;
                statsInt->af_stats_v3x.wina_highlit_cnt[i] =
                    ((left_stats->params.rawae3.data[i].channelr_xy & 0x3F) << 10) | left_stats->params.rawae3.data[i].channelb_xy;
            }
        } else {
            for (i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
                statsInt->af_stats_v3x.wnda_fv_v1[i] = right_stats->params.rawaf.ramdata[i].v1;
                statsInt->af_stats_v3x.wnda_fv_v2[i] = right_stats->params.rawaf.ramdata[i].v2;
                statsInt->af_stats_v3x.wnda_fv_h1[i] = right_stats->params.rawaf.ramdata[i].h1;
                statsInt->af_stats_v3x.wnda_fv_h2[i] = right_stats->params.rawaf.ramdata[i].h2;

                statsInt->af_stats_v3x.wnda_luma[i] = right_stats->params.rawae3.data[i].channelg_xy;
                statsInt->af_stats_v3x.wina_highlit_cnt[i] =
                    ((right_stats->params.rawae3.data[i].channelr_xy & 0x3F) << 10) | right_stats->params.rawae3.data[i].channelb_xy;
            }
        }

        LOGD_AF("af_split_info.wina: %d, %d, %d, %d, %f, %f",
                af_split_info.wina_side_info, af_split_info.wina_l_blknum, af_split_info.wina_r_blknum,
                af_split_info.wina_r_skip_blknum, af_split_info.wina_l_ratio, af_split_info.wina_r_ratio);
        LOGD_AF("af_split_info.winb: %d, %f, %f",
                af_split_info.winb_side_info, af_split_info.winb_l_ratio, af_split_info.winb_r_ratio);

        if(afParams.ptr()) {
            statsInt->af_stats_v3x.focusCode = afParams->data()->focusCode;
            statsInt->af_stats_v3x.zoomCode = afParams->data()->zoomCode;
            statsInt->af_stats_v3x.focus_endtim = afParams->data()->focusEndTim;
            statsInt->af_stats_v3x.focus_starttim = afParams->data()->focusStartTim;
            statsInt->af_stats_v3x.zoom_endtim = afParams->data()->zoomEndTim;
            statsInt->af_stats_v3x.zoom_starttim = afParams->data()->zoomStartTim;
            statsInt->af_stats_v3x.sof_tim = afParams->data()->sofTime;
            statsInt->af_stats_v3x.focusCorrection = afParams->data()->focusCorrection;
            statsInt->af_stats_v3x.zoomCorrection = afParams->data()->zoomCorrection;
            statsInt->af_stats_v3x.angleZ = afParams->data()->angleZ;
        }
    }

    return ret;
}


XCamReturn
RkAiqResourceTranslatorV3x::translateAfStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAfStatsProxy> &to)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp3x_isp_stat_buffer *stats;
    SmartPtr<RkAiqAfStats> statsInt = to->data();

    if (mIsMultiIsp) {
        return translateMultiAfStats(from, to);
    }

    stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x",
                  stats->frame_id, stats->meas_type);

    SmartPtr<RkAiqAfInfoProxy> afParams = buf->get_af_params();

    memset(&statsInt->af_stats_v3x, 0, sizeof(rk_aiq_isp_af_stats_v3x_t));
    statsInt->frame_id = stats->frame_id;

    //af
    {
        statsInt->af_stats_valid =
            (stats->meas_type >> 6) & (0x01) ? true : false;

        statsInt->af_stats_v3x.int_state = stats->params.rawaf.int_state;
        statsInt->af_stats_v3x.wndb_luma = stats->params.rawaf.afm_lum_b;
        statsInt->af_stats_v3x.wndb_sharpness = stats->params.rawaf.afm_sum_b;
        statsInt->af_stats_v3x.winb_highlit_cnt = stats->params.rawaf.highlit_cnt_winb;
        for (int i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
            statsInt->af_stats_v3x.wnda_fv_v1[i] = stats->params.rawaf.ramdata[i].v1;
            statsInt->af_stats_v3x.wnda_fv_v2[i] = stats->params.rawaf.ramdata[i].v2;
            statsInt->af_stats_v3x.wnda_fv_h1[i] = stats->params.rawaf.ramdata[i].h1;
            statsInt->af_stats_v3x.wnda_fv_h2[i] = stats->params.rawaf.ramdata[i].h2;

            statsInt->af_stats_v3x.wnda_luma[i] = stats->params.rawae3.data[i].channelg_xy;
            statsInt->af_stats_v3x.wina_highlit_cnt[i] =
                ((stats->params.rawae3.data[i].channelr_xy & 0x3F) << 10) | stats->params.rawae3.data[i].channelb_xy;
        }

        if(afParams.ptr()) {
            statsInt->af_stats_v3x.focusCode = afParams->data()->focusCode;
            statsInt->af_stats_v3x.zoomCode = afParams->data()->zoomCode;
            statsInt->af_stats_v3x.focus_endtim = afParams->data()->focusEndTim;
            statsInt->af_stats_v3x.focus_starttim = afParams->data()->focusStartTim;
            statsInt->af_stats_v3x.zoom_endtim = afParams->data()->zoomEndTim;
            statsInt->af_stats_v3x.zoom_starttim = afParams->data()->zoomStartTim;
            statsInt->af_stats_v3x.sof_tim = afParams->data()->sofTime;
            statsInt->af_stats_v3x.focusCorrection = afParams->data()->focusCorrection;
            statsInt->af_stats_v3x.zoomCorrection = afParams->data()->zoomCorrection;
            statsInt->af_stats_v3x.angleZ = afParams->data()->angleZ;
        }
    }

    return ret;
}

XCamReturn
RkAiqResourceTranslatorV3x::translateAdehazeStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAdehazeStatsProxy> &to)
{

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp3x_isp_stat_buffer *stats;
    SmartPtr<RkAiqAdehazeStats> statsInt = to->data();

    if (mIsMultiIsp) {
        return translateMultiAdehazeStats(from, to);
    }

    stats = (struct rkisp3x_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x",
                  stats->frame_id, stats->meas_type);
    //dehaze
    statsInt->adehaze_stats_valid = stats->meas_type >> 17 & 1;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_air_base = stats->params.dhaz.dhaz_adp_air_base;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_wt = stats->params.dhaz.dhaz_adp_wt;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_gratio = stats->params.dhaz.dhaz_adp_gratio;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_adp_wt = stats->params.dhaz.dhaz_adp_wt;
    statsInt->adehaze_stats.dehaze_stats_v30.dhaz_pic_sumh_left = stats->params.dhaz.dhaz_pic_sumh;
    for(int i = 0; i < ISP3X_DHAZ_HIST_IIR_NUM; i++)
        statsInt->adehaze_stats.dehaze_stats_v30.h_rgb_iir[i] = stats->params.dhaz.h_rgb_iir[i];

    to->set_sequence(stats->frame_id);

    return ret;
}

} //namespace RkCam
