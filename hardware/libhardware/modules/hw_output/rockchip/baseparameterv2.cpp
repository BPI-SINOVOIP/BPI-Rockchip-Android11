#include "baseparameter.h"

BaseParameterV2::BaseParameterV2() {
    mBaseParmApi = new baseparameter_api();
}

BaseParameterV2::~BaseParameterV2() {
    if (mBaseParmApi)
        delete mBaseParmApi;
}

bool BaseParameterV2::have_baseparameter()
{
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->have_baseparameter();
}

int BaseParameterV2::dump_baseparameter(const char *file_path) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->dump_baseparameter(file_path);
}


int BaseParameterV2::get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_disp_info(connector_type, connector_id, info);
}

int BaseParameterV2::set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_disp_info(connector_type, connector_id, info);
}

int BaseParameterV2::get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info)
{
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_screen_info(connector_type, connector_id, index, screen_info);    
}

int BaseParameterV2::set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_screen_info(connector_type, connector_id, index, screen_info);    
}

unsigned short BaseParameterV2::get_brightness(unsigned int connector_type, unsigned int connector_id) {
    if (mBaseParmApi == nullptr)
        return 50;
    return mBaseParmApi->get_brightness(connector_type, connector_id);        
}

unsigned short BaseParameterV2::get_contrast(unsigned int connector_type, unsigned int connector_id) {
    if (mBaseParmApi == nullptr)
        return 50;
    return mBaseParmApi->get_contrast(connector_type, connector_id);     
}

unsigned short BaseParameterV2::get_saturation(unsigned int connector_type, unsigned int connector_id) {
    if (mBaseParmApi == nullptr)
        return 50;
    return mBaseParmApi->get_saturation(connector_type, connector_id);    
}

unsigned short BaseParameterV2::get_hue(unsigned int connector_type, unsigned int connector_id) {
    if (mBaseParmApi == nullptr)
        return 50;
    return mBaseParmApi->get_hue(connector_type, connector_id);    
}

int BaseParameterV2::set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_brightness(connector_type, connector_id, value);    
}

int BaseParameterV2::set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_contrast(connector_type, connector_id, value);    
}

int BaseParameterV2::set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_saturation(connector_type, connector_id, value);    
}

int BaseParameterV2::set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_hue(connector_type, connector_id, value);    
}

int BaseParameterV2::get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_overscan_info(connector_type, connector_id, overscan_info); 
}

int BaseParameterV2::set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_overscan_info(connector_type, connector_id, overscan_info); 
}

int BaseParameterV2::get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_gamma_lut_data(connector_type, connector_id, data); 
}

int BaseParameterV2::set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_gamma_lut_data(connector_type, connector_id, data); 
}

int BaseParameterV2::get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_cubic_lut_data(connector_type, connector_id, data); 
}

int BaseParameterV2::set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_cubic_lut_data(connector_type, connector_id, data); 
}

int BaseParameterV2::set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->set_disp_header(index, connector_type, connector_id);     
}

bool BaseParameterV2::validate() {
    if (mBaseParmApi == nullptr)
        return false;
    return mBaseParmApi->validate();     
}
int BaseParameterV2::get_all_disp_header(struct disp_header *headers) {
    if (mBaseParmApi == nullptr)
        return -1;
    return mBaseParmApi->get_all_disp_header(headers);
}
