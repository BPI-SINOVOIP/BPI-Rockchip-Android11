#include <stdio.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/log.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "baseparameter_util.h"

#define BASE_OFFSET 8*1024
#define BACKUP_OFFSET 512*1024
#define TEST_BASE_PARMARTER
#define DEFAULT_BRIGHTNESS  50
#define DEFAULT_CONTRAST  50
#define DEFAULT_SATURATION  50
#define DEFAULT_HUE  50

#define BUFFER_LENGTH    256
#define RESOLUTION_AUTO 1<<0
#define COLOR_AUTO (1<<1)
#define HDCP1X_EN (1<<2)
#define RESOLUTION_WHITE_EN (1<<3)

#define BASEPARAMETER_IMAGE_SIZE 1024*1024
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "BaseParameterUtilV1", __VA_ARGS__)

enum {
    HWC_DISPLAY_PRIMARY     = 0,
    HWC_DISPLAY_EXTERNAL    = 1,    // HDMI, DP, etc.
    HWC_DISPLAY_VIRTUAL     = 2,

    HWC_NUM_PHYSICAL_DISPLAY_TYPES = 2,
    HWC_NUM_DISPLAY_TYPES          = 3,
};
enum {
    HWC_DISPLAY_PRIMARY_BIT     = 1 << HWC_DISPLAY_PRIMARY,
    HWC_DISPLAY_EXTERNAL_BIT    = 1 << HWC_DISPLAY_EXTERNAL,
    HWC_DISPLAY_VIRTUAL_BIT     = 1 << HWC_DISPLAY_VIRTUAL,
};

struct lut_data{
    uint16_t size;
    uint16_t lred[1024];
    uint16_t lgreen[1024];
    uint16_t lblue[1024];
};

struct drm_display_mode {
    /* Proposed mode values */
    int clock;		/* in kHz */
    int hdisplay;
    int hsync_start;
    int hsync_end;
    int htotal;
    int vdisplay;
    int vsync_start;
    int vsync_end;
    int vtotal;
    int vrefresh;
    int vscan;
    unsigned int flags;
    int picture_aspect_ratio;
};

enum output_format {
    output_rgb=0,
    output_ycbcr444=1,
    output_ycbcr422=2,
    output_ycbcr420=3,
    output_ycbcr_high_subsampling=4,  // (YCbCr444 > YCbCr422 > YCbCr420 > RGB)
    output_ycbcr_low_subsampling=5	, // (RGB > YCbCr420 > YCbCr422 > YCbCr444)
    invalid_output=6,
};

enum output_depth {
    Automatic=0,
    depth_24bit=8,
    depth_30bit=10,
};

struct overscan {
    unsigned int maxvalue;
    unsigned short leftscale;
    unsigned short rightscale;
    unsigned short topscale;
    unsigned short bottomscale;
};

struct hwc_inital_info{
    char device[128];
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    float fps;
};
struct bcsh_info {
    unsigned short brightness;
    unsigned short contrast;
    unsigned short saturation;
    unsigned short hue;
};

struct screen_info {
    int type;
    struct drm_display_mode resolution;// 52 bytes
    enum output_format  format; // 4 bytes
    enum output_depth depthc; // 4 bytes
    unsigned int feature;//4 //4 bytes
};

struct disp_info{
    struct screen_info screen_list[5];
    struct overscan scan;//12 bytes
    struct hwc_inital_info hwc_info; //140 bytes
    struct bcsh_info bcsh;
    unsigned int reserve[128]; //459x4
    struct lut_data mlutdata;/*6k+4*/
};


struct file_base_paramer
{
    struct disp_info main;
    struct disp_info aux;
};

struct file_base_paramer mBase_paramer;
struct file_base_paramer mBackup_paramer;
bool has_baseparameter = false;
static char const *const device_template[] =
{
    "/dev/block/platform/1021c000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30020000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/fe330000.sdhci/by-name/baseparameter",
    "/dev/block/platform/ff520000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/ff0f0000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30030000.nandc/by-name/baseparameter",
    "/dev/block/rknand_baseparameter",
    "/dev/block/by-name/baseparameter",
    NULL
};

int mDpy;

const char* GetBaseparameterFile(void)
{
    int i = 0;

    while (device_template[i]) {
        if (!access(device_template[i], R_OK | W_OK))
            return device_template[i];
        ALOGD("temp[%d]=%s access=%d(%s)", i,device_template[i], errno, strerror(errno));
        i++;
    }
    return NULL;
}

