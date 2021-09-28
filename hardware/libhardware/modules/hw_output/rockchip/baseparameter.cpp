#include "baseparameter.h"

#include <string>
#include <vector>

using namespace android;
#define UN_USED(x) (void)(x)

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

static bool getBaseParameterInfo(struct file_base_paramer_v1* base_paramer) {
    int file;
    const char *baseparameterfile = GetBaseparameterFile();
    if (baseparameterfile) {
        file = open(baseparameterfile, O_RDWR);
        if (file > 0) {
            unsigned int length = lseek(file, 0L, SEEK_END);

            lseek(file, 0L, SEEK_SET);
            ALOGD("getBaseParameterInfo size=%d", (int)sizeof(*base_paramer));
            if (length >  sizeof(*base_paramer)) {
                read(file, (void*)&(base_paramer->main), sizeof(base_paramer->main));
                lseek(file, BASE_OFFSET, SEEK_SET);
                read(file, (void*)&(base_paramer->aux), sizeof(base_paramer->aux));
                return true;
            }
        }
    }
    return false;
}

static std::string getPropertyString(std::string header, int dpy)
{
    std::string suffix;

    suffix = header;
    if (dpy == HWC_DISPLAY_PRIMARY)
        suffix += "main";
    else
        suffix += "aux";

    ALOGD("suffix=%s", suffix.c_str());
    return suffix;
}

static void saveBcshConfig(struct file_base_paramer_v1 *base_paramer, int dpy){
    std::string propertyStr;
    char bcshProperty[100];

    propertyStr = getPropertyString("persist.vendor.brightness.", dpy);
    property_get(propertyStr.c_str(), bcshProperty, "0");
    if(HWC_DISPLAY_PRIMARY == dpy){
        if (atoi(bcshProperty) > 0)
            base_paramer->main.bcsh.brightness = atoi(bcshProperty);
        else
            base_paramer->main.bcsh.brightness = DEFAULT_BRIGHTNESS;
    } else {
        if (atoi(bcshProperty) > 0)
            base_paramer->aux.bcsh.brightness = atoi(bcshProperty);
        else
            base_paramer->aux.bcsh.brightness = DEFAULT_BRIGHTNESS;
    }
    propertyStr = getPropertyString("persist.vendor.contrast.", dpy);
    property_get(propertyStr.c_str(), bcshProperty, "0");
    if(HWC_DISPLAY_PRIMARY == dpy){
        if (atoi(bcshProperty) > 0)
            base_paramer->main.bcsh.contrast = atoi(bcshProperty);
        else
            base_paramer->main.bcsh.contrast = DEFAULT_BRIGHTNESS;
    } else {
        if (atoi(bcshProperty) > 0)
           base_paramer->aux.bcsh.contrast = atoi(bcshProperty);
        else
           base_paramer->aux.bcsh.contrast = DEFAULT_BRIGHTNESS;
    }
    propertyStr = getPropertyString("persist.vendor.saturation.", dpy);
    property_get(propertyStr.c_str(), bcshProperty, "0");
    if(HWC_DISPLAY_PRIMARY == dpy){
        if (atoi(bcshProperty) > 0)
            base_paramer->main.bcsh.saturation = atoi(bcshProperty);
        else
            base_paramer->main.bcsh.saturation = DEFAULT_BRIGHTNESS;
    } else {
        if (atoi(bcshProperty) > 0)
            base_paramer->aux.bcsh.saturation = atoi(bcshProperty);
        else
            base_paramer->aux.bcsh.saturation = DEFAULT_BRIGHTNESS;
    }
    propertyStr = getPropertyString("persist.vendor.hue.", dpy);
    property_get(propertyStr.c_str(), bcshProperty, "0");
    if(HWC_DISPLAY_PRIMARY == dpy){
        if (atoi(bcshProperty) > 0)
            base_paramer->main.bcsh.hue = atoi(bcshProperty);
        else
            base_paramer->main.bcsh.hue = DEFAULT_BRIGHTNESS;
    } else {
        if (atoi(bcshProperty) > 0)
            base_paramer->aux.bcsh.hue = atoi(bcshProperty);
        else
            base_paramer->aux.bcsh.hue = DEFAULT_BRIGHTNESS;
    }
}

BaseParameterV1::BaseParameterV1()
{
    mBaseParameterInfos = (struct file_base_paramer_v1*)malloc(sizeof(struct file_base_paramer_v1));
    memset(mBaseParameterInfos, 0, sizeof(struct file_base_paramer_v1));
    hasInitial = getBaseParameterInfo(mBaseParameterInfos);
}

BaseParameterV1::~BaseParameterV1()
{
    if (mBaseParameterInfos)
        free(mBaseParameterInfos);
}

int BaseParameterV1::dump_baseparameter(const char *file_path)
{
    UN_USED(file_path);
    return 0;
}

