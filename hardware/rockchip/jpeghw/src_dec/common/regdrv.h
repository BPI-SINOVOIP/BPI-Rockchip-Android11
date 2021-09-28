/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : 
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: regdrv.h,v $
--  $Revision: 1.19 $
--  $Date: 2010/05/11 09:33:47 $
--
------------------------------------------------------------------------------*/

#ifndef REGDRV_H
#define REGDRV_H

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/

#include "vpu_type.h"

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#define DEC_8170_IRQ_RDY            0x01
#define DEC_8170_IRQ_BUS            0x02
#define DEC_8170_IRQ_BUFFER         0x04
#define DEC_8170_IRQ_ASO            0x08
#define DEC_8170_IRQ_ERROR          0x10
#define DEC_8170_IRQ_SLICE          0x20
#define DEC_8170_IRQ_TIMEOUT        0x40

#define DEC_8190_IRQ_RDY            DEC_8170_IRQ_RDY
#define DEC_8190_IRQ_BUS            DEC_8170_IRQ_BUS
#define DEC_8190_IRQ_BUFFER         DEC_8170_IRQ_BUFFER
#define DEC_8190_IRQ_ASO            DEC_8170_IRQ_ASO
#define DEC_8190_IRQ_ERROR          DEC_8170_IRQ_ERROR
#define DEC_8190_IRQ_SLICE          DEC_8170_IRQ_SLICE
#define DEC_8190_IRQ_TIMEOUT        DEC_8170_IRQ_TIMEOUT

typedef enum
{
/* include script-generated part */
#include "8170enum.h"
    HWIF_DEC_IRQ_STAT,
    HWIF_PP_IRQ_STAT,
    HWIF_LAST_REG,

    /* aliases */
    HWIF_MPEG4_DC_BASE = HWIF_I4X4_OR_DC_BASE,
    HWIF_INTRA_4X4_BASE = HWIF_I4X4_OR_DC_BASE,
    HWIF_RESV2HWGOLDEN_BASE = HWIF_REFER4_BASE,
    HWIF_RESV2HWPART1_BASE = HWIF_REFER13_BASE,
    HWIF_RESV2HWPART2_BASE = HWIF_RLC_VLC_BASE,
    HWIF_RESV2HWPROBTBL_BASE = HWIF_QTABLE_BASE,
    /* progressive JPEG */
    HWIF_PJPEG_COEFF_BUF = HWIF_DIR_MV_BASE,

    /* MVC */
    HWIF_MVC_E = HWIF_RESV1_E,
    HWIF_INTER_VIEW_BASE = HWIF_REFER15_BASE,

} hwIfName_e;

/*------------------------------------------------------------------------------
    Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/

void SetDecRegister(RK_U32 * regBase, RK_U32 id, RK_U32 value);
RK_U32 GetDecRegister(const RK_U32 * regBase, RK_U32 id);

#endif /* #ifndef REGDRV_H */