static int findSuitableInfoSlot(struct disp_info* info, int type)
{
    int found=0;
    for (int i=0;i<5;i++) {
        if (info->screen_list[i].type !=0 && info->screen_list[i].type == type) {
            found = i;
            break;
        } else if (info->screen_list[i].type !=0 && found == false){
            found++;
        }
    }
    if (found == -1) {
        found = 0;
        ALOGD("noting saved, used the first slot");
    }
    ALOGD("findSuitableInfoSlot: %d type=%d", found, type);
    return found;
}

static void saveResolutionInfo(struct file_base_paramer *base_paramer, int dpy, int type){
    int slot=-1;
    unsigned int left,top,right,bottom;

    if (type <= 0)
        type = DRM_MODE_CONNECTOR_HDMIA;
    left = top = right = bottom = 95;

    if (dpy == HWC_DISPLAY_PRIMARY) {
        slot = findSuitableInfoSlot(&base_paramer->main, type);
        base_paramer->main.screen_list[slot].resolution.clock = 148500;
        base_paramer->main.screen_list[slot].resolution.hdisplay = 1920;
        base_paramer->main.screen_list[slot].resolution.hsync_start = 2008;
        base_paramer->main.screen_list[slot].resolution.hsync_end = 2052;
        base_paramer->main.screen_list[slot].resolution.htotal = 2200;
        base_paramer->main.screen_list[slot].resolution.vdisplay = 1080;
        base_paramer->main.screen_list[slot].resolution.vsync_start = 1084;
        base_paramer->main.screen_list[slot].resolution.vsync_end = 1089;
        base_paramer->main.screen_list[slot].resolution.vtotal = 1125;
        base_paramer->main.screen_list[slot].resolution.vrefresh = 60;
        base_paramer->main.screen_list[slot].resolution.vscan = 0;
        base_paramer->main.screen_list[slot].resolution.flags = 0x5;

        base_paramer->main.scan.maxvalue = 100;
        base_paramer->main.scan.leftscale = (unsigned short)left;
        base_paramer->main.scan.topscale = (unsigned short)top;
        base_paramer->main.scan.rightscale = (unsigned short)right;
        base_paramer->main.scan.bottomscale = (unsigned short)bottom;
    } else {
        slot = findSuitableInfoSlot(&base_paramer->aux, type);
        base_paramer->aux.screen_list[slot].resolution.clock = 148500;
        base_paramer->aux.screen_list[slot].resolution.hdisplay = 1920;
        base_paramer->aux.screen_list[slot].resolution.hsync_start = 2008;
        base_paramer->aux.screen_list[slot].resolution.hsync_end = 2052;
        base_paramer->aux.screen_list[slot].resolution.htotal = 2200;
        base_paramer->aux.screen_list[slot].resolution.vdisplay = 1080;
        base_paramer->aux.screen_list[slot].resolution.vsync_start = 1084;
        base_paramer->aux.screen_list[slot].resolution.vsync_end = 1089;
        base_paramer->aux.screen_list[slot].resolution.vtotal = 1125;
        base_paramer->aux.screen_list[slot].resolution.vrefresh = 60;
        base_paramer->aux.screen_list[slot].resolution.vscan = 0;
        base_paramer->aux.screen_list[slot].resolution.flags = 0x5;

        base_paramer->aux.scan.maxvalue = 100;
        base_paramer->aux.scan.leftscale = (unsigned short)left;
        base_paramer->aux.scan.topscale = (unsigned short)top;
        base_paramer->aux.scan.rightscale = (unsigned short)right;
        base_paramer->aux.scan.bottomscale = (unsigned short)bottom;
    }
}

