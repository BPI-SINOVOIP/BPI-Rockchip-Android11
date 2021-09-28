#include <utils/Log.h>
#include <math.h>
#include <string.h>
typedef enum {
    HDR2SDR_EETF          = 0, 
    HDR2SDR_BT1886OETF    = 1
} TF_Type;
    
typedef struct hdr2sdr_cfg_yn {
    TF_Type      type;
    unsigned int max_luma_master;
    unsigned int min_luma_master;
    unsigned int max_luma_dst;
    unsigned int min_luma_dst;
    unsigned int hdr2sdr_tf[33];
} hdr2sdr_cfg_yn;

static const hdr2sdr_cfg_yn hdr2sdr_eetf_cfgs[5] = {
    {HDR2SDR_EETF, 1200, 200, 500, 500, 
    {1716, 
     1880,  2067,   2277,  2508,  2758,  3026,  3310,  3609, 
     3921,  4246,   4581,  4925,  5279,  5640,  6007,  6380, 
     6758,  7140,   7526,  7914,  8304,  8694,  9074,  9438, 
     9779,  10093,  10373, 10615, 10812, 10960, 11053, 11084}},
    {HDR2SDR_EETF, 1200, 200, 300, 500,
    {1745,  
     1885,  2053,  2249,  2469,  2712,  2976,  3257,  3556,  
     3869,  4196,  4535,  4884,  5242,  5608,  5980,  6358,
     6740,  7127,  7505,  7866,  8208,  8527,  8824,  9095,
     9340,  9557,  9744,  9900,  10024, 10114, 10169, 10188}},
    {HDR2SDR_EETF, 1200, 200, 400, 500,
    {1728,  
     1882,  2061,  2265,  2492,  2739,  3005,  3288,  3586,
     3899,  4224,  4561,  4907,  5262,  5625,  5995,  6370,
     6750,  7134,  7521,  7910,  8292,  8657,  9002,  9324,
     9620,  9886,  10120, 10318, 10477, 10594, 10666, 10691}},
    {HDR2SDR_EETF, 1200, 200, 600, 500,
    {1707,   1878,   2071,   2286,   2521,   2773,   3043,   3328,   3627,
     3939,   4263,   4597,   4940,   5292,   5652,   6018,   6389,   6766,
     7146,   7531,   7918,   8307,   8698,   9091,   9480,   9858,  10215,
     10542,  10830,  11069,  11251,  11367,  11407}},    
    {HDR2SDR_EETF, 1200, 200, 700, 500,
    {1700,   1877,   2075,   2293,   2531,   2786,   3057,   3343,   3642,
     3954,   4277,   4611,   4953,   5304,   5662,   6027,   6397,   6772,
     7152,   7535,   7921,   8309,   8700,   9092,   9486,   9884,  10276,
     10646,  10981,  11267,  11488,  11631,  11681}},     
};

static const hdr2sdr_cfg_yn hdr2sdr_bt1886oetf_cfgs[5] = {
    {HDR2SDR_BT1886OETF, 500, 500, 500, 500,
    {0,
     0,     0,     0,     0,     0,     0,     0,     314,
     746,   1323,  2093,  2657,  3120,  3519,  3874,  4196,
     4492,  5024,  5498,  5928,  6323,  7034,  7666,  8239,
     8766,  9716,  10560, 11325, 12029, 13296, 14422, 16383}},
    {HDR2SDR_BT1886OETF, 500, 500, 300, 500,
    {0,      0,      0,      0,      0,      0,      0,      0,     71,
     447,    963,    1671,   2200,   2640,   3023,   3366,   3679,   3968,
     4493,   4963,   5391,   5788,   6507,   7151,   7739,   8282,   9267,
     10150,  10955,  11700,  13050,  14259,  16383}},     
    {HDR2SDR_BT1886OETF, 500, 500, 400, 500,
    {0,      0,      0,      0,      0,      0,      0,      0,    185,
     559,    1072,   1774,   2300,   2737,   3117,   3457,   3768,   4055,
     4576,   5043,   5469,   5862,   6576,   7216,   7799,   8339,   9317,
     10193,  10993,  11733,  13073,  14274,  16383}},     
    {HDR2SDR_BT1886OETF, 500, 500, 600, 500,
    {0,      0,      0,      0,      0,      0,      0,     51,    321,
     692,    1200,   1897,   2418,   2851,   3228,   3566,   3874,   4159,
     4675,   5138,   5560,   5951,   6659,   7293,   7871,   8407,   9377,
     10245,  11038,  11772,  13101,  14292,  16383}},    
    {HDR2SDR_BT1886OETF, 500, 500, 700, 500,
    {0,      0,      0,      0,      0,      0,      0,     97,    366,
     736,    1243,   1937,   2457,   2889,   3265,   3602,   3909,   4193,
     4708,   5170,   5591,   5980,   6686,   7318,   7895,   8429,   9396,
     10263,  11053,  11785,  13110,  14297,  16383}},
};

