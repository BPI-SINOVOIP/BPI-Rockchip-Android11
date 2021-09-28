#ifndef BASE_PARAMETER_INTERFACE_H
#define BASE_PARAMETER_INTERFACE_H

#include "baseparameter_api.h"

#include <map>

#include "hw_types.h"
#include "rkdisplay/drmresources.h"
#include "rkdisplay/drmmode.h"
#include "rkdisplay/drmconnector.h"
#include "rockchip/baseparameter.h"

using namespace android;
class BaseParameter {
public:
    BaseParameter(){};
    virtual ~BaseParameter(){}
    virtual bool have_baseparameter() = 0;
    virtual int dump_baseparameter(const char *file_path) = 0;
    virtual int get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) = 0;
    virtual int set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) = 0;
    virtual int get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info) = 0;
    virtual int set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info) = 0;
    virtual unsigned short get_brightness(unsigned int connector_type, unsigned int connector_id) = 0;
    virtual unsigned short get_contrast(unsigned int connector_type, unsigned int connector_id) = 0;
    virtual unsigned short get_saturation(unsigned int connector_type, unsigned int connector_id) = 0;
    virtual unsigned short get_hue(unsigned int connector_type, unsigned int connector_id) = 0;
    virtual int set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value) = 0;
    virtual int set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value) = 0;
    virtual int set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value) = 0;
    virtual int set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value) = 0;
    virtual int get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info) = 0;
    virtual int set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info) = 0;
    virtual int get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) = 0;
    virtual int set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) = 0;
    virtual int get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) = 0;
    virtual int set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) = 0;
    virtual int set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id) = 0;
    virtual bool validate() = 0;
    virtual int get_all_disp_header(struct disp_header *headers) = 0;
    virtual void set_drm_connectors(std::map<int,DrmConnector*> conns) = 0;
    virtual void saveConfig() = 0;
};

class BaseParameterV1 : public BaseParameter {
public:
    BaseParameterV1();
    ~BaseParameterV1();
    bool have_baseparameter();
    int dump_baseparameter(const char *file_path);
    int get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    int set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    int get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    int set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    unsigned short get_brightness(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_contrast(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_saturation(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_hue(unsigned int connector_type, unsigned int connector_id);
    int set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    int set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    int get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    int set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    int get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    int set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    int set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id);
    bool validate();
    int get_all_disp_header(struct disp_header *headers);
    void set_drm_connectors(std::map<int,DrmConnector*> conns);
    void saveConfig();
private:
    int getDisplayId(unsigned int connector_type, unsigned int connector_id);
    struct file_base_paramer_v1* mBaseParameterInfos;
    std::map<int,DrmConnector*> mConns;
    bool hasInitial;
};

class BaseParameterV2 : public BaseParameter {
public:
    BaseParameterV2();
    virtual ~BaseParameterV2();
    virtual bool have_baseparameter();
    virtual int dump_baseparameter(const char *file_path);
    virtual int get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    virtual int set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    virtual int get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    virtual int set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    virtual unsigned short get_brightness(unsigned int connector_type, unsigned int connector_id);
    virtual unsigned short get_contrast(unsigned int connector_type, unsigned int connector_id);
    virtual unsigned short get_saturation(unsigned int connector_type, unsigned int connector_id);
    virtual unsigned short get_hue(unsigned int connector_type, unsigned int connector_id);
    virtual int set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    virtual int set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    virtual int set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    virtual int set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    virtual int get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    virtual int set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    virtual int get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    virtual int set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    virtual int get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    virtual int set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    virtual int set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id);
    virtual bool validate();
    virtual int get_all_disp_header(struct disp_header *headers);
    virtual void set_drm_connectors(std::map<int,DrmConnector*> conns){(void)conns;}
    virtual void saveConfig() {}
private:
    baseparameter_api* mBaseParmApi;
};
#endif
