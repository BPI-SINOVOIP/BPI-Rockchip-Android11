#include "baseparameter_util.h"
#include "baseparameter_api.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "BaseParameterUtilV2", __VA_ARGS__)

baseparameter_api* mBaseParmApi;
int mConnectorType;
int mConnectorId;

BaseParameterUtilV2::BaseParameterUtilV2() {
    mBaseParmApi = new baseparameter_api();
    mConnectorType = 0;
    mConnectorId = 0;
}

BaseParameterUtilV2::~BaseParameterUtilV2() {
    if (mBaseParmApi)
        delete mBaseParmApi;
}

void BaseParameterUtilV2::print()
{
    LOGD("BaseParameterUtilV2 print %d ", sizeof(disp_info) - sizeof(u32));
    if (mBaseParmApi == nullptr){
        LOGD("mBaseParmApi is null");
        return;
    }
    struct baseparameter_info base_paramer;
    struct baseparameter_info back_paramer;
    mBaseParmApi->get_baseparameter_info(BASE_PARAMETER, &base_paramer);
    mBaseParmApi->get_baseparameter_info(BACKUP_PARAMETER, &back_paramer);
    printf("========== base parameter ==========\n");
    for(int i = 0; i < 8; i++){
        LOGD("index %d type %d id %d\n", i, base_paramer.disp_header[i].connector_type, base_paramer.disp_header[i].connector_id);
        if(base_paramer.disp_header[i].connector_type != 0){
            printf("-connector type: %d connector id: %d offset: %d\n", base_paramer.disp_header[i].connector_type, base_paramer.disp_header[i].connector_id, base_paramer.disp_header[i].offset);
            printf("\tresolution: %dx%d@%c%d-%d-%d-%d-%d-%d-%d-%x clk=%d\n",
                    base_paramer.disp_info[i].screen_info[0].resolution.hdisplay, base_paramer.disp_info[i].screen_info[0].resolution.vdisplay, (base_paramer.disp_info[i].screen_info[0].resolution.flags & DRM_MODE_FLAG_INTERLACE) > 0 ? 'c' : 'p',
            base_paramer.disp_info[i].screen_info[0].resolution.vrefresh, base_paramer.disp_info[i].screen_info[0].resolution.hsync_start, base_paramer.disp_info[i].screen_info[0].resolution.hsync_end, base_paramer.disp_info[i].screen_info[0].resolution.htotal,
                    base_paramer.disp_info[i].screen_info[0].resolution.vsync_start, base_paramer.disp_info[i].screen_info[0].resolution.vsync_end, base_paramer.disp_info[i].screen_info[0].resolution.vtotal, base_paramer.disp_info[i].screen_info[0].resolution.flags, base_paramer.disp_info[i].screen_info[0].resolution.clock);
            printf("\tcorlor: format %d depth %d \n", base_paramer.disp_info[i].screen_info[0].format, base_paramer.disp_info[i].screen_info[0].depthc);
            printf("\tfeature:  0x%x \n", base_paramer.disp_info[i].screen_info[0].feature);
            printf("\tfbinfo: %dx%d@%d\n", base_paramer.disp_info[i].framebuffer_info.framebuffer_width, base_paramer.disp_info[i].framebuffer_info.framebuffer_height, base_paramer.disp_info[i].framebuffer_info.fps);
            printf("\tbcsh: %d %d %d %d\n", base_paramer.disp_info[i].bcsh_info.brightness, base_paramer.disp_info[i].bcsh_info.contrast, base_paramer.disp_info[i].bcsh_info.saturation, base_paramer.disp_info[i].bcsh_info.hue);
            printf("\toverscan: %d %d %d %d \n", base_paramer.disp_info[i].overscan_info.leftscale, base_paramer.disp_info[i].overscan_info.topscale, base_paramer.disp_info[i].overscan_info.rightscale, base_paramer.disp_info[i].overscan_info.bottomscale);
            printf("\tgamma size:%d\n", base_paramer.disp_info[i].gamma_lut_data.size);
            printf("\t3dlut size:%d\n", base_paramer.disp_info[i].cubic_lut_data.size);
        }
    }
    printf("\n========= backup parameter ==========\n");
    for(int i = 0; i < 8; i++){
        LOGD("index %d type %d id %d\n", i, back_paramer.disp_header[i].connector_type, back_paramer.disp_header[i].connector_id);
        if(back_paramer.disp_header[i].connector_type != 0){
            printf("-connector type: %d connector id: %d offset: %d\n", back_paramer.disp_header[i].connector_type, back_paramer.disp_header[i].connector_id, back_paramer.disp_header[i].offset);
            printf("\tresolution: %dx%d@%c%d-%d-%d-%d-%d-%d-%d-%x clk=%d\n",
                    back_paramer.disp_info[i].screen_info[0].resolution.hdisplay, back_paramer.disp_info[i].screen_info[0].resolution.vdisplay, (back_paramer.disp_info[i].screen_info[0].resolution.flags & DRM_MODE_FLAG_INTERLACE) > 0 ? 'c' : 'p',
            back_paramer.disp_info[i].screen_info[0].resolution.vrefresh, back_paramer.disp_info[i].screen_info[0].resolution.hsync_start, back_paramer.disp_info[i].screen_info[0].resolution.hsync_end, back_paramer.disp_info[i].screen_info[0].resolution.htotal,
                    back_paramer.disp_info[i].screen_info[0].resolution.vsync_start, back_paramer.disp_info[i].screen_info[0].resolution.vsync_end, back_paramer.disp_info[i].screen_info[0].resolution.vtotal, back_paramer.disp_info[i].screen_info[0].resolution.flags, back_paramer.disp_info[i].screen_info[0].resolution.clock);
            printf("\tcorlor: format %d depth %d \n", back_paramer.disp_info[i].screen_info[0].format, back_paramer.disp_info[i].screen_info[0].depthc);
            printf("\tfeature:  0x%x \n", back_paramer.disp_info[i].screen_info[0].feature);
            printf("\tfbinfo: %dx%d@%d\n", back_paramer.disp_info[i].framebuffer_info.framebuffer_width, back_paramer.disp_info[i].framebuffer_info.framebuffer_height, back_paramer.disp_info[i].framebuffer_info.fps);
            printf("\tbcsh: %d %d %d %d\n", back_paramer.disp_info[i].bcsh_info.brightness, back_paramer.disp_info[i].bcsh_info.contrast, back_paramer.disp_info[i].bcsh_info.saturation, back_paramer.disp_info[i].bcsh_info.hue);
            printf("\toverscan: %d %d %d %d \n", back_paramer.disp_info[i].overscan_info.leftscale, back_paramer.disp_info[i].overscan_info.topscale, back_paramer.disp_info[i].overscan_info.rightscale, back_paramer.disp_info[i].overscan_info.bottomscale);
            printf("\tgamma size:%d\n", back_paramer.disp_info[i].gamma_lut_data.size);
            printf("\t3dlut size:%d\n", back_paramer.disp_info[i].cubic_lut_data.size);
        }
    }
    printf("====================================\n");
}

