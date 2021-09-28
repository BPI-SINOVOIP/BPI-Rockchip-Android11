#define ADRC_Y_NUM             17


typedef struct AdrcGain_s {
    // M4_ARRAY_DESC("EnvLv", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0",M4_DIGIT(3), M4_DYNAMIC(1))
    float*            EnvLv;
    int EnvLv_len;
    // M4_ARRAY_DESC("DrcGain", "f32", M4_SIZE(1,100), M4_RANGE(1,8), "4",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            DrcGain; //sw_drc_gain
    int DrcGain_len;
    // M4_ARRAY_DESC("Alpha", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0.2",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            Alpha;
    int Alpha_len;
    // M4_ARRAY_DESC("Clip", "f32", M4_SIZE(1,100), M4_RANGE(0,64), "16",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            Clip;  //sw_drc_position, step: 1/255
    int Clip_len;
} AdrcGain_t;

typedef struct HighLight_s {
    // M4_ARRAY_DESC("EnvLv", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0",M4_DIGIT(3), M4_DYNAMIC(1))
    float*            EnvLv;
    int EnvLv_len;
    // M4_ARRAY_DESC("Strength", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "1",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            Strength;  //sw_drc_weig_maxl,  range[0,1], step 1/16
    int Strength_len;
} HighLight_t;

typedef struct LocalData_s
{
    // M4_ARRAY_DESC("EnvLv", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            EnvLv;
    int EnvLv_len;
    // M4_ARRAY_DESC("LocalWeit", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "1",M4_DIGIT(2), M4_DYNAMIC(1))
    float*            LocalWeit;  //sw_drc_weig_bilat, range[0 , 1], step: 1/16
    int LocalWeit_len;
    // M4_ARRAY_DESC("GlobalContrast", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0",M4_DIGIT(3), M4_DYNAMIC(1))
    float*            GlobalContrast; //sw_drc_lpdetail_ratio, setp 1/4096
    int GlobalContrast_len;
    // M4_ARRAY_DESC("LoLitContrast", "f32", M4_SIZE(1,100), M4_RANGE(0,1), "0",M4_DIGIT(3), M4_DYNAMIC(1))
    float*            LoLitContrast; //sw_drc_hpdetail_ratio, setp 1/4096
    int LoLitContrast_len;
} LocalData_t;

typedef struct local_s {
    // M4_ARRAY_TABLE_DESC("LocalTMOData", "array_table_ui", "none")
    LocalData_t LocalTMOData;
    // M4_NUMBER_DESC("curPixWeit", "f32", M4_RANGE(0,1), "0.37", M4_DIGIT(3))
    float curPixWeit; //sw_drc_weicur_pix,  range[0,1],step: 1/255
    // M4_NUMBER_DESC("preFrameWeit", "f32", M4_RANGE(0,1), "1.0", M4_DIGIT(3))
    float preFrameWeit;//sw_adrc_weipre_frame ,range[0,1],step: 1/255
    // M4_NUMBER_DESC("Range_force_sgm", "f32", M4_RANGE(0,1), "0.0", M4_DIGIT(4))
    float Range_force_sgm; //sw_drc_force_sgm_inv0 ,range[0,1], step 1/8191
    // M4_NUMBER_DESC("Range_sgm_cur", "f32", M4_RANGE(0,1), "0.125", M4_DIGIT(4))
    float Range_sgm_cur; //sw_drc_range_sgm_inv1, range[0,1], step 1/8191
    // M4_NUMBER_DESC("Range_sgm_pre", "f32", M4_RANGE(0,1), "0.125", M4_DIGIT(4))
    float Range_sgm_pre; //sw_drc_range_sgm_inv0,range[0,1], step 1/8191
    // M4_NUMBER_DESC("Space_sgm_cur", "u16", M4_RANGE(0,4095), "4068", M4_DIGIT(0))
    int Space_sgm_cur; //sw_drc_space_sgm_inv1
    // M4_NUMBER_DESC("Space_sgm_pre", "u16", M4_RANGE(0,4095), "3968", M4_DIGIT(0))
    int Space_sgm_pre; //sw_drc_space_sgm_inv0
} local_t;

typedef enum CompressMode_e {
    COMPRESS_AUTO     = 0,
    COMPRESS_MANUAL   = 1,
} CompressMode_t;

typedef struct Compress_s {
    // M4_ENUM_DESC("Mode", "CompressMode_t", "COMPRESS_AUTO")
    CompressMode_t Mode;
    // M4_ARRAY_MARK_DESC("Manual_curve", "u32", M4_SIZE(1,17),  M4_RANGE(0, 8192), "[0, 558, 1087, 1588, 2063, 2515, 2944, 3353, 3744, 4473, 5139, 5751, 6316, 6838, 7322, 7772, 8192]", M4_DIGIT(0), M4_DYNAMIC(0), "curve_table")
    uint16_t       Manual_curve[ADRC_Y_NUM];
} Compress_t;

typedef struct CalibDbV2_Adrc_s {
    // M4_BOOL_DESC("Enable", "1")
    bool Enable;
    // M4_ARRAY_TABLE_DESC("DrcGain", "array_table_ui", "none")
    AdrcGain_t DrcGain;
    // M4_ARRAY_TABLE_DESC("HiLight", "array_table_ui", "none")
    HighLight_t HiLight;
    // M4_STRUCT_DESC("LocalTMOSetting", "normal_ui_style")
    local_t LocalTMOSetting;
    // M4_STRUCT_DESC("CompressSetting", "normal_ui_style")
    Compress_t CompressSetting;
    // M4_ARRAY_DESC("Scale_y", "u16", M4_SIZE(1,17),  M4_RANGE(0, 2048), "[0,2,20,76,193,381,631,772,919,1066,1211,1479,1700,1863,1968,2024,2048]", M4_DIGIT(0), M4_DYNAMIC(0))
    int Scale_y[ADRC_Y_NUM];
    // M4_NUMBER_DESC("ByPassThr", "f32", M4_RANGE(0,1), "0", M4_DIGIT(4))
    float ByPassThr;
    // M4_NUMBER_DESC("Edge_Weit", "f32",  M4_RANGE(0,1), "1",M4_DIGIT(3))
    float Edge_Weit; //sw_drc_edge_scl, range[0,1], step 1/255
    // M4_BOOL_DESC("OutPutLongFrame", "0")
    bool  OutPutLongFrame;  //sw_drc_min_ogain
    // M4_NUMBER_DESC("IIR_frame", "u8", M4_RANGE(1,1000), "16", M4_DIGIT(0))
    int IIR_frame; //sw_drc_iir_frame, range [1, 1000]
    // M4_NUMBER_DESC("Tolerance", "f32", M4_RANGE(0,1), "0", M4_DIGIT(3))
    float                  Tolerance;
    // M4_NUMBER_DESC("damp", "f32", M4_RANGE(0,1), "0.9", M4_DIGIT(3))
    float damp;
} CalibDbV2_Adrc_t;

typedef struct CalibDbV2_drc_s {
    // M4_STRUCT_DESC("DrcTuningPara", "normal_ui_style")
    CalibDbV2_Adrc_t DrcTuningPara;
} CalibDbV2_drc_t;

#pragma once
