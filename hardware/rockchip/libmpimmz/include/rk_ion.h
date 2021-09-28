#ifndef __RK_ION_H__
#define __RK_ION_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
    申请ion内存
    is_cma 是否申请CMA heap内存，若为false则申请SYSTEM heap内存
    is_cacheable 申请的内存是否支持cache
    fd 存放返回的ion内存的dmabuf fd
    成功 返回0
    失败 返回负值
 */
int ion_alloc(uint32_t len, bool is_cma, bool is_cacheable, int *fd);

/*
    获取ion内存的物理地址
    fd dmabuf's fd
    paddr 存放返回的物理地址，如果非物理连续的内存，则地址被设置为-1
    成功 返回0
    失败 返回负值
 */
int ion_get_phys(int fd, uint64_t *paddr);

#ifdef __cplusplus
}
#endif

#endif