static void saveHwcInitalInfo(struct file_base_paramer *base_paramer, int dpy, char* fb_info, char* device){
    int fb_w=0, fb_h=0;
    int fps=0;
    printf("fb_info=%s devices=%s 2\n",fb_info, device);
    if (fb_info != NULL)
        sscanf(fb_info, "%dx%d@%d\0", &fb_w, &fb_h, &fps);
    else {
        fb_w = 1920;
        fb_h = 1080;
        printf("error: cant get fb_info\n");
    }
    printf("%d %d %d\n", fb_w, fb_h, fps);
    if (dpy == HWC_DISPLAY_PRIMARY){
        int len;
        char property[PROPERTY_VALUE_MAX];
        base_paramer->main.hwc_info.framebuffer_width = fb_w;
        base_paramer->main.hwc_info.framebuffer_height = fb_h;
        base_paramer->main.hwc_info.fps = fps;
        memset(property,0,sizeof(property));
        len = property_get("vendor.hwc.device.primary", property, NULL);
        if (len && device==NULL) {
            memcpy(base_paramer->main.hwc_info.device, property, strlen(property));
        } else if (device != NULL){
            sprintf(base_paramer->main.hwc_info.device, "%s", device);
        } else {
            base_paramer->main.hwc_info.device[0]='\0';
        }
    } else {
        int len;
        char property[PROPERTY_VALUE_MAX];
        base_paramer->aux.hwc_info.framebuffer_width = fb_w;
        base_paramer->aux.hwc_info.framebuffer_height = fb_h;
        base_paramer->aux.hwc_info.fps = fps;
        memset(property,0,sizeof(property));
        len = property_get("vendor.hwc.device.extend", property, NULL);
        if (len && device==NULL)
            memcpy(base_paramer->aux.hwc_info.device, property, strlen(property));
        else if (device != NULL)
            sprintf(base_paramer->aux.hwc_info.device, "%s", device);
        else
            base_paramer->aux.hwc_info.device[0]='\0';
    }
}

static void saveBcshConfig(struct file_base_paramer *base_paramer, int dpy){
    if (dpy == HWC_DISPLAY_PRIMARY){
        char property[PROPERTY_VALUE_MAX];

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.brightness.main", property, "0");
        if (atoi(property) > 0)
            base_paramer->main.reserve[0] = atoi(property);
        else
            base_paramer->main.reserve[0] = DEFAULT_BRIGHTNESS;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.contrast.main", property, "0");
        if (atoi(property) > 0)
            base_paramer->main.reserve[1] = atoi(property);
        else
            base_paramer->main.reserve[1] = DEFAULT_CONTRAST;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.saturation.main", property, "0");
        if (atoi(property) > 0)
            base_paramer->main.reserve[2] = atoi(property);
        else
            base_paramer->main.reserve[2] = DEFAULT_SATURATION;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.hue.main", property, "0");
        if (atoi(property) > 0)
            base_paramer->main.reserve[3] = atoi(property);
        else
            base_paramer->main.reserve[3] = DEFAULT_HUE;
    } else {
        char property[PROPERTY_VALUE_MAX];

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.brightness.aux", property, "0");
        if (atoi(property) > 0)
            base_paramer->aux.reserve[0] = atoi(property);
        else
            base_paramer->aux.reserve[0] = DEFAULT_BRIGHTNESS;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.contrast.aux", property, "0");
        if (atoi(property) > 0)
            base_paramer->aux.reserve[1] = atoi(property);
        else
            base_paramer->aux.reserve[1] = DEFAULT_CONTRAST;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.saturation.aux", property, "0");
        if (atoi(property) > 0)
            base_paramer->aux.reserve[2] = atoi(property);
        else
            base_paramer->aux.reserve[2] = DEFAULT_SATURATION;

        memset(property,0,sizeof(property));
        property_get("persist.vendor.sys.hue.aux", property, "0");
        if (atoi(property) > 0)
            base_paramer->aux.reserve[3] = atoi(property);
        else
            base_paramer->aux.reserve[3] = DEFAULT_HUE;
    }
}

static int saveConfig(struct file_base_paramer *paramer)
{
    int file;
    int ret;
    const char *baseparameterfile = GetBaseparameterFile();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        return -EIO;
    }
    lseek(file, 0, SEEK_SET);
    ret = write(file, (char*)paramer, sizeof(mBase_paramer));
    if (ret < 0) {
        LOGD("fail to write");
        close(file);
        return -EIO;
    }
    fsync(file);
    close(file);

    return 0;
}

int getTypeFromConnector() {
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        ALOGE("Failed to open dri- %s", strerror(-errno));
        return -ENODEV;
    }
    int ret = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
        ALOGE("Failed to set universal plane cap %d", ret);
        return ret;
    }

    ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret) {
        ALOGE("Failed to set atomic cap %d", ret);
        return ret;
    }

    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) {
        ALOGE("Failed to get DrmResources resources");
        return -ENODEV;
    }
    for (int i = 0; !ret && i < res->count_connectors; ++i) {
        drmModeConnectorPtr c = drmModeGetConnector(fd, res->connectors[i]);
        ALOGD("connector_type=%d", c->connector_type);
        if (!c) {
            ALOGE("Failed to get connector %d", res->connectors[i]);
            ret = -ENODEV;
            break;
        }

        drmModeFreeConnector(c);
    }
    if (res)
        drmModeFreeResources(res);
    if (fd > 0)
        close(fd);
    return 1;
}