bool BaseParameterUtilV2::validate(){
    if (mBaseParmApi == nullptr){
        LOGD("mBaseParmApi is null");
        return false;
    }
    return mBaseParmApi->validate();
}

int BaseParameterUtilV2::dump_baseparameter(const char *file_path){
    if (mBaseParmApi == nullptr){
        LOGD("mBaseParmApi is null");
        return -ENOENT;
    }
    return mBaseParmApi->dump_baseparameter(file_path);
}

void BaseParameterUtilV2::setDisplayId(int dpy){
    return;
}

void BaseParameterUtilV2::setConnectorTypeAndId(int connectorType, int connectorId){
    mConnectorType = connectorType;
    mConnectorId =  connectorId;
}

int BaseParameterUtilV2::setBcsh(int b, int c, int s, int h) {
    int ret = 0;
    ret = mBaseParmApi->set_brightness(mConnectorType, mConnectorId, b);
    if(ret != 0){
        LOGD("set_brightness ret %d ", ret);
        return ret;
    }
    ret = mBaseParmApi->set_contrast(mConnectorType, mConnectorId, c);
    if(ret != 0){
    LOGD("set_contrast ret %d ", ret);
        return ret;
    }
    ret = mBaseParmApi->set_saturation(mConnectorType, mConnectorId, s);
    if(ret != 0){
        LOGD("set_saturation ret %d ", ret);
        return ret;
    }
    ret = mBaseParmApi->set_hue(mConnectorType, mConnectorId, h);
    if(ret != 0){
        LOGD("set_hue ret %d ", ret);
        return ret;
    }
    return 0;
}

int BaseParameterUtilV2::setOverscan(int left, int top, int right, int bottom) {
    struct overscan_info overscan;
    overscan.maxvalue = 100;
    overscan.leftscale = left;
    overscan.rightscale = right;
    overscan.topscale = top;
    overscan.bottomscale = bottom;
    return mBaseParmApi->set_overscan_info(mConnectorType, mConnectorId, &overscan);
}

int BaseParameterUtilV2::setFramebufferInfo(int width, int height, int fps){
    struct framebuffer_info fb_info;
    fb_info.framebuffer_width = width;
    fb_info.framebuffer_height = height;
    fb_info.fps = fps;
    return mBaseParmApi->set_framebuffer_info(mConnectorType, mConnectorId, &fb_info);
}

int BaseParameterUtilV2::setColor(int format, int depth, int feature){
    struct screen_info screen;
    mBaseParmApi->get_screen_info(mConnectorType, mConnectorId, 0 , &screen);
    screen.format = (output_format)format;
    screen.depthc = (output_depth)depth;
    screen.feature = feature;
    return mBaseParmApi->set_screen_info(mConnectorType, mConnectorId, 0 , &screen);
}

int BaseParameterUtilV2::setResolution(int hdisplay, int vdisplay, int vrefresh, int hsync_start, int hsync_end, int htotal, int vsync_start, int vsync_end, int vtotal,int vscan, int flags, int clock, int feature){
    struct screen_info screen;
    mBaseParmApi->get_screen_info(mConnectorType, mConnectorId, 0 , &screen);
    screen.resolution.hdisplay = hdisplay;
    screen.resolution.vdisplay = vdisplay;
    screen.resolution.vrefresh = vrefresh;
    screen.resolution.hsync_start = hsync_start;
    screen.resolution.hsync_end = hsync_end;
    screen.resolution.htotal = htotal;
    screen.resolution.vsync_start = vsync_start;
    screen.resolution.vsync_end = vsync_end;
    screen.resolution.vtotal = vtotal;
    screen.resolution.vscan = vscan;
    screen.resolution.clock = clock;
    screen.resolution.flags = flags;
    screen.feature = feature;
    return mBaseParmApi->set_screen_info(mConnectorType, mConnectorId, 0 , &screen);
}

