/*
 * Copyright(C) 2010 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS
 * SOFTWARE, YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FORM ROCKCHIP
 * IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL
 * WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY,
 * ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license
 *   (a) to install, save and use the Software;
 *   (b) to * copy and distribute the Software in binary code format only
 * Except as expressively authorized by Rockchip in writing, you may NOT:
 *   (a) distribute the Software in source code;
 *   (b) distribute on a standalone basis but you may distribute the Software in
 *   conjunction with platforms incorporating Rockchip integrated circuits;
 *   (c) modify the Software in whole or part;
 *   (d) decompile, reverse-engineer, dissemble, or attempt to derive any source
 *   code from the Software;
 *   (e) remove or obscure any copyright, patent, or trademark statement or
 *   notices contained in the Software
 */

#include <string.h>
#include <ion/IonAlloc.h>
#include <errno.h>
#include <cutils/log.h>

#undef LOGE
#define LOGE    ALOGE

#include "ion_priv_vpu.h"
using android::IonAlloc;

int IonAlloc::alloc(unsigned long size, enum _ion_heap_type type, ion_buffer_t *data)
{
        int err = 0;
    
        ion_buffer_t *m_data = NULL;
        err = mIon->alloc(mIon, size, type, &m_data);
        if(err)
                return err;
        memcpy((void *)data, (void *)m_data, sizeof(ion_buffer_t));
        mIonHandleMap.add(data->virt, m_data);

        return err;
}
int IonAlloc::free(ion_buffer_t data)
{
        int err = 0;
        ion_buffer_t *m_data = (ion_buffer_t *)mIonHandleMap.valueFor(data.virt);

        err = mIon->free(mIon, m_data);
        if(err)
                return err;

        mIonHandleMap.removeItem(data.virt);
        return err;
}
int IonAlloc::share(ion_buffer_t data, int *share_fd)
{
        int err = 0;
        ion_buffer_t *m_data = (ion_buffer_t *)mIonHandleMap.valueFor(data.virt);
    
        err = mIon->share(mIon, m_data, share_fd);

        return err;
}
int IonAlloc::map(int share_fd, ion_buffer_t *data)
{
        int err = 0;

        ion_buffer_t *m_data = NULL;
        err= mIon->map(mIon, share_fd, &m_data);
        if(err)
                return err;
        memcpy((void *)data, (void *)m_data, sizeof(ion_buffer_t));
        mIonHandleMap.add(data->virt, m_data);

        return err;
}
int IonAlloc::unmap(ion_buffer_t data)
{
        int err = 0;
        ion_buffer_t *m_data = (ion_buffer_t *)mIonHandleMap.valueFor(data.virt);

        err = mIon->unmap(mIon, m_data);
        if(err)
                return err;

        mIonHandleMap.removeItem(data.virt);

        return err;
}

int IonAlloc::cache_op(ion_buffer_t data, enum cache_op_type type)
{
        int err = 0;
        ion_buffer_t *m_data = (ion_buffer_t *)mIonHandleMap.valueFor(data.virt);
    
        err= mIon->cache_op(mIon, m_data, type);

        return err;
}

void IonAlloc::set_id(enum ion_module_id id)
{
        private_device_t *dev = (private_device_t *)mIon;

        dev->id = id;

        return;
}
int IonAlloc::perform(int operation, ... )
{
        int err = 0;
        va_list args;

        va_start(args, operation);
        switch(operation) {

        case ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED: 
        case ION_MODULE_PERFORM_QUERY_BUFCOUNT: {
                struct ion_client_info info;   
                unsigned long i, _count = 0, size = 0, *p; 

                if(operation == ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED)
                        size = va_arg(args, unsigned long); 
                p = (unsigned long *)va_arg(args, void *);

                memset(&info, 0, sizeof(struct ion_client_info));
	        err = ioctl(((private_device_t *)mIon)->ionfd, ION_CUSTOM_GET_CLIENT_INFO, &info);
                if(err) {
		        LOGE("%s: ION_GET_CLIENT failed with error - %s",
                                __FUNCTION__, strerror(errno));
                        err = -errno;
	        } else {
                        if(operation == ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED)
                                *p = info.total_size;
                        else {
                                for(i = 0; i < info.count; i++) {
                                        if(info.buf[i].size == size)
                                                _count++;
                                }
                                *p = _count;
                        }
                        err = 0;
                }
                break;
        }
        
        case ION_MODULE_PERFORM_QUERY_HEAP_SIZE: 
        case ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED: 
        {
                struct ion_heap_info info;
                
                unsigned long *p = (unsigned long *)va_arg(args, void *); 

                info.id = ION_NOR_HEAP_ID;

	        err = ioctl(((private_device_t *)mIon)->ionfd, ION_CUSTOM_GET_HEAP_INFO, &info);
                if(err) {
		        LOGE("%s: ION_GET_CLIENT failed with error - %s",
                                __FUNCTION__, strerror(errno));
                        err = -errno;
	        } else {
                        if(operation == ION_MODULE_PERFORM_QUERY_HEAP_SIZE)
                                *p = info.total_size;
                        else
                                *p = info.allocated_size;
                        err = 0;
                }
                break;
        }

        default:
                LOGE("%s: operation(0x%x) not support", __FUNCTION__, operation);
                err = -EINVAL;
                break;
 
        }

        va_end(args);
        return err;
}

