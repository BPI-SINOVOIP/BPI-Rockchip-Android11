/*
 *
 * Copyright 2010 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
   * File:
   * ppOp.h
   * Description:
   * Global struct definition in VPU module
   * Author:
   *     Wu JunMin
   * Date:
   *    2015-03-31 14:43:40
 */
#ifndef _REG_H_
#define _REG_H_
/***************************************************************************************************
    File:
        reg.h
    Description:
        rkdec register class
    Author:
        Jian Huan
    Date:
        2010-12-8 16:43:41
***************************************************************************************************/
#include "vpu_macro.h"
#include "rkregdrv.h"

class rkdecregister
{

public:
	rkdecregister();
	~rkdecregister();

	void    SetRegisterFile(RK_U32 id, RK_U32 value);
	RK_U32  GetRegisterFile(RK_U32 id);
	
    void  SetRegisterMapAddr(RK_U32 *regMemAddr);


private:
    RK_U32  *regBase;

};
#endif
