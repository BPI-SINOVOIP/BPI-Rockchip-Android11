#ifndef __HDMITVINFO_H__
#define __HDMITVINFO_H__


#define HMW_UINT8 unsigned char
#define HMW_UINT32 unsigned int
#define HMW_VOID void

//HMW_OutputIoctl_E
typedef enum {
    HMW_HDMI_RK_UNKNOW,   /*#HMW_OutputEvent_F#*/   /*set event callback*/
    HMW_HDMI_RK_GET_TV_INFO,
    HMW_HDMI_RK_BUTT
} HMW_HDMIRK_Ioctl_E; 

//HMW_TV_INFO_S

typedef struct {

    HMW_UINT8  manufName[4];   //从HDMI的EDID信息中获取（接收端，如显示器）的厂商ID，如果HDMI未连接或获取不到则不填
    HMW_UINT32 manufModel;    //从HDMI的EDID信息中获取（接收端，如显示器）的产品ID，如果HDMI未连接或获取不到则不填
    HMW_UINT32 manufYear;     //从HDMI的EDID信息中获取（接收端，如显示器）的生产日志，如果HDMI未连接或获取不到则不填
    HMW_UINT8  displaySize[8];  //屏幕尺寸
} HMW_TV_INFO_S;

/**
 * Dataspace Definitions
 * ======================
 *
 * Dataspace is the definition of how pixel values should be interpreted.
 *
 * For many formats, this is the colorspace of the image data, which includes
 * primaries (including white point) and the transfer characteristic function,
 * which describes both gamma curve and numeric range (within the bit depth).
 *
 * Other dataspaces include depth measurement data from a depth camera.
 *
 * A dataspace is comprised of a number of fields.
 *
 * Version
 * --------
 * The top 2 bits represent the revision of the field specification. This is
 * currently always 0.
 *
 *
 * bits    31-30 29                      -                          0
 *        +-----+----------------------------------------------------+
 * fields | Rev |            Revision specific fields                |
 *        +-----+----------------------------------------------------+
 *
 * Field layout for version = 0:
 * ----------------------------
 *
 * A dataspace is comprised of the following fields:
 *      Standard
 *      Transfer function
 *      Range
 *
 * bits    31-30 29-27 26 -  22 21 -  16 15             -           0
 *        +-----+-----+--------+--------+----------------------------+
 * fields |  0  |Range|Transfer|Standard|    Legacy and custom       |
 *        +-----+-----+--------+--------+----------------------------+
 *          VV    RRR   TTTTT    SSSSSS    LLLLLLLL       LLLLLLLL
 *
 * If range, transfer and standard fields are all 0 (e.g. top 16 bits are
 * all zeroes), the bottom 16 bits contain either a legacy dataspace value,
 * or a custom value.
 */

