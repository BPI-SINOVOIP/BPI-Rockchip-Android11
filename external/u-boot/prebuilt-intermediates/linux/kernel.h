/* SPDX-License-Identifier: GPL-2.0+ */

#include_next <linux/kernel.h>

/* Workarounds for imx8image.c which uses kernel macros on host side */

#ifndef ALIGN
#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)
#endif

#ifndef __ALIGN_MASK
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#endif
