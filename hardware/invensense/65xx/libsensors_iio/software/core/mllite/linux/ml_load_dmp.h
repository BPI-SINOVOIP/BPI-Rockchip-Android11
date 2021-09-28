/*
 $License:
    Copyright (C) 2012 InvenSense Corporation, All Rights Reserved.
 $
 */

/*******************************************************************************
 *
 * $Id:$
 *
 ******************************************************************************/

#ifndef INV_LOAD_DMP_H
#define INV_LOAD_DMP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    Includes.
*/
#include "mltypes.h"

/*
    APIs
*/
inv_error_t inv_load_dmp(FILE  *fd);
void read_dmp_img(char *dmp_path, char *out_file);

#ifdef __cplusplus
}
#endif
#endif  /* INV_LOAD_DMP_H */