static const char* hdr2sdr_cfg_str_mask33 = "%s %d \
%d %d %d %d %d %d %d %d \
%d %d %d %d %d %d %d %d \
%d %d %d %d %d %d %d %d \
%d %d %d %d %d %d %d %d";

#define ARRAY_INTERATE33(x) x[0],\
    x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],\
    x[9],x[10],x[11],x[12],x[13],x[14],x[15],x[16],\
    x[17],x[18],x[19],x[20],x[21],x[22],x[23],x[24],\
    x[25],x[26],x[27],x[28],x[29],x[30],x[31],x[32]

static const char* hdr2sdr_cfg_node = "/sys/class/graphics/fb0/hdr2sdr_yn";

/* static bool vop_write_cfg(hdr2sdr_cfg_yn cfg) {
    char tmp[512];
    memset(tmp, 0, sizeof(tmp));
    switch(cfg.type) {
    case HDR2SDR_EETF:
        snprintf(tmp, 512, hdr2sdr_cfg_str_mask33, "hdr2sdr_eetf", ARRAY_INTERATE33(cfg.hdr2sdr_tf));
        break;
    case HDR2SDR_BT1886OETF:
        snprintf(tmp, 512, hdr2sdr_cfg_str_mask33, "hdr2sdr_bt1886oetf", ARRAY_INTERATE33(cfg.hdr2sdr_tf));
        break;
    default:
        return false;
    }

    FILE* file = fopen(hdr2sdr_cfg_node, "w");
    if (file == NULL) {
        ALOGE("vop_write_cfg(): open fail");
        return false;
    }

    ALOGE("%s", tmp);
    fputs(tmp, file);    
    fclose(file);
    
    return true;
} */


#define HDR2SDR_SEG_COUNT 32
#define HDR2SDR_XINDEX_COUNT 33
#define HDR2SDR_BIT_DEPTH 14
static float LinearToNonLinear(float v) {
    float c1 = 3424.0 / 4096;
    float c2 = 2413.0 / 4096 * 32;
    float c3 = 2392.0 / 4096 * 32;
    float m = 2523.0 / 4096 * 128;
    float n = 2610.0 / 4096 * (1.0 / 4);
    float ret = 1.0 * pow ((c1 + c2 * pow(v, n)) / (1 + c3 * pow(v , n)), m);
    return ret > 0 ? ret : 0;
}

static float makeHDR2SDREETF(float _maxlumi_master, float _minlumi_master,
			float _maxlumi_dst, float _minlumi_dst, 
			int* eetf_out = NULL, int* maxlumi_out = NULL, int* minlumi_out = NULL) {
    ALOGE("RKTEST makeHDR2SDREETF");
    float maxlumi_master = _maxlumi_master / 10000.0;
    float minlumi_master = _minlumi_master / 10000.0;
    float maxlumi_dst = _maxlumi_dst / 10000.0;
    float minlumi_dst = _minlumi_dst / 10000.0;

    float xdst[HDR2SDR_SEG_COUNT];
    int   xdst_bit[HDR2SDR_SEG_COUNT];
    int   xdst_index[HDR2SDR_XINDEX_COUNT];
    int   ydst_index[HDR2SDR_XINDEX_COUNT];
    float xdst_init_value = 32 * 16;
    int   xdst_bit_init_value = 9;
    xdst_index[0] = 0;
    
    int i = 0;
    float cumsum = 0;
    for(i = 0;i < HDR2SDR_SEG_COUNT; i++) {
        xdst[i] = xdst_init_value;
        xdst_bit[i] = xdst_bit_init_value;
        cumsum += xdst[i];
        xdst_index[i + 1] = cumsum;
    }

    float maxlumi_dst_nonlinear = LinearToNonLinear(maxlumi_dst);
    float minlumi_dst_nonlinear = LinearToNonLinear(minlumi_dst);
    float maxlumi_master_nonlinear = LinearToNonLinear(maxlumi_master);
    float minlumi_master_nonlinear = LinearToNonLinear(minlumi_master);
    float ks = 1.5 * maxlumi_dst_nonlinear - 0.5;
    // float b = minlumi_dst_nonlinear;
    int   xbitmask = pow(2, HDR2SDR_BIT_DEPTH) - 1;
    int   ybitmask = pow(2, HDR2SDR_BIT_DEPTH) - 1;
    
    float I0[HDR2SDR_XINDEX_COUNT];
    float I0TS[HDR2SDR_XINDEX_COUNT];
    float Im[HDR2SDR_XINDEX_COUNT];
    for (i = 0; i < HDR2SDR_XINDEX_COUNT; i++) {
        I0[i] = xdst_index[i] / float(xbitmask) * maxlumi_master_nonlinear;
        I0TS[i] = (I0[i] - ks) / (maxlumi_master_nonlinear - ks);
        Im[i] = I0[i] * (I0[i] < ks ? 1 : 0) + 
                ((2 * pow(I0TS[i], 3) - 3 * pow(I0TS[i], 2) + 1) * ks + 
                    (pow(I0TS[i], 3) - 2 * pow(I0TS[i], 2) + I0TS[i]) * (maxlumi_master_nonlinear - ks) +
                    (-2 * pow(I0TS[i], 3) + 3 * pow(I0TS[i], 2)) * maxlumi_dst_nonlinear) * (I0[i] >= ks ? 1 : 0);
        // float p = Im[i];
        
        Im[i] = Im[i] + (minlumi_dst_nonlinear - minlumi_master_nonlinear) / pow((maxlumi_dst_nonlinear - minlumi_master_nonlinear), 4) * pow((maxlumi_dst_nonlinear - Im[i]) > 0 ?(maxlumi_dst_nonlinear - Im[i]) : 0, 4);
        ydst_index[i] = Im[i] * ybitmask;
        ydst_index[i] = ydst_index[i] > 0 ? ydst_index[i] : 0; // eetf
        if (eetf_out)
            eetf_out[i] = ydst_index[i];
    }

    if (maxlumi_out)
        *maxlumi_out = maxlumi_dst * ybitmask;

    if (minlumi_out)
        *minlumi_out = minlumi_dst * ybitmask;

    //makeHDR2SDROETF(500, 0.5);
    return 0;
}




