/*
 * E Ink - pixel process library
 *
 * Copyright (C) 2020 E Ink Holdings Inc.
 *
 */

#ifndef LIBEINK_H
#define LIBEINK_H

#if defined(__cplusplus)
extern "C" {
#endif

namespace android {

/**
 * wbf:
 *     The input wbf file for reference
 * return: 0 , Success
 *         others , Fail
 */
int EInk_Init(char *wbf);

/**
 * image:
 *     The Y8 image to be processed, the content may be modifed.
 *
 * previous:
 *     The previous Y8 image, which's content won't be modified.
 *
 * width, height:
 *     The dimension of both images.
 *     Assume the stride == width.
 *
 * Note:
 *     Do not apply other image prcossing methods after eink_process().
 *
 */
void eink_process(uint8_t *image, uint8_t *previous,
                  uint32_t width, uint32_t height);
/**
 * color:
 *     The current color image with RGB format, which's content won't be modified.
 * 
 * image:
 *     The current Y8 image to be processed, the content may be modifed.
 *
 * previous:
 *     The previous Y8 image, the content may be modifed.
 *
 * width, height:
 *     The dimension of both images.
 *     Assume the stride == width.
 *
 * Note:
 *     Do not apply other image prcossing methods after eink_process().
 *
 */
void eink_process_color(uint8_t *color, uint8_t *image, uint8_t *previous,
                  uint32_t width, uint32_t height);

/**
 *
 * return:
 *     The build date in decimal number, Ex: 20200225 => Feb 25, 2020.
 */
uint32_t eink_get_version(void);

}
#if defined(__cplusplus)
}
#endif

#endif /* LIBEINK_H */
