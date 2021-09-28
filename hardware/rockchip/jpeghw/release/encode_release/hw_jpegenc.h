#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __RK_HW_JPEGENC_H__
#define __RK_HW_JPEGENC_H__
#include "vpu_global.h"
#include "vpu_mem_pool.h"


typedef enum
{
    JPEGENC_YUV420_P = 0,   //JPEGENC_YUV420_PLANAR; /* YYYY... UUUU... VVVV */
    JPEGENC_YUV420_SP = 1,  //JPEGENC_YUV420_SEMIPLANAR; /* YYYY... UVUVUV...   */
    JPEGENC_YUV422_YUYV = 2,    //JPEGENC_YUV422_INTERLEAVED_YUYV; /* YUYVYUYV...           */
    JPEGENC_YUV422_UYVY = 3,    //JPEGENC_YUV422_INTERLEAVED_UYVY; /* UYVYUYVY...           */
    HWJPEGENC_RGB565 = 4,
    HWJPEGENC_RGB888 = 10
} JpegEncType;

typedef struct{
    uint32_t num;
    uint32_t denom;
}rat_t;

#define EXIF_TAG_SUBSEC_TIME                    0x9290
#define EXIF_TAG_SUBSEC_TIME_ORIG               0x9291
#define EXIF_TAG_SUBSEC_TIME_DIG                0x9292

#define EXIF_TYPE_BYTE              1
#define EXIF_TYPE_ASCII             2
#define EXIF_TYPE_SHORT             3
#define EXIF_TYPE_LONG              4
#define EXIF_TYPE_RATIONAL          5
#define EXIF_TYPE_UNDEFINED         7
#define EXIF_TYPE_SLONG             9
#define EXIF_TYPE_SRATIONAL         10

typedef struct{
    /*IFD0*/
    char *maker;//manufacturer of digicam, just to adjust to make inPhybusAddr to align to 64
    int makerchars;//length of maker, contain the end '\0', so equal strlen(maker)+1
    char *modelstr;//model number of digicam
    int modelchars;//length of modelstr, contain the end '\0'
    int Orientation;//usually 1
    //XResolution, YResolution; if need be not 72, TODO...
    char DateTime[20];//must be 20 chars->  yyyy:MM:dd0x20hh:mm:ss'\0'
    /*Exif SubIFD*/
    rat_t ExposureTime;//such as 1/400=0.0025s
    rat_t ApertureFNumber;//actual f-number
    int ISOSpeedRatings;//CCD sensitivity equivalent to Ag-Hr film speedrate
    rat_t CompressedBitsPerPixel;
    rat_t ShutterSpeedValue;
    rat_t ApertureValue;
    rat_t ExposureBiasValue;
    rat_t MaxApertureValue;
    int MeteringMode;
    int Flash;
    rat_t FocalLength;
    rat_t FocalPlaneXResolution;
    rat_t FocalPlaneYResolution;
    int SensingMethod;//2 means 1 chip color area sensor
    int FileSource;//3 means the image source is digital still camera
    int CustomRendered;//0
    int ExposureMode;//0
    int WhiteBalance;//0
    rat_t DigitalZoomRatio;// inputw/inputw
    //int FocalLengthIn35mmFilm;
    int SceneCaptureType;//0
    char *makernote;
    int  makernotechars;//length of makernote, include of the end '\0'
    char subsectime[8];
}RkExifInfo;

typedef struct
{
    /*GPS IFD*/
    //int GpsInfoPrecent;
    char GPSLatitudeRef[2];//'N\0' 'S\0'
    rat_t GPSLatitude[3];
    char GPSLongitudeRef[2];//'E\0' 'W\0'
    rat_t GPSLongitude[3];
    char GPSAltitudeRef;
    rat_t GPSAltitude;
    rat_t GpsTimeStamp[3];
    char GpsDateStamp[11];//"YYYY:MM:DD\0"

    char *GPSProcessingMethod;//[101]
    int GpsProcessingMethodchars;//length of GpsProcessingMethod
}RkGPSInfo;

typedef enum{
    DEGREE_0 = 0,
    DEGREE_90 = 1,
    DEGREE_270 = 2,
    DEGREE_180 = 3
}JpegEncDegree;
typedef struct
{
    int frameHeader;//if 1, insert all headers(SOI,APP0,DQT,SOF0,DRI,DHT,SOS);if 0, insert only APP0 and SOS headers
    JpegEncDegree rotateDegree;//if degree is 90 or 270, check that width and height and thumbwidth and thumbheight must % 16 = 0.
    int y_rgb_addr;
    int uv_addr;
    int yuvaddrfor180;//if rotate 180, we need another phy buf. use ipp to do rotating 180. TO DO by soft handler  
    int inputW;//inputW >= (encodedW+15)&(~15) and inputW%16=0(for YUV420)
    int inputH;//inputH >= encodedH and inputH%8=0(for YUV420)
    //int encodedW;//encodedW%4=0, >= 96
    //int encodedH;//encodedH%2=0, >=32
    JpegEncType type;
    int qLvl;

    int doThumbNail;//insert thumbnail at APP0 extension if motionjpeg, else at APP1 extension(exifinfo should not be null)
    const void *thumbData;//if thumbData is NULL, do scale, the type above can be JPEGENC_YUV420_SP only
    int thumbDataLen;
    int thumbW;//if thumbData is not NULL, ignore this. even. [96,255]
    int thumbH;//thumbW*thumbH % 8 = 0
    int thumbqLvl;
    RkExifInfo *exifInfo;//if dothumbnail and thumbdata is null and insert all header, this must not be null
    RkGPSInfo *gpsInfo;//be null when gps is not set, else not be null
    unsigned char* y_vir_addr;
    unsigned char* uv_vir_addr;
    vpu_display_mem_pool *pool;
}JpegEncInInfo;

typedef struct
{
    int outBufPhyAddr;
    unsigned char* outBufVirAddr;
    int finalOffset;//out invalid data offset to outBufAddr above
    int outBuflen;//1024 + thumbnail length + init jpeg length + thumbnaillength(tmp buf)
    int jpegFileLen;
    int JpegHeaderLen;//jpeg head len include thumb
    int ThumbFileLen;//thumb jpeg lenth 
    int (*cacheflush)(int buf_type, int offset, int len);
}JpegEncOutInfo;

extern int hw_jpeg_encode(JpegEncInInfo *inInfo, JpegEncOutInfo *outInfo);
extern int doSoftScale(uint8_t *srcy, uint8_t *srcuv, int srcw, int srch, uint8_t *dsty, uint8_t *dstuv, int dstw, int dsth, int flag, int format);
#endif
#ifdef __cplusplus
}
#endif