bool BaseParameterV1::have_baseparameter()
{
    bool ret = true;
    const char *baseparameterfile = GetBaseparameterFile();
    if (!baseparameterfile)
        ret = false;
    return ret;
}

int BaseParameterV1::getDisplayId(unsigned int connector_type, unsigned int connector_id)
{
    int dpy = 0;
    DrmConnector* mConnector = nullptr;

    for (auto conn: mConns) {
        mConnector = conn.second;
        if (mConnector->get_type() == connector_type && mConnector->connector_id() == connector_id) {
            dpy = conn.first;
            break;
        }
    }
    return dpy;
}

int BaseParameterV1::get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info)
{
    struct disp_info_v1* info_v1;
    if (!info) 
        return -1;
    
    if (mBaseParameterInfos) {
        if (getDisplayId(connector_type, connector_id) == HWC_DISPLAY_PRIMARY) {
            info_v1 = &mBaseParameterInfos->main;
        } else {
            info_v1 = &mBaseParameterInfos->aux;
        }
        for (int i=0;i<4;i++) {
            info->screen_info[i].type = info_v1->screen_list[i].type;
            info->screen_info[i].id = 0;
            memcpy(&info->screen_info[i].resolution, &info_v1->screen_list[i].resolution, sizeof(struct drm_display_mode));
            info->screen_info[i].format = info_v1->screen_list[i].format;
            info->screen_info[i].depthc = info_v1->screen_list[i].depthc;
            info->screen_info[i].feature = info_v1->screen_list[i].feature;
        }
        memcpy(&info->bcsh_info, &info_v1->bcsh, sizeof(info_v1->bcsh));
        memcpy(&info->overscan_info, &info_v1->scan, sizeof(info_v1->scan));
        info->framebuffer_info.framebuffer_width = info_v1->hwc_info.framebuffer_width;
        info->framebuffer_info.framebuffer_height = info_v1->hwc_info.framebuffer_height;
        info->framebuffer_info.fps = info_v1->hwc_info.fps;
    }
    return 0;
}

int BaseParameterV1::set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info)
{
    if (mBaseParameterInfos && hasInitial) {
        struct disp_info_v1* info_v1;
        if (getDisplayId(connector_type, connector_id) == HWC_DISPLAY_PRIMARY) {
            info_v1 = &mBaseParameterInfos->main;
        } else {
            info_v1 = &mBaseParameterInfos->aux;
        }
        for (int i=0;i<4;i++) {
            info_v1->screen_list[i].type = info->screen_info[i].type;
            memcpy(&info_v1->screen_list[i].resolution, &info->screen_info[i].resolution, sizeof(struct drm_display_mode));
            info_v1->screen_list[i].format = info->screen_info[i].format;
            info_v1->screen_list[i].depthc = info->screen_info[i].depthc;
            info_v1->screen_list[i].feature = info->screen_info[i].feature;
        }
        memcpy(&info_v1->bcsh, &info->bcsh_info, sizeof(info_v1->bcsh));
        memcpy(&info_v1->scan, &info->overscan_info, sizeof(info_v1->scan));
        info_v1->hwc_info.framebuffer_width = info->framebuffer_info.framebuffer_width;
        info_v1->hwc_info.framebuffer_height = info->framebuffer_info.framebuffer_height;
        info_v1->hwc_info.fps = info->framebuffer_info.fps;
    }
    return 0;
}

unsigned short BaseParameterV1::get_brightness(unsigned int connector_type, unsigned int connector_id)
{
    unsigned short brightness = 0;
    bool foudBaseParameter = false;
    std::string propertyStr;
    char mBcshProperty[PROPERTY_VALUE_MAX];
    struct file_base_paramer_v1 base_paramer;

    foudBaseParameter = getBaseParameterInfo(&base_paramer);
    propertyStr = getPropertyString("persist.vendor.brightness.", getDisplayId(connector_type, connector_id));
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        brightness =  atoi(mBcshProperty);
    } else if (foudBaseParameter) {
       brightness = base_paramer.main.bcsh.brightness;
    } else {
        brightness = DEFAULT_BRIGHTNESS;
    }
  return brightness;
}

unsigned short BaseParameterV1::get_contrast(unsigned int connector_type, unsigned int connector_id)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    unsigned short contrast = 0;
    bool foudBaseParameter = false;
    std::string propertyStr;
    char mBcshProperty[PROPERTY_VALUE_MAX];
    struct file_base_paramer_v1 base_paramer;

    foudBaseParameter = getBaseParameterInfo(&base_paramer);
    propertyStr = getPropertyString("persist.vendor.contrast.", getDisplayId(connector_type, connector_id));
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        contrast =  atoi(mBcshProperty);
    } else if (foudBaseParameter) {
        contrast = base_paramer.main.bcsh.contrast;
    } else {
        contrast = DEFAULT_CONTRAST;
    }
    return contrast;
}