static void printParameter(struct file_base_paramer *base_paramer){
    printf("-main: \n");
    for (int i=0;i<5;i++) {
        if (base_paramer->main.screen_list[i].type != 0 ) {
            printf("\tresolution: slot[%d] type=%d %dx%d@p-%d-%d-%d-%d-%d-%d-%x clk=%d\n",
                    i, base_paramer->main.screen_list[i].type,
                    base_paramer->main.screen_list[i].resolution.hdisplay,
                    base_paramer->main.screen_list[i].resolution.vdisplay,
                    base_paramer->main.screen_list[i].resolution.hsync_start,
                    base_paramer->main.screen_list[i].resolution.hsync_end,
                    base_paramer->main.screen_list[i].resolution.htotal,
                    base_paramer->main.screen_list[i].resolution.vsync_start,
                    base_paramer->main.screen_list[i].resolution.vsync_end,
                    base_paramer->main.screen_list[i].resolution.vtotal,
                    base_paramer->main.screen_list[i].resolution.flags,
                    base_paramer->main.screen_list[i].resolution.clock);
            printf("\tcorlor: format %d depth %d \n", base_paramer->main.screen_list[i].format,
                    base_paramer->main.screen_list[i].depthc);
            printf("\tfeature:  0x%x \n", base_paramer->main.screen_list[i].feature);
        }
    }
    printf("\tfbinfo: %dx%d@%f device:%s\n", base_paramer->main.hwc_info.framebuffer_width,
            base_paramer->main.hwc_info.framebuffer_height, base_paramer->main.hwc_info.fps,
            base_paramer->main.hwc_info.device);
    printf("\tbcsh: %d %d %d %d \n", base_paramer->main.bcsh.brightness, base_paramer->main.bcsh.contrast,
            base_paramer->main.bcsh.saturation, base_paramer->main.bcsh.hue);
    printf("\toverscan: %d %d %d %d \n", base_paramer->main.scan.leftscale, base_paramer->main.scan.topscale,
            base_paramer->main.scan.rightscale, base_paramer->main.scan.bottomscale);
    //    printf("\tfeature:  0x%x \n", base_paramer->main.feature);

    printf("-aux: \n");
    for (int i=0;i<5;i++) {
        if (base_paramer->aux.screen_list[i].type != 0 ) {
            printf("\tresolution:slot[%d] type=%d %dx%d@p-%d-%d-%d-%d-%d-%d-%x clk=%d\n",
                    i, base_paramer->aux.screen_list[i].type,
                    base_paramer->aux.screen_list[i].resolution.hdisplay,
                    base_paramer->aux.screen_list[i].resolution.vdisplay,
                    base_paramer->aux.screen_list[i].resolution.hsync_start,
                    base_paramer->aux.screen_list[i].resolution.hsync_end,
                    base_paramer->aux.screen_list[i].resolution.htotal,
                    base_paramer->aux.screen_list[i].resolution.vsync_start,
                    base_paramer->main.screen_list[i].resolution.vsync_end,
                    base_paramer->aux.screen_list[i].resolution.vtotal,
                    base_paramer->aux.screen_list[i].resolution.flags,
                    base_paramer->aux.screen_list[i].resolution.clock);
            printf("\tcorlor: format %d depth %d \n", base_paramer->aux.screen_list[i].format,
                    base_paramer->aux.screen_list[i].depthc);
            printf("\tfeature:  0x%x \n", base_paramer->aux.screen_list[i].feature);
        }
    }
    printf("\tfbinfo: %dx%d@%f device:%s\n", base_paramer->aux.hwc_info.framebuffer_width,
            base_paramer->aux.hwc_info.framebuffer_height, base_paramer->aux.hwc_info.fps,
            base_paramer->aux.hwc_info.device);
    printf("\tbcsh: %d %d %d %d \n", base_paramer->aux.bcsh.brightness, base_paramer->aux.bcsh.contrast,
            base_paramer->aux.bcsh.saturation, base_paramer->aux.bcsh.hue);
    printf("\toverscan: %d %d %d %d \n", base_paramer->aux.scan.leftscale, base_paramer->aux.scan.topscale,
            base_paramer->aux.scan.rightscale, base_paramer->aux.scan.bottomscale);
    //  printf("\tfeature:  0x%x \n", base_paramer->aux.feature);

    getTypeFromConnector();
}

