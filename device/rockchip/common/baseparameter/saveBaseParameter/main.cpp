#include <cutils/properties.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#include "baseparameter_util.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "saveBaseparameter", __VA_ARGS__)
#define COLOR_AUTO (1<<1)
#define BACKUP_OFFSET 512*1024
#define BASEPARAMETER_IMAGE_SIZE 1024*1024
#define RESOLUTION_AUTO 1<<0

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


static void usage(bool isHwc2){
    fprintf(stderr, "\nsaveParameter: read and write baseparameter partition tool\n");
    fprintf(stderr, "\nUsage:\n");
    fprintf(stderr, "\t-h\t Help info\n");
    fprintf(stderr, "\t-p\t Print Baseparamter\n");
    fprintf(stderr, "\t-t\t output to target file (e: \"/sdcard/baseparameter.img)\"\n");
    fprintf(stderr, "\t-f\t Framebuffer Resolution (e: 1920x1080@60)\n");
    fprintf(stderr, "\t-c\t Color (e: RGB-8bit or YCBCR444-10bit)\n");
    fprintf(stderr, "\t-u\t Is Enable Auto Resolution (auto resolution:\"auto\";set one fixed resolution:\n");
    fprintf(stderr, "\t  \t hdisplay,vdisplay,vrefresh,hsync_start,hsync_end,htotal,vsync_start,vsync_end,vtotal,vscan,flags,clock\n");
    fprintf(stderr, "\t  \t e: \"1920,1080,60,2008,2052,2200,1084,1089,1125,0,5,148500\")\n");
    fprintf(stderr, "\t-o\t Overscan (left,top,right,bottom e: overscan \"100,100,100,100\")\n");
    fprintf(stderr, "\t-b\t BCSH (brightness,contrast,saturation,hue e: \"50,50,50,50\") \n");
    fprintf(stderr, "\t-R\t Reset Baseparameter (1:only reset user setting baseparameter partition; 2:reset baseparameter paratition include backup)\n");
    if(isHwc2){
    fprintf(stderr, "\t-C\t Choose Connector type and id to Setting (e: 11,0 or 16,0)\n");
    fprintf(stderr, "\nExample: saveBaseParameter -C \"16,0\" -f \"1920x1080@60\" -c Auto -u 2 -o \"100,100,100,100\" -b \"50,50,50,50\"\n");
} else{
    fprintf(stderr, "\t-d\t Choose Display to Setting (e: 0 or 1)\n");
    fprintf(stderr, "\nExample: saveBaseParameter -d 0 -f \"1920x1080@60\" -D \"HDMI-A,TV\" -c Auto -u 2 -o \"100,100,100,100\" -b \"50,50,50,50\"\n");
}
    fprintf(stderr, "\n===== Rockchip All Rights Reserved =====\n\n");
}

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

const char* getBaseparameterFile(void)
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

int reset(){
    int file;
    int ret;
    const char *baseparameterfile = getBaseparameterFile();
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
    char *data = (char *)malloc(BASEPARAMETER_IMAGE_SIZE / 2);
    lseek(file, BACKUP_OFFSET, SEEK_SET);
    ret = read(file, data, BASEPARAMETER_IMAGE_SIZE / 2);
    if (ret < 0) {
        LOGD("fail to read");
        close(file);
        free(data);
        return -EIO;
    }
    lseek(file, 0L, SEEK_SET);
    ret = write(file, (char*)data, BASEPARAMETER_IMAGE_SIZE / 2);
    fsync(file);
    close(file);
    free(data);
    return 0;
}