unsigned short BaseParameterV1::get_saturation(unsigned int connector_type, unsigned int connector_id)
{
    unsigned short saturation = 0;
    bool foudBaseParameter = false;
    std::string propertyStr;
    char mBcshProperty[PROPERTY_VALUE_MAX];
    struct file_base_paramer_v1 base_paramer;

    foudBaseParameter = getBaseParameterInfo(&base_paramer);
    propertyStr = getPropertyString("persist.vendor.saturation.", getDisplayId(connector_type, connector_id));
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        saturation =  atoi(mBcshProperty);
    } else if (foudBaseParameter) {
        saturation = base_paramer.main.bcsh.saturation;
    } else {
        saturation = DEFAULT_SATURATION;
    }
    return saturation;
}

unsigned short BaseParameterV1::get_hue(unsigned int connector_type, unsigned int connector_id)
{
    struct file_base_paramer_v1 base_paramer;
    char mBcshProperty[PROPERTY_VALUE_MAX];
    bool foudBaseParameter = false;
    unsigned short hue = 0;
    std::string propertyStr;
    
    foudBaseParameter = getBaseParameterInfo(&base_paramer);
    propertyStr = getPropertyString("persist.vendor.hue.", getDisplayId(connector_type, connector_id));
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        hue =  atoi(mBcshProperty);
    } else if (foudBaseParameter) {
        hue = base_paramer.main.bcsh.hue;
    } else {
        hue = DEFAULT_HUE;
    }
    return hue;
}

int BaseParameterV1::set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(value);
    saveConfig();
    return 0;
}

int BaseParameterV1::set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(value);
    saveConfig();
    return 0;
}

int BaseParameterV1::set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(value);
    saveConfig();
    return 0;
}

int BaseParameterV1::set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(value);
    saveConfig();
    return 0;
}

int BaseParameterV1::get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(overscan_info);
    return 0;
}

int BaseParameterV1::set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(overscan_info);
    return 0;
}

int BaseParameterV1::get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(data);
    return 0;
}

int BaseParameterV1::set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data)
{
    if (!hasInitial) {
        ALOGW("baseparamter hasInitial false");
        return -EINVAL; 
    }

    int file;
    int offset = 0;
    const char *filepath = GetBaseparameterFile();

    if (!mBaseParameterInfos || !filepath) {
        ALOGD("hw_output: saveConfig filepath is NULL mBaseParameterInfo=%p*********", mBaseParameterInfos);
        sync();
        return -ENOENT;
    }

    file = open(filepath, O_RDWR);
    if (file < 0) {
        ALOGW("base paramter file can not be opened");
        sync();
        return -EIO;
    }

    struct disp_info_v1* info_v1;
    if (getDisplayId(connector_type, connector_id) == HWC_DISPLAY_PRIMARY) {
        info_v1 = &mBaseParameterInfos->main;
        offset = 0;
    } else {
        info_v1 = &mBaseParameterInfos->aux;
        offset = BASE_OFFSET;
    }
    memcpy(&info_v1->mlutdata, data, sizeof(info_v1->mlutdata));
    lseek(file, offset, SEEK_SET);
    write(file, (char*)(info_v1), sizeof(struct disp_info_v1));
    close(file);
    sync();
    return 0;
}

int BaseParameterV1::get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(data);
    return -1;
}

int BaseParameterV1::set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(data);
    return -1;
}

int BaseParameterV1::set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(index);
    return -1;
}

int BaseParameterV1::set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(screen_info);
    UN_USED(index);
    return -1;
}

int BaseParameterV1::get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info)
{
    UN_USED(connector_type);
    UN_USED(connector_id);
    UN_USED(screen_info);
    UN_USED(index);
    return -1;
}

int BaseParameterV1::get_all_disp_header(struct disp_header *headers)
{
    UN_USED(headers);
    return -1;
}

bool BaseParameterV1::validate() {
    return true;
}

void BaseParameterV1::set_drm_connectors(std::map<int,DrmConnector*> conns)
{
    mConns = conns;
}

void BaseParameterV1::saveConfig()
{
    int file;
    const char *filepath = GetBaseparameterFile();

    if (!mBaseParameterInfos || !filepath) {
        ALOGD("hw_output: saveConfig filepath is NULL mBaseParameterInfo=%p*********", mBaseParameterInfos);
        sync();
        return;
    }
    saveBcshConfig(mBaseParameterInfos, HWC_DISPLAY_PRIMARY);
    saveBcshConfig(mBaseParameterInfos, HWC_DISPLAY_EXTERNAL);
    file = open(filepath, O_RDWR);
    if (file < 0) {
        ALOGW("base paramter file can not be opened");
        sync();
        return;
    }

    lseek(file, 0L, SEEK_SET);
    write(file, (char*)(&mBaseParameterInfos->main), sizeof(struct disp_info_v1));
    lseek(file, BASE_OFFSET, SEEK_SET);
    write(file, (char*)(&mBaseParameterInfos->aux), sizeof(struct disp_info_v1));
    close(file);
    sync();
}