BaseParameterUtilV1::BaseParameterUtilV1() {
    mDpy = 0;
    int file;
    const char *baseparameterfile = GetBaseparameterFile();
    if (!baseparameterfile) {
        sync();
        printf("can't find baseParameter");
    }else {
        file = open(baseparameterfile, O_RDWR);
        if (file < 0) {
            printf("base paramter file can not be opened \n");
            sync();
        } else {
            // caculate file's size and read it
            unsigned int length = lseek(file, 0L, SEEK_END);
            if(length < sizeof(mBase_paramer)) {
                printf("BASEPARAME data's length is error\n");
                close(file);
            }else {
                lseek(file, 0L, SEEK_SET);
                read(file, (void*)&(mBase_paramer.main), sizeof(mBase_paramer.main));/*read main display info*/
                lseek(file, BASE_OFFSET, SEEK_SET);
                read(file, (void*)&(mBase_paramer.aux), sizeof(mBase_paramer.aux));/*read aux display info*/
                lseek(file, BACKUP_OFFSET, SEEK_SET);
                read(file, (void*)&(mBackup_paramer.main), sizeof(mBackup_paramer.main));/*read main display info*/
                lseek(file, BACKUP_OFFSET + BASE_OFFSET, SEEK_SET);
                read(file, (void*)&(mBackup_paramer.aux), sizeof(mBackup_paramer.aux));/*read aux display info*/
                has_baseparameter = true;
                close(file);
            }
        }
    }
}

BaseParameterUtilV1::~BaseParameterUtilV1() {

}

void BaseParameterUtilV1::print()
{
    if(has_baseparameter){
        printf("========== base parameter ==========\n");
        printParameter(&mBase_paramer);
        printf("\n========= backup parameter ==========\n");
        printParameter(&mBackup_paramer);
        printf("====================================\n");
    }
}

bool BaseParameterUtilV1::validate()
{
    return false;
}

int BaseParameterUtilV1::dump_baseparameter(const char *file_path){
    int file;
    int ret;
    const char *baseparameterfile = GetBaseparameterFile();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        return -EIO;
    }
    char *data = (char *)malloc(BASEPARAMETER_IMAGE_SIZE);
    lseek(file, 0L, SEEK_SET);
    ret = read(file, data, BASEPARAMETER_IMAGE_SIZE);
    if (ret < 0) {
        LOGD("fail to read");
        close(file);
        free(data);
        return -EIO;
    }
    close(file);
    file = open(file_path, O_CREAT | O_WRONLY, 0666);
    LOGD("open %s file %d errno %d", file_path, file, errno);
    if (file < 0) {
        LOGD("fail to open");
        free(data);
        return -EIO;
    }
    lseek(file, BASEPARAMETER_IMAGE_SIZE-1, SEEK_SET);
    ret = write(file, "\0", 1);
    if (ret < 0) {
        LOGD("fail to write");
        close(file);
        free(data);
        return -EIO;
    }
    lseek(file, 0L, SEEK_SET);
    ret = write(file, (char*)data, BASEPARAMETER_IMAGE_SIZE);
    fsync(file);
    close(file);
    free(data);
    LOGD("dump_baseparameter %s success\n", file_path);

    return 0;
}

void BaseParameterUtilV1::setDisplayId(int dpy){
    mDpy = dpy;
}

void BaseParameterUtilV1::setConnectorTypeAndId(int connectorType, int connectorId){
    return;
}

int BaseParameterUtilV1::setBcsh(int b, int c, int s, int h) {
    if(mDpy == HWC_DISPLAY_PRIMARY){
        mBase_paramer.main.bcsh.brightness = b;
        mBase_paramer.main.bcsh.contrast = c;
        mBase_paramer.main.bcsh.saturation = s;
        mBase_paramer.main.bcsh.hue = h;
    } else {
        mBase_paramer.aux.bcsh.brightness = b;
        mBase_paramer.aux.bcsh.contrast = c;
        mBase_paramer.aux.bcsh.saturation = s;
        mBase_paramer.aux.bcsh.hue = h;
    }
    return saveConfig(&mBase_paramer);
}

