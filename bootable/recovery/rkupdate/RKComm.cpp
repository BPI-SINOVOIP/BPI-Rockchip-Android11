/*
 * Copyright 2019 Rockchip Electronics Co., Ltd
 * Dayao Ji <jdy@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include "rkupdate/RKComm.h"
#include "rkupdate/RKLog.h"
#include "rkupdate/RKAndroidDevice.h"
#include <cinttypes>


CRKComm::CRKComm(CRKLog *pLog)
{
	m_log = pLog;
	m_bEmmc = false;
	m_hDev = m_hLbaDev = -1;
}
CRKComm::~CRKComm()
{
}

CRKUsbComm::CRKUsbComm(CRKLog *pLog):CRKComm(pLog)
{
	//char bootmode[100];
	//property_get("ro.bootmode", bootmode, "unknown");
	//if(!strcmp(bootmode, "emmc"))
	//	m_bEmmc = true;
	//else
	//	m_bEmmc = false;
    char *emmc_point = getenv(EMMC_POINT_NAME);
    pLog->Record(_T("INFO:emmc_point-->is %s"),emmc_point);
    m_hLbaDev = open(emmc_point, O_RDWR|O_SYNC,0);
    if (m_hLbaDev<0){
        if (pLog)
            pLog->Record(_T("INFO:is nand devices..."));
        m_bEmmc = false;
    }else{
        if (pLog)
            pLog->Record(_T("INFO:is emmc devices..."));
        m_bEmmc = true;
        long long  filelen= lseek(m_hLbaDev,0L,SEEK_END);
	lseek(m_hLbaDev,0L,SEEK_SET);
	printf("flashSize is %lld\n",filelen);
        m_FlashSize = filelen;
        close(m_hLbaDev);
    }

	if (m_bEmmc)
	{
		if (pLog)
			pLog->Record(_T("INFO:CRKUsbComm-->is emmc."));
		m_hDev = open(EMMC_DRIVER_DEV_VENDOR,O_RDWR,0);
		if (m_hDev<0)
		{
			if (pLog){
				pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%s"),EMMC_DRIVER_DEV_VENDOR, strerror(errno));
			    pLog->Record(_T("ERROR:CRKUsbComm-->try to read %s."),EMMC_DRIVER_DEV);
            }

		    m_hDev = open(EMMC_DRIVER_DEV,O_RDWR,0);
            if(m_hDev<0){
			    if (pLog){
				    pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%s"),EMMC_DRIVER_DEV, strerror(errno));
				    pLog->Record(_T("ERROR:CRKUsbComm-->please to check drmboot.ko."));
                }
            }
            else
            {
                if (pLog)
                    pLog->Record(_T("INFO:CRKUsbComm EMMC_DRIVER_DEV -->%s=%d"),EMMC_DRIVER_DEV,m_hDev);
            }

		}
		else
		{
			if (pLog)
				pLog->Record(_T("INFO:CRKUsbComm EMMC_DRIVER_DEV_VENDOR-->%s=%d"),EMMC_DRIVER_DEV_VENDOR,m_hDev);
		}
        //get EMMC_DRIVER_DEV_LBA from
        m_hLbaDev= open(emmc_point, O_RDWR|O_SYNC,0);
		if (m_hLbaDev<0)
		{
			if (pLog)
				pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%d"),emmc_point,errno);
		}
		else
		{
			if (pLog)
				pLog->Record(_T("INFO:CRKUsbComm emmc_point-->%s=%d"),emmc_point,m_hLbaDev);
		}

	}
	else
	{
		if (pLog)
			pLog->Record(_T("INFO:CRKUsbComm-->is nand."));
		m_hDev = open(NAND_DRIVER_DEV_VENDOR,O_RDWR,0);
		if (m_hDev<0)
		{
			if (pLog){
				pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%d"),NAND_DRIVER_DEV_VENDOR,strerror(errno));
				pLog->Record(_T("ERROR:CRKUsbComm-->try to read from %s."),NAND_DRIVER_DEV_VENDOR);
            }
		    m_hDev = open(NAND_DRIVER_DEV,O_RDWR,0);
			if (pLog){
				pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%d"),NAND_DRIVER_DEV,strerror(errno));
            }else{
                if (pLog)
                    pLog->Record(_T("INFO:CRKUsbComm-->%s=%d"),NAND_DRIVER_DEV,m_hDev);
            }

		}
		else
		{
			if (pLog)
				pLog->Record(_T("INFO:CRKUsbComm-->%s=%d"),NAND_DRIVER_DEV_VENDOR,m_hDev);
		}

//		m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
//		if (m_hLbaDev<0)
//		{
//			if (pLog)
//				pLog->Record(_T("ERROR:CRKUsbComm-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
//		}
//		else
//		{
//			//CtrlNandLbaRead();
//			CtrlNandLbaWrite();
//			if (pLog)
//				pLog->Record(_T("INFO:CRKUsbComm-->%s=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
//		}
	}

}
void CRKUsbComm::RKU_ReopenLBAHandle()
{
	if (m_bEmmc)
		return;
	if (m_hLbaDev>0)
	{
		close(m_hLbaDev);
		m_hLbaDev = -1;
	}
//	if (m_bEmmc)
//	{
//		m_hLbaDev= open(EMMC_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
//		if (m_hLbaDev<0)
//		{
//			if (m_log)
//				m_log->Record(_T("ERROR:RKU_ReopenLBAHandle-->open %s failed,err=%d"),EMMC_DRIVER_DEV_LBA,errno);
//		}
//		else
//		{
//			if (m_log)
//				m_log->Record(_T("INFO:RKU_ReopenLBAHandle-->%s=%d"),EMMC_DRIVER_DEV_LBA,m_hLbaDev);
//		}

//	}
//	else
//	{
		m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
		if (m_hLbaDev<0)
		{
			if (m_log)
				m_log->Record(_T("ERROR:RKU_ReopenLBAHandle-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
		}
		else
		{
			if (m_log)
				m_log->Record(_T("INFO:RKU_ReopenLBAHandle-->%s=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
		}
//	}

}
int CRKUsbComm::RKU_ShowNandLBADevice()
{
	if (m_bEmmc)
		return ERR_SUCCESS;
	BYTE blockState[64];
	memset(blockState,0,64);
	int iRet;
	iRet = RKU_TestBadBlock(0,0,MAX_TEST_BLOCKS,blockState);
	if (iRet!=ERR_SUCCESS)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_ShowNandLBADevice-->RKU_TestBadBlock failed,ret=%d"),iRet);
	}
	return iRet;
}


CRKUsbComm::~CRKUsbComm()
{
	if (m_hDev>0)
		close(m_hDev);
	if (m_hLbaDev>0)
	{
//		if (!m_bEmmc)
//		{
//			CtrlNandLbaRead(false);
//			CtrlNandLbaWrite(false);
//		}
		close(m_hLbaDev);
	}
}

int CRKUsbComm::RKU_EraseBlock(BYTE ucFlashCS,DWORD dwPos,DWORD dwCount,BYTE ucEraseType)
{(void)ucFlashCS; (void)dwPos; (void)dwCount; (void)ucEraseType;
	return ERR_SUCCESS;
}

int CRKUsbComm::RKU_EraseBlock_discard(unsigned int dwPos, unsigned int part_size)
{
	u64 range[2];
	int ret;
	range[0] = (off64_t)dwPos * 512;
	range[1] = (off64_t)part_size * 512;

	printf("dwPos = %" PRIu64 ", len=%" PRIu64 " \n", range[0], range[1]);

	if (m_hLbaDev < 0)
		return ERR_DEVICE_OPEN_FAILED;

	ret = ioctl(m_hLbaDev, BLKDISCARD, &range);
	if (ret < 0) {
		printf("Discard failed\n");
		return 1;
	} else {
		printf("Wipe used discard success\n");
		return 0;
	}
}

int CRKUsbComm::RKU_ReadChipInfo(BYTE* lpBuffer)
{(void)lpBuffer;
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_ReadFlashID(BYTE* lpBuffer)
{(void)lpBuffer;
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_ReadFlashInfo(BYTE* lpBuffer,UINT *puiRead)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	ret = ioctl(m_hDev,GET_FLASH_INFO_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_ReadFlashInfo ioctl failed,err=%d"),errno);
		return ERR_FAILED;
	}
	*puiRead = 11;
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_ReadLBA(DWORD dwPos,DWORD dwCount,BYTE* lpBuffer,BYTE bySubCode)
{
    long long ret;
    long long dwPosBuf;
	(void)bySubCode;
	if (m_hLbaDev<0)
	{
		if (!m_bEmmc)
		{
			m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
			if (m_hLbaDev<0)
			{
				if (m_log)
					m_log->Record(_T("ERROR:RKU_ReadLBA-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
				return ERR_DEVICE_OPEN_FAILED;
			}
			else
			{
				if (m_log)
					m_log->Record(_T("INFO:RKU_ReadLBA-->open %s ok,handle=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
			}
		}
		else
			return ERR_DEVICE_OPEN_FAILED;
	}
	if (m_bEmmc && !CRKAndroidDevice::bGptFlag)
		dwPos += 8192;

    dwPosBuf = dwPos;
	ret = lseek64(m_hLbaDev,(off64_t)dwPosBuf*512,SEEK_SET);
	if (ret<0)
	{
		if (m_log){
			m_log->Record(_T("ERROR:RKU_ReadLBA seek failed,err=%d,ret=%lld."),errno,ret);
            m_log->Record(_T("the dwPosBuf = dwPosBuf*512,dwPosBuf:%lld!"), dwPosBuf*512);
        }
		return ERR_FAILED;
	}
	ret = read(m_hLbaDev,lpBuffer,dwCount*512);
	if (ret!=dwCount*512)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_ReadLBA read failed,err=%d"),errno);
		return ERR_FAILED;
	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_ReadSector(DWORD dwPos,DWORD dwCount,BYTE* lpBuffer)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	DWORD *pOffsetSec=(DWORD *)(lpBuffer);
	DWORD *pCountSec=(DWORD *)(lpBuffer+4);
	*pOffsetSec = dwPos;
	*pCountSec = dwCount;
	ret = ioctl(m_hDev,READ_SECTOR_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_ReadSector failed,err=%d"),errno);
		return ERR_FAILED;
	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_ResetDevice(BYTE bySubCode)
{
	(void)bySubCode;
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_TestBadBlock(BYTE ucFlashCS,DWORD dwPos,DWORD dwCount,BYTE* lpBuffer)
{
	int ret;
	(void)ucFlashCS; (void)dwPos; (void)dwCount;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	ret = ioctl(m_hDev,GET_BAD_BLOCK_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_TestBadBlock failed,err=%d"),errno);
		return ERR_FAILED;
	}
	if (m_log)
	{
		string strOutput;
		m_log->PrintBuffer(strOutput,lpBuffer,64);
		m_log->Record(_T("INFO:BadBlockState:\r\n%s"),strOutput.c_str());
	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_TestDeviceReady(DWORD *dwTotal,DWORD *dwCurrent,BYTE bySubCode)
{
	(void)dwTotal; (void)dwCurrent; (void)bySubCode;
	return ERR_DEVICE_READY;
}
int CRKUsbComm::RKU_WriteLBALoader(DWORD dwPos,DWORD dwCount,BYTE* lpBuffer,BYTE bySubCode)
{
	long long ret;
    long long dwPosBuf;
	(void)bySubCode;
	if (m_hLbaDev<0)
	{
		printf("---------m_hLbaDev<0\n");
		if (!m_bEmmc)
		{
			printf("---------nand\n");
			m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
			if (m_hLbaDev<0)
			{
				if (m_log)
					m_log->Record(_T("ERROR:RKU_WriteLBA-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
				return ERR_DEVICE_OPEN_FAILED;
			}
			else
			{
				if (m_log)
					m_log->Record(_T("INFO:RKU_WriteLBA-->open %s ok,handle=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
			}
		}
		else
			return ERR_DEVICE_OPEN_FAILED;
	}
	/*
	if (m_bEmmc && !CRKAndroidDevice::bGptFlag)
	{
		m_log->Record(_T("add----8192"));
		dwPos += 8192;
	}*/
	//如果是非GPT需要加8192,但是单独升级loader不需要加8192(4M)
    /*
	if (m_bEmmc)
		if(!CRKAndroidDevice::bGptFlag && !CRKAndroidDevice::bOnlyLoader)
		{
			m_log->Record(_T("aaaaadd----8192"));
			dwPos += 8192;
		}*/
    dwPosBuf = dwPos;
	ret = lseek64(m_hLbaDev,(off64_t)dwPosBuf*512,SEEK_SET);
	if (ret<0)
	{
		if (m_log){
			m_log->Record(_T("ERROR:RKU_WriteLBA seek failed,err=%d,ret:%lld"),errno, ret);
            m_log->Record(_T("ERROR:the dwPosBuf = dwPosBuf*512,dwPosBuf:%lld!"), dwPosBuf*512);
        }
		m_log->Record(_T("ERROR:RKU_WriteLBA seek failed,err=%d,ret:%lld"),errno, ret);
        m_log->Record(_T("ERROR:the dwPosBuf = dwPosBuf*512,dwPosBuf:%lld!"), dwPosBuf*512);
		return ERR_FAILED;

	}
	ret = write(m_hLbaDev,lpBuffer,dwCount*512);
	if (ret!=dwCount*512)
	{
		sleep(1);
		if (m_log)
			m_log->Record(_T("ERROR:RKU_WriteLBA write failed,err=%d"),errno);
		return ERR_FAILED;
	}