#define LOG2(A) (log(A)/log(2))

static float makeHDR2SDROETF(float _maxlumi_dst, float _minlumi_dst,
    int* oetf_out = NULL)
{
    float maxlumi_dst = _maxlumi_dst / 10000.0;
    float minlumi_dst = _minlumi_dst / 10000.0;    
    float xdst[HDR2SDR_SEG_COUNT] = {
            1.0/64, 1.0/64, 1.0/32, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 8,
			8, 8, 8, 8, 8, 16, 16, 16, 16, 32, 32, 32, 32, 64, 64, 64, 64, 128, 128, 256};    
    int xdst_bit[HDR2SDR_SEG_COUNT];    
    float xdst_index[HDR2SDR_XINDEX_COUNT];        
    int ydst_index[HDR2SDR_XINDEX_COUNT];
    int xbitmask = pow(2, 16) - 1;
    int ybitmask = pow(2, 14) - 1;
    int i;
    
    float cumsum = 0;
    for (i = 0; i < HDR2SDR_SEG_COUNT; i++) {
        xdst_bit[i] = LOG2(xdst[i] * 64);
        cumsum += (xdst[i] * 64);
        xdst_index[i] = cumsum / xbitmask;
    }
    xdst_index[HDR2SDR_XINDEX_COUNT - 1] = 1;

    float r = 2.2;
    float exp_v = 1.0 / 2.2;
    float a = pow(pow(maxlumi_dst, exp_v) - pow(minlumi_dst, exp_v), r);
    float b = pow(minlumi_dst, exp_v) / (pow(maxlumi_dst, exp_v) - pow(minlumi_dst, exp_v));

    ydst_index[0] = -b;
    for (i = 0; i < HDR2SDR_XINDEX_COUNT; i++) {
        if (i == 0)
            ydst_index[i] = ydst_index[i] * ybitmask;
        else {
            ydst_index[i] = (pow(xdst_index[i - 1] * maxlumi_dst / a, exp_v) - b) * ybitmask;
        }
        if (ydst_index[i] < 0)
            ydst_index[i] = 0;

        if (oetf_out)
            oetf_out[i] = ydst_index[i];
    }
    
    return 0;
}


static int makeMaxMin(float _maxlumi_master, float _minlumi_master,
			float _maxlumi_dst, float _minlumi_dst, int* maxMin){
    ALOGE("RKTEST makeHDR2SDREETF _maxlumi_master = %f,_minlumi_master = %f",_maxlumi_master,_minlumi_master);
    float maxlumi_dst = _maxlumi_dst / 10000.0;
    float minlumi_dst = _minlumi_dst / 10000.0;
    // int   xbitmask = pow(2, HDR2SDR_BIT_DEPTH) - 1;
    int   ybitmask = pow(2, HDR2SDR_BIT_DEPTH) - 1;

    if (maxMin)
        maxMin[0] = maxlumi_dst * ybitmask;

    if (maxMin)
        maxMin[1] = minlumi_dst * ybitmask;

    //makeHDR2SDROETF(500, 0.5);
    return 0;
}