int BaseParameterUtilV1::setOverscan(int left, int top, int right, int bottom) {
    if(mDpy == HWC_DISPLAY_PRIMARY){
        mBase_paramer.main.scan.leftscale = left;
        mBase_paramer.main.scan.topscale = top;
        mBase_paramer.main.scan.rightscale = right;
        mBase_paramer.main.scan.bottomscale = bottom;
    } else {
        mBase_paramer.aux.scan.leftscale = left;
        mBase_paramer.aux.scan.topscale = top;
        mBase_paramer.aux.scan.rightscale = right;
        mBase_paramer.aux.scan.bottomscale = bottom;
    }
    return saveConfig(&mBase_paramer);

}

int BaseParameterUtilV1::setFramebufferInfo(int width, int height, int fps){
    if(mDpy == HWC_DISPLAY_PRIMARY){
        mBase_paramer.main.hwc_info.framebuffer_width = width;
        mBase_paramer.main.hwc_info.framebuffer_height = height;
        mBase_paramer.main.hwc_info.fps = (float)fps;
    } else {
        mBase_paramer.aux.hwc_info.framebuffer_width = width;
        mBase_paramer.aux.hwc_info.framebuffer_height = height;
        mBase_paramer.aux.hwc_info.fps = (float)fps;
    }
    return saveConfig(&mBase_paramer);
}

int BaseParameterUtilV1::setColor(int format, int depth, int feature){
    if(mDpy == HWC_DISPLAY_PRIMARY){
        mBase_paramer.main.screen_list[0].format = (output_format)format;
        mBase_paramer.main.screen_list[0].depthc = (output_depth)depth;
        mBase_paramer.main.screen_list[0].feature = feature;
    } else {
        mBase_paramer.aux.screen_list[0].format = (output_format)format;
        mBase_paramer.aux.screen_list[0].depthc = (output_depth)depth;
        mBase_paramer.aux.screen_list[0].feature = feature;
    }
    return saveConfig(&mBase_paramer);
}

int BaseParameterUtilV1::setResolution(int hdisplay, int vdisplay, int vrefresh, int hsync_start, int hsync_end, int htotal, int vsync_start, int vsync_end, int vtotal, int vscan, int flags, int clock, int feature){
    if(mDpy == HWC_DISPLAY_PRIMARY){
        mBase_paramer.main.screen_list[0].resolution.hdisplay = hdisplay;
        mBase_paramer.main.screen_list[0].resolution.vdisplay = vdisplay;
        mBase_paramer.main.screen_list[0].resolution.vrefresh = vrefresh;
        mBase_paramer.main.screen_list[0].resolution.hsync_start = hsync_start;
        mBase_paramer.main.screen_list[0].resolution.hsync_end = hsync_end;
        mBase_paramer.main.screen_list[0].resolution.htotal = htotal;
        mBase_paramer.main.screen_list[0].resolution.vsync_start = vsync_start;
        mBase_paramer.main.screen_list[0].resolution.vsync_end = vsync_end;
        mBase_paramer.main.screen_list[0].resolution.vtotal = vtotal;
        mBase_paramer.main.screen_list[0].resolution.vscan = vscan;
        mBase_paramer.main.screen_list[0].resolution.clock = clock;
        mBase_paramer.main.screen_list[0].resolution.flags = flags;
        mBase_paramer.main.screen_list[0].feature = feature;
    } else {
        mBase_paramer.aux.screen_list[0].resolution.hdisplay = hdisplay;
        mBase_paramer.aux.screen_list[0].resolution.vdisplay = vdisplay;
        mBase_paramer.aux.screen_list[0].resolution.vrefresh = vrefresh;
        mBase_paramer.aux.screen_list[0].resolution.hsync_start = hsync_start;
        mBase_paramer.aux.screen_list[0].resolution.hsync_end = hsync_end;
        mBase_paramer.aux.screen_list[0].resolution.htotal = htotal;
        mBase_paramer.aux.screen_list[0].resolution.vsync_start = vsync_start;
        mBase_paramer.aux.screen_list[0].resolution.vsync_end = vsync_end;
        mBase_paramer.aux.screen_list[0].resolution.vtotal = vtotal;
        mBase_paramer.aux.screen_list[0].resolution.vscan = vscan;
        mBase_paramer.aux.screen_list[0].resolution.clock = clock;
        mBase_paramer.aux.screen_list[0].resolution.flags = flags;
        mBase_paramer.aux.screen_list[0].feature = feature;
    }
    return saveConfig(&mBase_paramer);

}