typedef enum android_dataspace {
    /*
     * Default-assumption data space, when not explicitly specified.
     *
     * It is safest to assume the buffer is an image with sRGB primaries and
     * encoding ranges, but the consumer and/or the producer of the data may
     * simply be using defaults. No automatic gamma transform should be
     * expected, except for a possible display gamma transform when drawn to a
     * screen.
     */
    HAL_DATASPACE_UNKNOWN = 0x0,

    /*
     * Arbitrary dataspace with manually defined characteristics.  Definition
     * for colorspaces or other meaning must be communicated separately.
     *
     * This is used when specifying primaries, transfer characteristics,
     * etc. separately.
     *
     * A typical use case is in video encoding parameters (e.g. for H.264),
     * where a colorspace can have separately defined primaries, transfer
     * characteristics, etc.
     */
    HAL_DATASPACE_ARBITRARY = 0x1,

    /*
     * Color-description aspects
     *
     * The following aspects define various characteristics of the color
     * specification. These represent bitfields, so that a data space value
     * can specify each of them independently.
     */

    HAL_DATASPACE_STANDARD_SHIFT = 16,

    /*
     * Standard aspect
     *
     * Defines the chromaticity coordinates of the source primaries in terms of
     * the CIE 1931 definition of x and y specified in ISO 11664-1.
     */
    HAL_DATASPACE_STANDARD_MASK = 63 << HAL_DATASPACE_STANDARD_SHIFT,  // 0x3F

    /*
     * Chromacity coordinates are unknown or are determined by the application.
     * Implementations shall use the following suggested standards:
     *
     * All YCbCr formats: BT709 if size is 720p or larger (since most video
     *                    content is letterboxed this corresponds to width is
     *                    1280 or greater, or height is 720 or greater).
     *                    BT601_625 if size is smaller than 720p or is JPEG.
     * All RGB formats:   BT709.
     *
     * For all other formats standard is undefined, and implementations should use
     * an appropriate standard for the data represented.
     */
    HAL_DATASPACE_STANDARD_UNSPECIFIED = 0 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.300   0.600
     *  blue            0.150   0.060
     *  red             0.640   0.330
     *  white (D65)     0.3127  0.3290
     *
     * Use the unadjusted KR = 0.2126, KB = 0.0722 luminance interpretation
     * for RGB conversion.
     */
    HAL_DATASPACE_STANDARD_BT709 = 1 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.290   0.600
     *  blue            0.150   0.060
     *  red             0.640   0.330
     *  white (D65)     0.3127  0.3290
     *
     *  KR = 0.299, KB = 0.114. This adjusts the luminance interpretation
     *  for RGB conversion from the one purely determined by the primaries
     *  to minimize the color shift into RGB space that uses BT.709
     *  primaries.
     */
    HAL_DATASPACE_STANDARD_BT601_625 = 2 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.290   0.600
     *  blue            0.150   0.060
     *  red             0.640   0.330
     *  white (D65)     0.3127  0.3290
     *
     * Use the unadjusted KR = 0.222, KB = 0.071 luminance interpretation
     * for RGB conversion.
     */
    HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED = 3 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.310   0.595
     *  blue            0.155   0.070
     *  red             0.630   0.340
     *  white (D65)     0.3127  0.3290
     *
     *  KR = 0.299, KB = 0.114. This adjusts the luminance interpretation
     *  for RGB conversion from the one purely determined by the primaries
     *  to minimize the color shift into RGB space that uses BT.709
     *  primaries.
     */
    HAL_DATASPACE_STANDARD_BT601_525 = 4 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.310   0.595
     *  blue            0.155   0.070
     *  red             0.630   0.340
     *  white (D65)     0.3127  0.3290
     *
     * Use the unadjusted KR = 0.212, KB = 0.087 luminance interpretation
     * for RGB conversion (as in SMPTE 240M).
     */
    HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED = 5 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.170   0.797
     *  blue            0.131   0.046
     *  red             0.708   0.292
     *  white (D65)     0.3127  0.3290
     *
     * Use the unadjusted KR = 0.2627, KB = 0.0593 luminance interpretation
     * for RGB conversion.
     */
    HAL_DATASPACE_STANDARD_BT2020 = 6 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.170   0.797
     *  blue            0.131   0.046
     *  red             0.708   0.292
     *  white (D65)     0.3127  0.3290
     *
     * Use the unadjusted KR = 0.2627, KB = 0.0593 luminance interpretation
     * for RGB conversion using the linear domain.
     */
    HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE = 7 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x      y
     *  green           0.21   0.71
     *  blue            0.14   0.08
     *  red             0.67   0.33
     *  white (C)       0.310  0.316
     *
     * Use the unadjusted KR = 0.30, KB = 0.11 luminance interpretation
     * for RGB conversion.
     */
    HAL_DATASPACE_STANDARD_BT470M = 8 << HAL_DATASPACE_STANDARD_SHIFT,

    /*
     * Primaries:       x       y
     *  green           0.243   0.692
     *  blue            0.145   0.049
     *  red             0.681   0.319
     *  white (C)       0.310   0.316
     *
     * Use the unadjusted KR = 0.254, KB = 0.068 luminance interpretation
     * for RGB conversion.
     */
    HAL_DATASPACE_STANDARD_FILM = 9 << HAL_DATASPACE_STANDARD_SHIFT,

    HAL_DATASPACE_TRANSFER_SHIFT = 22,

    /*
     * Transfer aspect
     *
     * Transfer characteristics are the opto-electronic transfer characteristic
     * at the source as a function of linear optical intensity (luminance).
     *
     * For digital signals, E corresponds to the recorded value. Normally, the
     * transfer function is applied in RGB space to each of the R, G and B
     * components independently. This may result in color shift that can be
     * minized by applying the transfer function in Lab space only for the L
     * component. Implementation may apply the transfer function in RGB space
     * for all pixel formats if desired.
     */

    HAL_DATASPACE_TRANSFER_MASK = 31 << HAL_DATASPACE_TRANSFER_SHIFT,  // 0x1F

    /*
     * Transfer characteristics are unknown or are determined by the
     * application.
     *
     * Implementations should use the following transfer functions:
     *
     * For YCbCr formats: use HAL_DATASPACE_TRANSFER_SMPTE_170M
     * For RGB formats: use HAL_DATASPACE_TRANSFER_SRGB
     *
     * For all other formats transfer function is undefined, and implementations
     * should use an appropriate standard for the data represented.
     */
    HAL_DATASPACE_TRANSFER_UNSPECIFIED = 0 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * Transfer characteristic curve:
     *  E = L
     *      L - luminance of image 0 <= L <= 1 for conventional colorimetry
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_LINEAR = 1 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * Transfer characteristic curve:
     *
     * E = 1.055 * L^(1/2.4) - 0.055  for 0.0031308 <= L <= 1
     *   = 12.92 * L                  for 0 <= L < 0.0031308
     *     L - luminance of image 0 <= L <= 1 for conventional colorimetry
     *     E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_SRGB = 2 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * BT.601 525, BT.601 625, BT.709, BT.2020
     *
     * Transfer characteristic curve:
     *  E = 1.099 * L ^ 0.45 - 0.099  for 0.018 <= L <= 1
     *    = 4.500 * L                 for 0 <= L < 0.018
     *      L - luminance of image 0 <= L <= 1 for conventional colorimetry
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_SMPTE_170M = 3 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * Assumed display gamma 2.2.
     *
     * Transfer characteristic curve:
     *  E = L ^ (1/2.2)
     *      L - luminance of image 0 <= L <= 1 for conventional colorimetry
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_GAMMA2_2 = 4 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     *  display gamma 2.8.
     *
     * Transfer characteristic curve:
     *  E = L ^ (1/2.8)
     *      L - luminance of image 0 <= L <= 1 for conventional colorimetry
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_GAMMA2_8 = 5 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * SMPTE ST 2084
     *
     * Transfer characteristic curve:
     *  E = ((c1 + c2 * L^n) / (1 + c3 * L^n)) ^ m
     *  c1 = c3 - c2 + 1 = 3424 / 4096 = 0.8359375
     *  c2 = 32 * 2413 / 4096 = 18.8515625
     *  c3 = 32 * 2392 / 4096 = 18.6875
     *  m = 128 * 2523 / 4096 = 78.84375
     *  n = 0.25 * 2610 / 4096 = 0.1593017578125
     *      L - luminance of image 0 <= L <= 1 for HDR colorimetry.
     *          L = 1 corresponds to 10000 cd/m2
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_ST2084 = 6 << HAL_DATASPACE_TRANSFER_SHIFT,

    /*
     * ARIB STD-B67 Hybrid Log Gamma
     *
     * Transfer characteristic curve:
     *  E = r * L^0.5                 for 0 <= L <= 1
     *    = a * ln(L - b) + c         for 1 < L
     *  a = 0.17883277
     *  b = 0.28466892
     *  c = 0.55991073
     *  r = 0.5
     *      L - luminance of image 0 <= L for HDR colorimetry. L = 1 corresponds
     *          to reference white level of 100 cd/m2
     *      E - corresponding electrical signal
     */
    HAL_DATASPACE_TRANSFER_HLG = 7 << HAL_DATASPACE_TRANSFER_SHIFT,

    HAL_DATASPACE_RANGE_SHIFT = 27,

    /*
     * Range aspect
     *
     * Defines the range of values corresponding to the unit range of 0-1.
     * This is defined for YCbCr only, but can be expanded to RGB space.
     */
    HAL_DATASPACE_RANGE_MASK = 7 << HAL_DATASPACE_RANGE_SHIFT,  // 0x7

    /*
     * Range is unknown or are determined by the application.  Implementations
     * shall use the following suggested ranges:
     *
     * All YCbCr formats: limited range.
     * All RGB or RGBA formats (including RAW and Bayer): full range.
     * All Y formats: full range
     *
     * For all other formats range is undefined, and implementations should use
     * an appropriate range for the data represented.
     */
    HAL_DATASPACE_RANGE_UNSPECIFIED = 0 << HAL_DATASPACE_RANGE_SHIFT,

    /*
     * Full range uses all values for Y, Cb and Cr from
     * 0 to 2^b-1, where b is the bit depth of the color format.
     */
    HAL_DATASPACE_RANGE_FULL = 1 << HAL_DATASPACE_RANGE_SHIFT,

    /*
     * Limited range uses values 16/256*2^b to 235/256*2^b for Y, and
     * 1/16*2^b to 15/16*2^b for Cb, Cr, R, G and B, where b is the bit depth of
     * the color format.
     *
     * E.g. For 8-bit-depth formats:
     * Luma (Y) samples should range from 16 to 235, inclusive
     * Chroma (Cb, Cr) samples should range from 16 to 240, inclusive
     *
     * For 10-bit-depth formats:
     * Luma (Y) samples should range from 64 to 940, inclusive
     * Chroma (Cb, Cr) samples should range from 64 to 960, inclusive
     */
    HAL_DATASPACE_RANGE_LIMITED = 2 << HAL_DATASPACE_RANGE_SHIFT,

    /*
     * Legacy dataspaces
     */

    /*
     * sRGB linear encoding:
     *
     * The red, green, and blue components are stored in sRGB space, but
     * are linear, not gamma-encoded.
     * The RGB primaries and the white point are the same as BT.709.
     *
     * The values are encoded using the full range ([0,255] for 8-bit) for all
     * components.
     */
    HAL_DATASPACE_SRGB_LINEAR = 0x200, // deprecated, use HAL_DATASPACE_V0_SRGB_LINEAR

    HAL_DATASPACE_V0_SRGB_LINEAR = HAL_DATASPACE_STANDARD_BT709 |
            HAL_DATASPACE_TRANSFER_LINEAR | HAL_DATASPACE_RANGE_FULL,


    /*
     * sRGB gamma encoding:
     *
     * The red, green and blue components are stored in sRGB space, and
     * converted to linear space when read, using the SRGB transfer function
     * for each of the R, G and B components. When written, the inverse
     * transformation is performed.
     *
     * The alpha component, if present, is always stored in linear space and
     * is left unmodified when read or written.
     *
     * Use full range and BT.709 standard.
     */
    HAL_DATASPACE_SRGB = 0x201, // deprecated, use HAL_DATASPACE_V0_SRGB

    HAL_DATASPACE_V0_SRGB = HAL_DATASPACE_STANDARD_BT709 |
            HAL_DATASPACE_TRANSFER_SRGB | HAL_DATASPACE_RANGE_FULL,


    /*
     * YCbCr Colorspaces
     * -----------------
     *
     * Primaries are given using (x,y) coordinates in the CIE 1931 definition
     * of x and y specified by ISO 11664-1.
     *
     * Transfer characteristics are the opto-electronic transfer characteristic
     * at the source as a function of linear optical intensity (luminance).
     */

    /*
     * JPEG File Interchange Format (JFIF)
     *
     * Same model as BT.601-625, but all values (Y, Cb, Cr) range from 0 to 255
     *
     * Use full range, BT.601 transfer and BT.601_625 standard.
     */
    HAL_DATASPACE_JFIF = 0x101, // deprecated, use HAL_DATASPACE_V0_JFIF

    HAL_DATASPACE_V0_JFIF = HAL_DATASPACE_STANDARD_BT601_625 |
            HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_RANGE_FULL,

    /*
     * ITU-R Recommendation 601 (BT.601) - 625-line
     *
     * Standard-definition television, 625 Lines (PAL)
     *
     * Use limited range, BT.601 transfer and BT.601_625 standard.
     */
    HAL_DATASPACE_BT601_625 = 0x102, // deprecated, use HAL_DATASPACE_V0_BT601_625

    HAL_DATASPACE_V0_BT601_625 = HAL_DATASPACE_STANDARD_BT601_625 |
            HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_RANGE_LIMITED,


    /*
     * ITU-R Recommendation 601 (BT.601) - 525-line
     *
     * Standard-definition television, 525 Lines (NTSC)
     *
     * Use limited range, BT.601 transfer and BT.601_525 standard.
     */
    HAL_DATASPACE_BT601_525 = 0x103, // deprecated, use HAL_DATASPACE_V0_BT601_525

    HAL_DATASPACE_V0_BT601_525 = HAL_DATASPACE_STANDARD_BT601_525 |
            HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_RANGE_LIMITED,

    /*
     * ITU-R Recommendation 709 (BT.709)
     *
     * High-definition television
     *
     * Use limited range, BT.709 transfer and BT.709 standard.
     */
    HAL_DATASPACE_BT709 = 0x104, // deprecated, use HAL_DATASPACE_V0_BT709

    HAL_DATASPACE_V0_BT709 = HAL_DATASPACE_STANDARD_BT709 |
            HAL_DATASPACE_TRANSFER_SMPTE_170M | HAL_DATASPACE_RANGE_LIMITED,

    /*
     * Data spaces for non-color formats
     */

    /*
     * The buffer contains depth ranging measurements from a depth camera.
     * This value is valid with formats:
     *    HAL_PIXEL_FORMAT_Y16: 16-bit samples, consisting of a depth measurement
     *       and an associated confidence value. The 3 MSBs of the sample make
     *       up the confidence value, and the low 13 LSBs of the sample make up
     *       the depth measurement.
     *       For the confidence section, 0 means 100% confidence, 1 means 0%
     *       confidence. The mapping to a linear float confidence value between
     *       0.f and 1.f can be obtained with
     *         float confidence = (((depthSample >> 13) - 1) & 0x7) / 7.0f;
     *       The depth measurement can be extracted simply with
     *         uint16_t range = (depthSample & 0x1FFF);
     *    HAL_PIXEL_FORMAT_BLOB: A depth point cloud, as
     *       a variable-length float (x,y,z, confidence) coordinate point list.
     *       The point cloud will be represented with the android_depth_points
     *       structure.
     */
    HAL_DATASPACE_DEPTH = 0x1000

} android_dataspace_t;

/*
 * Supported HDR formats. Must be kept in sync with equivalents in Display.java.
 */
typedef enum android_hdr {
    /* Device supports Dolby Vision HDR */
    HAL_HDR_DOLBY_VISION = 1,

    /* Device supports HDR10 */
    HAL_HDR_HDR10 = 2,

    /* Device supports hybrid log-gamma HDR */
    HAL_HDR_HLG = 3
} android_hdr_t;

struct HDMITVInfo {
	android_hdr_t hdrtype;
	android_dataspace_t dataspace;
};

int PortingOutputIoctl(HMW_HDMIRK_Ioctl_E op, HMW_VOID* arg);

/* Get TV supported dataspace, value is defined by android_dataspace_t */
int HdmiSupportedDataSpace(void);

int setHdmiHDR(int enable);

int HdmiSetColorimetry(android_dataspace_t Colorimetry);
#endif