//	ret = fsync(m_hLbaDev);
//	if (ret!=0)
//	{
//		if (m_log)
//			m_log->Record(_T("ERROR:RKU_WriteLBA fsync failed,err=%d"),errno);
//	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_WriteLBA(DWORD dwPos,DWORD dwCount,BYTE* lpBuffer,BYTE bySubCode)
{
    long long ret;
    long long dwPosBuf;
	(void)bySubCode;
        if (m_hLbaDev<0)
        {
                printf("---------m_hLbaDev<0\n");
                if (!m_bEmmc)
                {
                        printf("---------nand\n");
                        m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
                        if (m_hLbaDev<0)
                        {
                                if (m_log)
                                        m_log->Record(_T("ERROR:RKU_WriteLBA-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
                                return ERR_DEVICE_OPEN_FAILED;
                        }
                        else
                        {
                                if (m_log)
                                        m_log->Record(_T("INFO:RKU_WriteLBA-->open %s ok,handle=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
                        }
                }
                else
                        return ERR_DEVICE_OPEN_FAILED;
        }
        if (m_bEmmc && !CRKAndroidDevice::bGptFlag)
        {
                m_log->Record(_T("add----8192"));
                dwPos += 8192;
        }
        //如果是非GPT需要加8192,但是单独升级loader不需要加8192(4M)
    /*
        if (m_bEmmc)
                if(!CRKAndroidDevice::bGptFlag && !CRKAndroidDevice::bOnlyLoader)
                {
                        m_log->Record(_T("aaaaadd----8192"));
                        dwPos += 8192;
                }*/
    dwPosBuf = dwPos;
        ret = lseek64(m_hLbaDev,(off64_t)dwPosBuf*512,SEEK_SET);
        if (ret<0)
        {
                if (m_log){
                        m_log->Record(_T("ERROR:RKU_WriteLBA seek failed,err=%d,ret:%lld"),errno, ret);
            m_log->Record(_T("ERROR:the dwPosBuf = dwPosBuf*512,dwPosBuf:%lld!"), dwPosBuf*512);
        }
                m_log->Record(_T("ERROR:RKU_WriteLBA seek failed,err=%d,ret:%lld"),errno, ret);
        m_log->Record(_T("ERROR:the dwPosBuf = dwPosBuf*512,dwPosBuf:%lld!"), dwPosBuf*512);
                return ERR_FAILED;

        }
        ret = write(m_hLbaDev,lpBuffer,dwCount*512);
        if (ret!=dwCount*512)
        {
                sleep(1);
                if (m_log)
                        m_log->Record(_T("ERROR:RKU_WriteLBA write failed,err=%d"),errno);
                return ERR_FAILED;
        }
//      ret = fsync(m_hLbaDev);
//      if (ret!=0)
//      {
//              if (m_log)
//                      m_log->Record(_T("ERROR:RKU_WriteLBA fsync failed,err=%d"),errno);
//      }
        return ERR_SUCCESS;
}
int CRKUsbComm::RKU_WriteSector(DWORD dwPos,DWORD dwCount,BYTE* lpBuffer)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	DWORD *pOffset=(DWORD *)(lpBuffer);
	DWORD *pCount=(DWORD *)(lpBuffer+4);
	*pOffset = dwPos;
	*pCount = dwCount;
	ret = ioctl(m_hDev,WRITE_SECTOR_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_WriteSector failed,err=%d"),errno);
		return ERR_FAILED;
	}
	return ERR_SUCCESS;
}

int CRKUsbComm::RKU_EndWriteSector(BYTE* lpBuffer)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	ret = ioctl(m_hDev,END_WRITE_SECTOR_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_EndWriteSector failed,err=%d"),errno);
		return ERR_FAILED;
	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_GetLockFlag(BYTE* lpBuffer)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	ret = ioctl(m_hDev,GET_LOCK_FLAG_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_GetLockFlag failed,err=%d"),errno);
		return ERR_FAILED;
	}
	DWORD *pFlag=(DWORD *)lpBuffer;
	if (m_log)
	{
		m_log->Record(_T("INFO:LockFlag:0x%08x"),*pFlag);
	}
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_GetPublicKey(BYTE* lpBuffer)
{
	int ret;
	if (m_hDev<0)
		return ERR_DEVICE_OPEN_FAILED;
	ret = ioctl(m_hDev,GET_PUBLIC_KEY_IO,lpBuffer);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:RKU_GetPublicKey failed,err=%d"),errno);
		return ERR_FAILED;
	}
	return ERR_SUCCESS;
}
bool CRKUsbComm::CtrlNandLbaWrite(bool bEnable)
{
	int ret;
	if (m_bEmmc)
		return false;
	if (m_hLbaDev<0)
		return false;
	if (bEnable)
		ret = ioctl(m_hLbaDev,ENABLE_NAND_LBA_WRITE_IO);
	else
		ret = ioctl(m_hLbaDev,DISABLE_NAND_LBA_WRITE_IO);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:CtrlNandLbaWrite failed,enable=%d,err=%d"),bEnable,errno);
		return false;
	}

	return true;
}
bool CRKUsbComm::CtrlNandLbaRead(bool bEnable)
{
	int ret;
	if (m_bEmmc)
		return false;
	if (m_hLbaDev<0)
		return false;
	if (bEnable)
		ret = ioctl(m_hLbaDev,ENABLE_NAND_LBA_READ_IO);
	else
		ret = ioctl(m_hLbaDev,DISABLE_NAND_LBA_READ_IO);
	if (ret)
	{
		if (m_log)
			m_log->Record(_T("ERROR:CtrlNandLbaRead failed,enable=%d,err=%d"),bEnable,errno);
		return false;
	}
	return true;
}

long long CRKUsbComm::RKU_GetFlashSize()
{
    long long length;

	if (m_hLbaDev<0)
	{
		if (!m_bEmmc)
		{
			m_hLbaDev= open(NAND_DRIVER_DEV_LBA,O_RDWR|O_SYNC,0);
			if (m_hLbaDev<0)
			{
				if (m_log)
					m_log->Record(_T("ERROR:RKU_ReadLBA-->open %s failed,err=%d"),NAND_DRIVER_DEV_LBA,errno);
				return ERR_DEVICE_OPEN_FAILED;
			}
			else
			{
				if (m_log)
					m_log->Record(_T("INFO:RKU_ReadLBA-->open %s ok,handle=%d"),NAND_DRIVER_DEV_LBA,m_hLbaDev);
			}
		}
		else
			return ERR_DEVICE_OPEN_FAILED;
	}




	length = lseek64(m_hLbaDev,(off64_t)0,SEEK_END);
	if (length < 0)
	{
		if (m_log){
			m_log->Record(_T("RKU_GetFlashSize get flash size failed"));
        }
		return 0;
	}

	return (length/512);
}