int main(int argc, char** argv){
    bool hasOpts = false;
    bool isHwc2 = false;
    int isSaveToTargetFile = 0;
    int isPrintBaseInfo = 0;
    int res;
    int display = -1;
    int isReset = 0;
    char property[100];
    char* target_save_file = "/sdcard/baseparameter.img";
    char* bcsh = NULL;
    char* connector = NULL;
    char* overscan = NULL;
    char* fb_info = NULL;
    char* corlor_info = NULL;
    char* resolution = NULL;

    property_get("vendor.ghwc.version", property, NULL);
    if (strstr(property, "HWC2") != NULL) {
        isHwc2 = true;
    }
    while ((res = getopt(argc, argv, "t:b:f:d:D:u:c:o:C:hapR")) >= 0) {
        //printf("res = %d\n", res);
        hasOpts = true;
        switch (res) {
            case 'p':
                isPrintBaseInfo = 1;
                printf("print baseparameter\n");
                break;
            case 'h':
                usage(isHwc2);
                return 0;
            case 't':
                target_save_file = optarg;
                isSaveToTargetFile = 1;
                printf("save to %s (-t)\n", target_save_file);
                break;
            case 'b':
                bcsh = optarg;
                printf("bcsh %s (-b)\n", bcsh);
                break;
            case 'C':
                connector = optarg;
                printf("connector %s (-C)\n", connector);
                break;
            case 'd':
                display = atoi(optarg);
                if (display > 1) {
                    usage(isHwc2);
                    return -1;
                }
                printf("display %d (-d)\n", display);
                break;
            case 'o':
                overscan = optarg;
                printf("overscan %s (-o)\n", overscan);
                break;
            case 'f':
                fb_info = optarg;
                if (strstr(fb_info, "x") == NULL || strstr(fb_info, "@")==NULL){
                    usage(isHwc2);
                    return -1;
                }
                printf("framebuffer %s (-f)\n", fb_info);
                break;
            case 'c':
                corlor_info = optarg;
                printf("color %s (-c)\n", corlor_info);
                break;
            case 'u':
                resolution = optarg;
                printf("resolution %s (-u)\n", resolution);
                break;
            case 'R':
                isReset = 1;
                printf("reset baseparameter\n");
                break;
            default:
                usage(isHwc2);
                return 0;
        }
    }

    if (hasOpts == false) {
        usage(isHwc2);
        return 0;
    }
    BaseParameterUtil* util;
    if(isHwc2) {
        util = new BaseParameterUtilV2();
    } else {
        util = new BaseParameterUtilV1();
    }
    if(isPrintBaseInfo > 0){
        util->print();
    }
    if(isSaveToTargetFile > 0){
        int ret = util->dump_baseparameter(target_save_file);
        if(ret == 0){
            printf("save to %s successfully\n", target_save_file);
        } else {
            printf("save to %s failed\n", target_save_file);
        }
    }
    if(connector != NULL){
        int type, id;
        sscanf(connector, "%d,%d", &type, &id);
        util->setConnectorTypeAndId(type, id);
    }
    if(display >= 0){
        util->setDisplayId(display);
    }
    if (bcsh != NULL) {
        int b,c,s,h;
        sscanf(bcsh, "%d,%d,%d,%d", &b, &c, &s, &h);
        util->setBcsh(b,c,s,h);
    }

    if(overscan != NULL){
        int left, top, right, bottom;
        sscanf(overscan, "%d,%d,%d,%d", &left, &top, &right, &bottom);
        util->setOverscan(left, top, right, bottom);
    }
    if(fb_info != NULL){
        int fb_w, fb_h, fps;
        sscanf(fb_info, "%dx%d@%d\0", &fb_w, &fb_h, &fps);
        util->setFramebufferInfo(fb_w, fb_h, fps);
    }
    if(corlor_info != NULL){
        output_format o_format;
        output_depth o_depth;
        int feature;
        if(strcmp(corlor_info, "Auto")){
            char color[16];
            char depth[16];
            sscanf(corlor_info, "%s-%s", color, depth);
            if (strncmp(color, "RGB", 3) == 0)
                o_format = output_rgb;
            else if (strncmp(color, "YCBCR444", 8) == 0)
                o_format = output_ycbcr444;
            else if (strncmp(color, "YCBCR422", 8) == 0)
                o_format = output_ycbcr422;
            else if (strncmp(color, "YCBCR420", 8) == 0)
                o_format = output_ycbcr420;
            else {
                o_format = output_ycbcr_high_subsampling;
                feature |= COLOR_AUTO;
            }

            if (strstr(corlor_info, "8bit") != NULL)
                o_depth = depth_24bit;
            else if (strstr(corlor_info, "10bit") != NULL)
                o_depth = depth_30bit;
            else
                o_depth = Automatic;
        }else {
            o_format = output_ycbcr_high_subsampling;
            o_depth = Automatic;
            feature |= COLOR_AUTO;
        }
        util->setColor(o_format, o_depth, feature);
    }
    if(isReset > 0){
        reset();
    }
    if(resolution != NULL){
        if(strcmp(resolution, "auto")){
            int hdisplay, vdisplay, vrefresh, hsync_start, hsync_end, htotal, vsync_start, vsync_end, vtotal, vscan, flags, clock;
            sscanf(resolution, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &hdisplay, &vdisplay, &vrefresh, &hsync_start, &hsync_end, &htotal, &vsync_start, &vsync_end, &vtotal, &vscan, &flags, &clock);
            util->setResolution(hdisplay, vdisplay, vrefresh, hsync_start, hsync_end, htotal, vsync_start, vsync_end, vtotal, vscan, flags, clock, 0);
        }else{
            util->setResolution(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, RESOLUTION_AUTO);
        }
    }
    return 0;
}
