/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef _MAC_AX_INIT_8852B_H_
#define _MAC_AX_INIT_8852B_H_

#include "../../type.h"

#ifdef CONFIG_NEW_HALMAC_INTERFACE
struct mac_ax_adapter *get_mac_8852b_adapter(enum mac_ax_intf intf,
					     u8 cv, void *phl_adapter,
					     void *drv_adapter,
					     struct mac_ax_pltfm_cb *pltfm_cb)
#else
struct mac_ax_adapter *get_mac_8852b_adapter(enum mac_ax_intf intf,
					     u8 cv, void *drv_adapter,
					     struct mac_ax_pltfm_cb *pltfm_cb);
#endif
#endif
