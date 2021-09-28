#ifndef _GRAPHICPOLICY_H_
#define _GRAPHICPOLICY_H_


/***********************************************************************************************
*函数名: graphic_policy
*函数功能描述: 判断传入的问题id是否需要workaround。并且自动获取并判断该问题的进程名是否符合条件。
*函数参数  : 问题id，如：2001、3002
*函数返回值: 0(disable); 1(enable);
*作者: Zhixiong Lin
*函数创建日期: 2020/09/05
***********************************************************************************************/
int graphic_policy(int obj_bug_id);


#define GPID_SF_NODRAW  0001
#define GPID_SF_MIRROR  0002
#define GPID_SF_KEYSTONE 0003
#define GPID_HWC_4K_SKIPLINE  1001
#define GPID_HWC_BOOTANIMATION  1002
#define GPID_GRALLOC_CTS1  2001
#define GPID_GRALLOC_CTS2  2002
#define GPID_GUI_FPSCTL  3001
#define GPID_HWUI_CLOSE  4001
#define GPID_MALI_FRAMESKIP  5001
#define GPID_MALI_KODI  5002
#define GPID_PVR_CTS1  6001
#define GPID_PVR_CTS2  6002




#endif //_GRAPHICPOLICY_H_

