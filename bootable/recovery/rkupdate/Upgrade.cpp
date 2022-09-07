/*
 * Copyright 2019 Rockchip Electronics Co., Ltd
 * Dayao Ji <jdy@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

//#include "RKImage.h"
//#include "RKLog.h"
//#include "RKComm.h"
#include "rkupdate/RKAndroidDevice.h"
//#include "rkrsa_api.h"
//#include "uuid/uuid.h"
#include "rkupdate/RKSparse.h"
UpgradeCallbackFunc g_callback=NULL;
UpgradeProgressCallbackFunc g_progress_callback=NULL;

bool CreateUid(PBYTE pUid)
{
	if (!pUid)
	{
		return false;
	}
	memset(pUid,0,RKDEVICE_UID_LEN);

	PBYTE pManufactory,pTime,pGuid,pCrc;
	pManufactory = pUid;
	pTime = pManufactory + 8;
	pGuid = pTime + 4;
	pCrc = pGuid + 16;
	memcpy(pManufactory,"ROCKCHIP",8);
	time_t now;
	now = time(NULL);
	memcpy(pTime,(BYTE *)&now,4);
	#if 0
	uuid_t guidValue;
	uuid_generate(guidValue);
	#else
	unsigned char raw[16]={0};
	gen_rand_uuid(raw);
	#endif

	#if 0
	memcpy(pGuid,(BYTE *)guidValue,16);
	#else
	memcpy(pGuid,(BYTE *)raw,16);
	#endif
	
	USHORT usCrc=0;
	usCrc = CRC_CCITT(pManufactory,28);
	memcpy(pCrc,(BYTE *)&usCrc,2);
	return true;
	
}

bool ParsePartitionInfo(string &strPartInfo,string &strName,UINT &uiOffset,UINT &uiLen)
{
	string::size_type pos,prevPos;
	string strOffset,strLen;
	int iCount;
	prevPos = pos = 0;
	if (strPartInfo.size()<=0)
	{
		return false;
	}
	pos = strPartInfo.find('@');
	if (pos==string::npos)
	{
		return false;
	}
	strLen = strPartInfo.substr(prevPos,pos-prevPos);
	strLen.erase(0,strLen.find_first_not_of(_T(" ")));
	strLen.erase(strLen.find_last_not_of(_T(" "))+1);
	if (strchr(strLen.c_str(),'-'))
	{
		uiLen = 0xFFFFFFFF;
	}
	else
	{
		iCount = sscanf(strLen.c_str(),"0x%x",&uiLen);
		if (iCount!=1)
		{
			return false;
		}
	}

	prevPos = pos +1;
	pos = strPartInfo.find('(',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strOffset = strPartInfo.substr(prevPos,pos-prevPos);
	strOffset.erase(0,strOffset.find_first_not_of(_T(" ")));
	strOffset.erase(strOffset.find_last_not_of(_T(" "))+1);
	iCount = sscanf(strOffset.c_str(),"0x%x",&uiOffset);
	if (iCount!=1)
	{
		return false;
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find(')',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strName = strPartInfo.substr(prevPos,pos-prevPos);
	strName.erase(0,strName.find_first_not_of(_T(" ")));
	strName.erase(strName.find_last_not_of(_T(" "))+1);

	return true;
}

bool parse_parameter(char *pParameter,PARAM_ITEM_VECTOR &vecItem)
{
	
	stringstream paramStream(pParameter);
	bool bRet,bFind=false;
	string strLine,strPartition,strPartInfo,strPartName;
	string::size_type line_size,pos,posColon,posComma;
	UINT uiPartOffset,uiPartSize;
	STRUCT_PARAM_ITEM item;
	vecItem.clear();
        int i=0;
	while (!paramStream.eof())
	{
            i++;
	    getline(paramStream,strLine);
            line_size = strLine.size();
	    if (line_size==0)
            {
		continue;
            }
	    if(strLine[0]=='#')
            {
		continue;
            }
	    if (strLine[line_size-1]=='\r')
	    {
		strLine = strLine.substr(0,line_size-1);
	    }
            pos = strLine.find("mtdparts");
	    if (pos==string::npos)
	    {
		continue;
	    }
	    bFind = true;
	    posColon = strLine.find(':',pos);
	    if (posColon==string::npos)
	    {
		continue;
	    }
	    strPartition = strLine.substr(posColon+1);
	    //提取分区信息
		pos = 0;
		posComma = strPartition.find(',',pos);
		while (posComma!=string::npos)
		{
			strPartInfo = strPartition.substr(pos,posComma-pos);
			bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
			if (bRet)
			{			
				strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
			}
			pos = posComma+1;
			posComma = strPartition.find(',',pos);
		}
		strPartInfo = strPartition.substr(pos);
		if (strPartInfo.size()>0)
		{
			bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
			if (bRet)
			{
				strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
			}
		}
		break;
	}
	return bFind;
	
}

bool parse_Gptparameter(string str,PARAM_ITEM_VECTOR &vecItem)
{
	bool bRet,bFind=false;
	string strLine,strPartition,strPartInfo,strPartName;
	string::size_type line_size,pos,posColon,posComma;
	UINT uiPartOffset,uiPartSize;
        ifstream  fin(str);
	STRUCT_PARAM_ITEM item;
	vecItem.clear();
        int i=0;
	while (getline(fin,strLine))
	{
        i++;
        printf("str = %s",strLine.c_str());
	line_size = strLine.size();
	if (line_size==0)
        {
	    continue;
        }
	    if(strLine[0]=='#')
        {
	    continue;
        }
	if (strLine[line_size-1]=='\r')
        {
	    strLine = strLine.substr(0,line_size-1);
	}
	pos = strLine.find("mtdparts");
	if (pos==string::npos)
	{
	    continue;
	}
	bFind = true;
	posColon = strLine.find(':',pos);
	if (posColon==string::npos)
	{
	    continue;
	}
	strPartition = strLine.substr(posColon+1);
	//提取分区信息
	pos = 0;
	posComma = strPartition.find(',',pos);
	while (posComma!=string::npos)
	{
	    strPartInfo = strPartition.substr(pos,posComma-pos);
	    bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
	    if (bRet)
	    {
	        strcpy(item.szItemName,strPartName.c_str());
		item.uiItemOffset = uiPartOffset;
		item.uiItemSize = uiPartSize;
		vecItem.push_back(item);
	    }
	    pos = posComma+1;
	    posComma = strPartition.find(',',pos);
	}
	strPartInfo = strPartition.substr(pos);
	if (strPartInfo.size()>0)
	{
	    bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
	    if (bRet)
	    {
		strcpy(item.szItemName,strPartName.c_str());
		item.uiItemOffset = uiPartOffset;
		item.uiItemSize = uiPartSize;
		vecItem.push_back(item);
	    }
	}
	break;
	}
	return bFind;
}

bool get_parameter_loader( CRKComm *pComm,char *pParameter, int &nParamSize)
{
	if ((nParamSize!=-1)&&(!pParameter))
	{
		return false;
	}
	BYTE paramHead[512];
	DWORD *pParamTag=(DWORD *)paramHead;
	DWORD *pParamSize=(DWORD *)(paramHead+4);
	int iRet;

	iRet = pComm->RKU_ReadLBA(0,1,paramHead);
	if (iRet!=ERR_SUCCESS)
	{
		return false;
	}
	if (*pParamTag!=0x4D524150)
	{
		return false;
	}
	if (nParamSize==-1)
	{//获取parameter大小
		nParamSize = *pParamSize;
		return true;
	}
	if (nParamSize<*pParamSize)
	{
		return false;
	}

	nParamSize = *pParamSize;
	int nParamSec;
	nParamSec = (nParamSize+12-1)/512+1;
	PBYTE pBuffer=NULL;
	pBuffer = new BYTE[nParamSec*512];
	if (!pBuffer)
	{

		return false;
	}
	iRet = pComm->RKU_ReadLBA(0,nParamSec,pBuffer);
	if (iRet!=ERR_SUCCESS)
	{
		delete []pBuffer;
		pBuffer = NULL;
		return false;
	}

	memcpy(pParameter,pBuffer+8,nParamSize);
	delete []pBuffer;
	pBuffer = NULL;
	return true;
}
bool read_bytes_from_partition(DWORD dwPartitionOffset,long long ullstart,DWORD dwCount,PBYTE pOut,CRKComm *pComm)
{
	int iRet;
	UINT uiTransferSize = 16*1024;
	UINT uiTransferSec = uiTransferSize/SECTOR_SIZE;
	BYTE *pBuffer = NULL;
	UINT uiBegin=dwPartitionOffset,uiLen,uiReadBytes=0,uiTmp;
	DWORD dwWritePos=0;
	pBuffer = new BYTE[uiTransferSize];
	if (!pBuffer)
		return false;
	uiTmp = ullstart % 2048;
	if (uiTmp==0)
	{
		uiBegin += ullstart / SECTOR_SIZE;
	}
	else
	{
		uiReadBytes = 2048 - uiTmp;
		uiBegin += ((ullstart/2048)*4);
		uiLen = 4;
		iRet = pComm->RKU_ReadLBA(uiBegin,uiLen,pBuffer);
		if (iRet!=ERR_SUCCESS)
		{
			delete []pBuffer;
			return false;
		}
		if (dwCount>=uiReadBytes)
		{
			memcpy(pOut+dwWritePos,pBuffer+uiTmp,uiReadBytes);
			dwWritePos += uiReadBytes;
			dwCount -= uiReadBytes;
		}
		else
		{
			memcpy(pOut+dwWritePos,pBuffer+uiTmp,dwCount);
			dwWritePos += dwCount;
			dwCount = 0;
		}
		uiBegin += uiLen;
	}
	while (dwCount>0)
	{
		if (dwCount>=uiTransferSize)
		{
			uiReadBytes = uiTransferSize;
			uiLen = uiTransferSec;
		}
		else
		{
			uiReadBytes = dwCount;
			uiLen = BYTE2SECTOR(uiReadBytes);
		}
		iRet = pComm->RKU_ReadLBA(uiBegin,uiLen,pBuffer);
		if (iRet!=ERR_SUCCESS)
		{
			delete []pBuffer;
			return false;
		}
		memcpy(pOut+dwWritePos,pBuffer,uiReadBytes);
		dwWritePos += uiReadBytes;
		dwCount -= uiReadBytes;
		uiBegin += uiLen;
	}
	delete []pBuffer;
	return true;
}

bool check_fw_header(CRKComm *pComm,DWORD dwOffset,PSTRUCT_RKIMAGE_HDR pHeader,CRKLog *pLog=NULL)
{
	int nHeaderSec = BYTE2SECTOR(sizeof(STRUCT_RKIMAGE_HDR));
	char model[256]={0};
	PBYTE pBuf=NULL;
	pBuf = new BYTE[nHeaderSec*SECTOR_SIZE];
	if (!pBuf)
		return false;
	int iRet;
	iRet = pComm->RKU_ReadLBA(dwOffset,nHeaderSec,pBuf);
	if (iRet!=ERR_SUCCESS)
	{
		delete []pBuf;
		pBuf = NULL;
		return false;
	}
	memcpy(pHeader,pBuf,sizeof(STRUCT_RKIMAGE_HDR));
	delete []pBuf;
	pBuf = NULL;
	if (pHeader->tag!=RKIMAGE_TAG)
		return false;
	
    property_get("ro.product.model", model, "");
	if (pLog)
		pLog->Record(_T("model:%s\nbackup firmware model:%s\n"),model,pHeader->machine_model);
    if(strcmp(model, pHeader->machine_model))
    {
        return false;
    }
	return true;
}
bool check_fw_crc(CRKComm *pComm,DWORD dwOffset,PSTRUCT_RKIMAGE_HDR pHeader,CRKLog *pLog=NULL)
{
	int iRet;
	long long ullRemain,ullCrcOffset;
	if (pHeader->machine_model[29]=='H')
	{
		ullRemain = *((DWORD *)(&pHeader->machine_model[30]));
		ullRemain <<= 32;
		ullRemain += pHeader->size;
	}
	else
		ullRemain = pHeader->size;
	if (ullRemain<=0)
		return false;
	ullCrcOffset = ullRemain;
	UINT uiTransferSize = 16*1024;
	UINT uiTransferSec = uiTransferSize/SECTOR_SIZE;
	BYTE *pBuffer = NULL;
	BYTE oldCrc[4];
	UINT uiBegin=dwOffset,uiLen,uiCrc=0,uiReadBytes=0;
	pBuffer = new BYTE[uiTransferSize];
	if (!pBuffer)
		return false;
	while(ullRemain>0)
	{
		if (ullRemain>=uiTransferSize)
		{
			uiReadBytes = uiTransferSize;
			uiLen = uiTransferSec;
		}
		else
		{
			uiReadBytes = ullRemain;
			uiLen = BYTE2SECTOR(uiReadBytes);
		}
		iRet = pComm->RKU_ReadLBA(uiBegin,uiLen,pBuffer);
		if (iRet!=ERR_SUCCESS)
		{
			delete []pBuffer;
			if (pLog)
				pLog->Record(_T("ERROR:check_fw_crc-->RKU_ReadLBA failed,err=%d"),iRet);
			return false;
		}
		uiCrc = CRC_32(pBuffer,uiReadBytes,uiCrc);
		uiBegin += uiLen;
		ullRemain -= uiReadBytes;
	}
	delete []pBuffer;
	if (!read_bytes_from_partition(dwOffset,ullCrcOffset,4,oldCrc,pComm))
	{
		if (pLog)
			pLog->Record(_T("ERROR:check_fw_crc-->read old crc failed"));
		return false;
	}
	if (uiCrc!=*((UINT *)(oldCrc)))
		return false;
	return true;
	
}

bool download_backup_image(PARAM_ITEM_VECTOR &vecParam,char *pszItemName,DWORD dwBackupOffset,STRUCT_RKIMAGE_HDR &hdr,CRKComm *pComm,CRKLog *pLog=NULL)
{
	DWORD dwToOffset,dwToSize;
	int i,iRet;
	if (g_progress_callback)
		g_progress_callback(0.5,50);
	for (i=0;i<vecParam.size();i++)
	{
		if (strcmp(pszItemName,vecParam[i].szItemName)==0)
		{
			dwToOffset = vecParam[i].uiItemOffset;
			dwToSize = vecParam[i].uiItemSize;
			break;
		}
	}
	if (i>=vecParam.size())
	{
		if (pLog)
			pLog->Record(_T("ERROR:download_backup_image-->no found dest partition."));
		return false;
	}
	long long ullSrcPos,ullSrcSize;
	for (i=0;i<hdr.item_count;i++)
	{
		if (strcmp(pszItemName,hdr.item[i].name)==0)
		{
			if (hdr.item[i].file[50]=='H')
			{
				ullSrcPos= *((DWORD *)(&hdr.item[i].file[51]));
				ullSrcPos <<= 32;
				ullSrcPos += hdr.item[i].offset;
			}
			else
			{
				ullSrcPos = hdr.item[i].offset;
			}
			if (hdr.item[i].file[55]=='H')
			{
				ullSrcSize= *((DWORD *)(&hdr.item[i].file[56]));
				ullSrcSize <<= 32;
				ullSrcSize += hdr.item[i].size;
			}
			else
			{
				ullSrcSize = hdr.item[i].size;
			}
			break;
		}
	}
	if (i>=hdr.item_count)
	{
		if (pLog)
			pLog->Record(_T("ERROR:download_backup_image-->no found source in the backup."));
		return false;
	}
	long long ullRemain,ullstart,ullToStart;
	UINT uiBegin,uiLen,uiTransferByte;
	UINT uiBufferSize=16*1024;
	BYTE buffer[16*1024];
	BYTE readbuffer[16*1024];
	
	//write image
	ullRemain = ullSrcSize;
	uiBegin = dwToOffset;
	ullstart = ullSrcPos;
	while(ullRemain>0)
	{
		if (ullRemain>=uiBufferSize)
		{
			uiTransferByte = uiBufferSize;
			uiLen = 32;
		}
		else
		{
			uiTransferByte = ullRemain;
			uiLen = BYTE2SECTOR(uiTransferByte);
		}
		if (!read_bytes_from_partition(dwBackupOffset,ullstart,uiTransferByte,buffer,pComm))
		{
			if (pLog)
				pLog->Record(_T("ERROR:download_backup_image-->read data from backup failed."));
			return false;
		}
		iRet = pComm->RKU_WriteLBA(uiBegin,uiLen,buffer);
		if (iRet!=ERR_SUCCESS)
		{
			if (pLog)
				pLog->Record(_T("ERROR:download_backup_image-->write data to partition failed."));
			return false;
		}
		ullRemain -= uiTransferByte;
		uiBegin += uiLen;
		ullstart += uiTransferByte;
		
	}
	pComm->RKU_ReopenLBAHandle();
	if (g_progress_callback)
		g_progress_callback(1,0);
	if (g_progress_callback)
		g_progress_callback(0.4,30);
//check image
	if (pLog)
		pLog->Record(_T("Start to check system..."));
	ullRemain = ullSrcSize;
	ullToStart = 0;
	ullstart = ullSrcPos;
	while(ullRemain>0)
	{
		if (ullRemain>=uiBufferSize)
		{
			uiTransferByte = uiBufferSize;
		}
		else
		{
			uiTransferByte = ullRemain;
		}
		if (!read_bytes_from_partition(dwBackupOffset,ullstart,uiTransferByte,buffer,pComm))
		{
			if (pLog)
				pLog->Record(_T("ERROR:download_backup_image-->read data from backup failed."));
			return false;
		}
		if (!read_bytes_from_partition(dwToOffset,ullToStart,uiTransferByte,readbuffer,pComm))
		{
			if (pLog)
				pLog->Record(_T("ERROR:download_backup_image-->read data from partition failed."));
			return false;
		}
		if (memcmp(buffer,readbuffer,uiTransferByte)!=0)
		{
			if (pLog)
				pLog->Record(_T("ERROR:download_backup_image-->compare data failed."));
			return false;
		}

		ullRemain -= uiTransferByte;
		ullToStart += uiTransferByte;
		ullstart += uiTransferByte;
		
	}
	if (g_progress_callback)
		g_progress_callback(1,0);
	return true;
}



bool IsDeviceLock(CRKComm *pComm,bool &bLock)
{
	bLock = false;(void)pComm;
	return true;
#if 0
	int iRet;
	BYTE buffer[4];
	iRet = pComm->RKU_GetLockFlag(buffer);
	if (iRet!=ERR_SUCCESS)
		return false;
	DWORD *pFlag=(DWORD *)buffer;
	if (*pFlag==1)
		bLock = true;
	else
		bLock = false;
	return true;
#endif
}
bool GetPubicKeyFromExternal(char *szDev,CRKLog *pLog,unsigned char *pKey,unsigned int &nKeySize)
{
	int hDev=-1;
	int j,ret,nRsaByte;
	bool bSuccess=false;
	BYTE bData[SECTOR_SIZE*8];
	PRKANDROID_IDB_SEC0 pSec0=(PRKANDROID_IDB_SEC0)bData;
	PRK_SECURE_HEADER pSecureHdr=(PRK_SECURE_HEADER)(bData+SECTOR_SIZE*4);
	string strOutput;
	if (!szDev)
	{
		printf("In GetPubicKeyFromExternal device=NULL\n");
		return false;
	}
	else
		printf("In GetPubicKeyFromExternal device=%s\n",szDev);
	hDev= open(szDev,O_RDONLY,0);
	if (hDev<0)
	{
		if (pLog)
			pLog->Record(_T("ERROR:GetPubicKeyFromExternal-->open %s failed,err=%d"),szDev,errno);
		goto Exit_GetPubicKeyFromExternal;
	}
	else
	{
		if (pLog)
			pLog->Record(_T("INFO:GetPubicKeyFromExternal-->%s=%d"),szDev,hDev);
	}

	ret = lseek(hDev,64*512,SEEK_SET);
	if (ret<0)
	{
		if (pLog)
			pLog->Record(_T("ERROR:GetPubicKeyFromExternal-->seek IDBlock failed,err=%d"),errno);
		goto Exit_GetPubicKeyFromExternal;
	}
	ret = read(hDev,bData,8*512);
	if (ret!=8*512)
	{
		if (pLog)
			pLog->Record(_T("ERROR:GetPubicKeyFromExternal-->read IDBlock failed,err=%d"),errno);
		goto Exit_GetPubicKeyFromExternal;
	}
//	if (pLog)
//	{
//		pLog->PrintBuffer(strOutput,bData,512,16);
//		pLog->Record("INFO:idb\n%s",strOutput.c_str());
//	}
	P_RC4(bData,SECTOR_SIZE);
//	if (pLog)
//	{
//		pLog->PrintBuffer(strOutput,bData,512,16);
//		pLog->Record("INFO:idb rc4\n%s",strOutput.c_str());
//	}
	if (pSec0->dwTag!=0x0FF0AA55)
	{
		if (pLog)
			pLog->Record(_T("ERROR:GetPubicKeyFromExternal-->check IDBlock failed,tag=0x%x"),pSec0->dwTag);
		goto Exit_GetPubicKeyFromExternal;
	}
	if (pSec0->uiRc4Flag==0)
	{
		for(j=0;j<4;j++)
			P_RC4(bData+SECTOR_SIZE*(j+4),SECTOR_SIZE);
	}
	if (pSecureHdr->uiTag!=0x4B415352)
	{
		if (pLog)
			pLog->Record(_T("ERROR:GetPubicKeyFromExternal-->check SecureHeader failed,tag=0x%x"),pSecureHdr->uiTag);
		goto Exit_GetPubicKeyFromExternal;	
	}
	nRsaByte = pSecureHdr->usRsaBit/8;
	*((USHORT *)pKey) = pSecureHdr->usRsaBit;
	for(j=0;j<nRsaByte;j++)
		*(pKey+j+2) = pSecureHdr->nFactor[nRsaByte-j-1];
	for(j=0;j<nRsaByte;j++)
		*(pKey+j+2+nRsaByte) = pSecureHdr->eFactor[nRsaByte-j-1];
	nKeySize = nRsaByte*2+2;
//	if (pLog)
//	{
//		pLog->PrintBuffer(strOutput,pKey,nKeySize,16);
//		pLog->Record("INFO:Key\n%s",strOutput.c_str());
//	}
	bSuccess = true;
Exit_GetPubicKeyFromExternal:
	if (hDev!=-1)
		close(hDev);
	return bSuccess;
}

bool GetPubicKeyFromDevice(CRKLog *pLog,unsigned char *pKey,unsigned int &nKeySize)
{
	bool bSuccess=false,bRet;
	CRKComm *pComm=NULL;
	CRKAndroidDevice *pDevice=NULL;
	STRUCT_RKDEVICE_DESC device;
	pComm = new CRKUsbComm(pLog);
	if (!pComm)
	{
		pLog->Record("ERROR:GetPubicKeyFromDevice-->new CRKComm failed!");
		goto EXIT_GetPubicKeyFromDevice;
	}
	pDevice = new CRKAndroidDevice(device);
	if (!pDevice)
	{
		pLog->Record("ERROR:GetPubicKeyFromDevice-->new CRKAndroidDevice failed!");
		goto EXIT_GetPubicKeyFromDevice;
	}
	pDevice->SetObject(NULL,pComm,pLog);
	pDevice->m_pCallback = (UpgradeCallbackFunc)NULL;
	pDevice->m_pProcessCallback = (UpgradeProgressCallbackFunc)NULL;
	bRet = pDevice->GetPublicKey(pKey,nKeySize);
	if (!bRet)
	{
		pLog->Record("ERROR:GetPubicKeyFromDevice-->GetPublicKey failed!");
		goto EXIT_GetPubicKeyFromDevice;
	}
	bSuccess = true;
EXIT_GetPubicKeyFromDevice:
	if (pDevice)
	{
		delete pDevice;
		pDevice = NULL;
	}
	else if (pComm)
	{
		delete pComm;
		pComm = NULL;
	}
	return bSuccess;
}
bool UnlockDevice(CRKImage *pImage,CRKLog *pLog,unsigned char *pKey,unsigned int nKeySize)
{
	PBYTE pMd5,pSignMd5;
	int nSignSize;
	unsigned int nOutput = 0;
	//bool bRet;
	BYTE output[256];
	string strOutput;
	(void)nKeySize;
	printf("in UnlockDevice\n");
	if ((!pImage)||(!pKey))
		return false;
	nSignSize = pImage->GetMd5Data(pMd5,pSignMd5);
	if (nSignSize==0)
	{
		if (pLog)
			pLog->Record("Get signed info failed.");
		return false;
	}
	//bRet= DoRsa(output,&nOutput,pSignMd5,nSignSize,pKey,nKeySize);
	//if (!bRet)
	//{
	//	if (pLog)
	//		pLog->Record("DoRsa failed.");
	//	return false;
	//}
	if(pLog)
	{
		pLog->PrintBuffer(strOutput,pMd5,32,16);
		pLog->Record("INFO:Old Md5\n%s",strOutput.c_str());
		pLog->PrintBuffer(strOutput,output+nOutput-32,32,16);
		pLog->Record("INFO:New Md5\n%s",strOutput.c_str());
	}
	return true;
	if (memcmp(pMd5,output+nOutput-32,32)==0)
		return true;
	else
		return false;
}

bool do_rk_firmware_upgrade(char *szFw,void *pCallback,void *pProgressCallback,char *szBootDev)
{
	bool bSuccess=false,bRet=false,bLock;
	int iRet;
	CRKImage *pImage=NULL;
	CRKLog *pLog=NULL;
	CRKAndroidDevice *pDevice=NULL;
	CRKComm *pComm=NULL;
	STRUCT_RKDEVICE_DESC device;
	//BYTE key[514];
	//UINT nKeySize=514;
	BYTE uid[RKDEVICE_UID_LEN];
	tstring strFw = szFw;
	tstring strUid;
	(void)szBootDev;
	g_callback = (UpgradeCallbackFunc)pCallback;
	g_progress_callback = (UpgradeProgressCallbackFunc)pProgressCallback;
	if (g_progress_callback)
		g_progress_callback(0.1,10);

	pLog = new CRKLog();
	if (!pLog)
		goto EXIT_UPGRADE;
	pLog->Record("Start to upgrade firmware...");
	if (g_callback)
		g_callback("Start to upgrade firmware... \n");
	pImage = new CRKImage(strFw,bRet);
	if (!bRet)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->new CRKImage failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->new CRKImage failed! \n");
		goto EXIT_UPGRADE;
	}
	pComm = new CRKUsbComm(pLog);
	if (!pComm)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->new CRKComm failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->new CRKComm failed! \n");
		goto EXIT_UPGRADE;
	}
	if (IsDeviceLock(pComm,bLock))
	{
		if (bLock)
		{
			bRet = true;
			pImage = new CRKImage(strFw,bRet);
			if (!bRet)
			{
				pLog->Record("ERROR:do_rk_firmware_upgrade-->new CRKImage with check failed,%s!",szFw);
				if (g_callback)
					g_callback("ERROR:do_rk_firmware_upgrade-->new CRKImage with check failed,%s! \n",szFw);
				goto EXIT_UPGRADE;
			}

			//bRet = GetPubicKeyFromExternal(szBootDev,pLog,key,nKeySize);
			//if (!bRet)
			//{
			//	if (szBootDev)
			//		pLog->Record("ERROR:do_rk_firmware_upgrade-->Get PubicKey failed,dev=%s!",szBootDev);
			//	else
			//		pLog->Record("ERROR:do_rk_firmware_upgrade-->Get PubicKey failed,dev=NULL!");
			//	goto EXIT_UPGRADE;
			//}
			//if(access("/res/publicKey.bin",F_OK) == 0){
			//	int fd = open("/tmp/publicKey.bin", O_RDONLY, 0);
			//	nKeySize = read(fd, key, 514);
			//}else{
			//	printf("access /res/publicKey.bin failed!\n");
			//	goto EXIT_UPGRADE;
			//}
			//if (!UnlockDevice(pImage,pLog,key,nKeySize))
			//{
			//	pLog->Record("ERROR:do_rk_firmware_upgrade-->UnlockDevice failed!");
			//	goto EXIT_UPGRADE;
			//}
//			if (pCallback)
//				((UpgradeCallbackFunc)pCallback)("pause");

		}
		else
		{
			pImage = new CRKImage(strFw,bRet);
			if (!bRet)
			{
				pLog->Record("ERROR:do_rk_firmware_upgrade-->new CRKImage failed,%s!",szFw);
				if (g_callback)
					g_callback("ERROR:do_rk_firmware_upgrade-->new CRKImage failed,%s! \n",szFw);
				goto EXIT_UPGRADE;
			}
		}
	}
	else
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->IsDeviceLock failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->IsDeviceLock failed! \n");
		goto EXIT_UPGRADE;
	}
	pDevice = new CRKAndroidDevice(device);
	if (!pDevice)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->new CRKAndroidDevice failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->new CRKAndroidDevice failed! \n");
		goto EXIT_UPGRADE;
	}
	pDevice->SetObject(pImage,pComm,pLog);
	if (CreateUid(uid))
	{
		pDevice->Uid = uid;
		pLog->PrintBuffer(strUid,uid,RKDEVICE_UID_LEN);
		pLog->Record("uid:%s",strUid.c_str());
		if (g_callback)
			g_callback("uid:%s \n",strUid.c_str());
	}
	pDevice->m_pCallback = (UpgradeCallbackFunc)pCallback;
	pDevice->m_pProcessCallback = (UpgradeProgressCallbackFunc)pProgressCallback;
	pLog->Record("Get FlashInfo...");
	if (g_callback)
		g_callback("Get FlashInfo... \n");
	bRet = pDevice->GetFlashInfo();
	if (!bRet)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->GetFlashInfo failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->GetFlashInfo failed! \n");
		goto EXIT_UPGRADE;
	}
	pLog->Record("IDBlock Preparing...");
	if (g_callback)
		g_callback("IDBlock Preparing... \n");
	iRet = pDevice->PrepareIDB();
	if (iRet!=ERR_SUCCESS)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->PrepareIDB failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->PrepareIDB failed! \n");
		goto EXIT_UPGRADE;
	}
	pLog->Record("IDBlock Writing...");
	if (g_callback)
		g_callback("IDBlock Writing... \n");
	iRet = pDevice->DownloadIDBlock();
	if (iRet!=ERR_SUCCESS)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->DownloadIDBlock failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->DownloadIDBlock failed! \n");
		goto EXIT_UPGRADE;
	}

	if (strFw.find(_T(".bin"))!=tstring::npos)
	{
		pLog->Record("INFO:do_rk_firmware_upgrade-->Download loader only success!");
		if (g_callback)
			g_callback("INFO:do_rk_firmware_upgrade-->Download loader only success! \n");
		bSuccess = true;
		return bSuccess;
	}

	if (g_callback)
		g_callback("INFO:do_rk_firmware_upgrade begin DownloadImage... ! \n");
	iRet = pDevice->DownloadImage();
	if (iRet!=ERR_SUCCESS)
	{
		pLog->Record("ERROR:do_rk_firmware_upgrade-->DownloadImage failed!");
		if (g_callback)
			g_callback("ERROR:do_rk_firmware_upgrade-->DownloadImage failed! \n");
		goto EXIT_UPGRADE;
	}

	bSuccess = true;
EXIT_UPGRADE:
	if (bSuccess)
	{
		pLog->Record("Finish to upgrade firmware.");
		if (g_callback)
			g_callback("Finish to upgrade firmware. \n");
	}
	else
	{
		pLog->Record("Fail to upgrade firmware!");
		if (g_callback)
			g_callback("Fail to upgrade firmware! \n");
	}
	if (pLog)
	{
		delete pLog;
		pLog = NULL;
	}
	if (pImage)
	{
		delete pImage;
		pImage = NULL;
	}
	if (pDevice)
	{
		delete pDevice;
		pDevice = NULL;
	}
	else
	{
		if (pComm)
		{
			delete pComm;
			pComm = NULL;
		}
	}
	
	return bSuccess;
}
bool do_rk_partition_upgrade(char *szFw,void *pCallback,void *pProgressCallback,char nBoot,char *szBootDev)
{
	bool bSuccess=false,bRet=false,bLock;
	int iRet;
	CRKImage *pImage=NULL;
	CRKLog *pLog=NULL;
	CRKAndroidDevice *pDevice=NULL;
	CRKComm *pComm=NULL;
	STRUCT_RKDEVICE_DESC device;
	BYTE key[514];
	UINT nKeySize=514;
	tstring strFw = szFw;
	vector<int> vecDownloadEntry;
	vecDownloadEntry.clear();
	g_callback = (UpgradeCallbackFunc)pCallback;
	g_progress_callback = (UpgradeProgressCallbackFunc)pProgressCallback;
	if (g_progress_callback)
		g_progress_callback(0.1,5);
	pLog = new CRKLog();
	if (!pLog)
		goto EXIT_DOWNLOAD;
	pLog->Record("Start to upgrade partition...");
	
	pComm = new CRKUsbComm(pLog);
	if (!pComm)
	{
		pLog->Record("ERROR:do_rk_partition_upgrade-->new CRKComm failed!");
		goto EXIT_DOWNLOAD;
	}
	if (IsDeviceLock(pComm,bLock))
	{
		if (bLock)
		{
			bRet = true;
			pImage = new CRKImage(strFw,bRet);
			if (!bRet)
			{
				pLog->Record("ERROR:do_rk_partition_upgrade-->new CRKImage with check failed,%s!",szFw);
				goto EXIT_DOWNLOAD;
			}
			if(nBoot==0)//get key from nand or emmc
				bRet = GetPubicKeyFromDevice(pLog,key,nKeySize);
			else if((nBoot==1)||(nBoot==2))//get key from sd or usb disk
				bRet = GetPubicKeyFromExternal(szBootDev,pLog,key,nKeySize);
			else
				bRet = false;
			if (!bRet)
			{
				if (szBootDev)
					pLog->Record("ERROR:do_rk_partition_upgrade-->Get PubicKey failed,boot=%d,dev=%s!",nBoot,szBootDev);
				else
					pLog->Record("ERROR:do_rk_partition_upgrade-->Get PubicKey failed,boot=%d,dev=NULL!",nBoot);
				goto EXIT_DOWNLOAD;
			}
				
			if (!UnlockDevice(pImage,pLog,key,nKeySize))
			{
				pLog->Record("ERROR:do_rk_partition_upgrade-->UnlockDevice failed!");
				goto EXIT_DOWNLOAD;
			}
//			if (pCallback)
//				((UpgradeCallbackFunc)pCallback)("pause");

		}
		else
		{
			pImage = new CRKImage(strFw,bRet);
			if (!bRet)
			{
				pLog->Record("ERROR:do_rk_partition_upgrade-->new CRKImage failed,%s!",szFw);
				goto EXIT_DOWNLOAD;
			}
		}
			
	}
	else
	{
		pLog->Record("ERROR:do_rk_partition_upgrade-->IsDeviceLock failed!");
		goto EXIT_DOWNLOAD;
	}
	pDevice = new CRKAndroidDevice(device);
	if (!pDevice)
	{
		pLog->Record("ERROR:do_rk_partition_upgrade-->new CRKAndroidDevice failed!");
		goto EXIT_DOWNLOAD;
	}
	pDevice->SetObject(pImage,pComm,pLog);
	pDevice->m_pCallback = (UpgradeCallbackFunc)pCallback;
	pDevice->m_pProcessCallback = (UpgradeProgressCallbackFunc)pProgressCallback;
	bRet = pDevice->GetFlashInfo();
	if (!bRet)
	{
		pLog->Record("ERROR:do_rk_partition_upgrade-->GetFlashInfo failed!");
		goto EXIT_DOWNLOAD;
	}
	iRet = pComm->RKU_ShowNandLBADevice();
	pLog->Record("Info:do_rk_partition_upgrade-->RKU_ShowNandLBADevice ret=%d",iRet);
	iRet = pDevice->UpgradePartition();
	if (iRet!=ERR_SUCCESS)
	{
		pLog->Record("ERROR:do_rk_partition_upgrade-->DownloadImage failed!");
		goto EXIT_DOWNLOAD;
	}
	
	bSuccess = true;
EXIT_DOWNLOAD:
	if (bSuccess)
	{
		pLog->Record("Finish to upgrade partition.");
	}
	else
	{
		pLog->Record("Fail to upgrade partition!");
	}
	if (pLog)
	{
		delete pLog;
		pLog = NULL;
	}
	if (pImage)
	{
		delete pImage;
		pImage = NULL;
	}
	if (pDevice)
	{
		delete pDevice;
		pDevice = NULL;
	}
	else
	{
		if (pComm)
		{
			delete pComm;
			pComm = NULL;
		}
	}
	
	return bSuccess;
}

	
bool do_rk_backup_recovery(void *pCallback,void *pProgressCallback)
{
	bool bSuccess=false,bRet;
	int i,iRet;
	CRKLog *pLog=NULL;
	CRKComm *pComm=NULL;
	char *pParam=NULL;
	int nParamSize=-1;
	DWORD dwBackupOffset=0;
	PARAM_ITEM_VECTOR vecParam;
	STRUCT_RKIMAGE_HDR hdr;
	g_callback = (UpgradeCallbackFunc)pCallback;
	g_progress_callback = (UpgradeProgressCallbackFunc)pProgressCallback;
	if (g_progress_callback)
		g_progress_callback(0.1,10);
	pLog = new CRKLog();
	if (!pLog)
		goto EXIT_RECOVERY;
	pLog->Record("Start to recovery from backup...");
	
	pComm = new CRKUsbComm(pLog);
	if (!pComm)
	{
		pLog->Record("ERROR:do_rk_backup_recovery-->new CRKComm failed!");
		goto EXIT_RECOVERY;
	}
	iRet = pComm->RKU_ShowNandLBADevice();
	pLog->Record("Info:do_rk_backup_recovery-->RKU_ShowNandLBADevice ret=%d",iRet);
	pLog->Record("Start to read parameter...");
	bRet = get_parameter_loader(pComm,pParam,nParamSize);
	if (bRet)
	{
		pParam = new char[nParamSize];
		if (pParam)
		{
			bRet = get_parameter_loader(pComm,pParam,nParamSize);	
		}	
	}
	if (!bRet)
	{
		pLog->Record("Read parameter failed!");
		goto EXIT_RECOVERY;
	}
	pLog->Record("Start to parse parameter...");
	bRet = parse_parameter(pParam,vecParam);
	if (!bRet)
	{
		pLog->Record("Parse parameter failed!");
		goto EXIT_RECOVERY;
	}
	for (i=0;i<vecParam.size();i++)
	{
		if (strcmp(vecParam[i].szItemName,PARTNAME_BACKUP)==0)
		{
			dwBackupOffset = vecParam[i].uiItemOffset;
			break;
		}
	}
	if (dwBackupOffset==0)
	{
		pLog->Record("Get backup offset failed!");
		goto EXIT_RECOVERY;
	}
	pLog->Record("Start to check firmware...");
	if (!check_fw_header(pComm,dwBackupOffset,&hdr,pLog))
	{
		pLog->Record("Check firmware header failed!");
		goto EXIT_RECOVERY;
	}

	if (!check_fw_crc(pComm,dwBackupOffset,&hdr,pLog))
	{
		pLog->Record("Check firmware crc failed!");
		goto EXIT_RECOVERY;
	}

	pLog->Record("Start to write system...");
	if(!download_backup_image(vecParam,(char*)PARTNAME_SYSTEM,dwBackupOffset,hdr,pComm,pLog))
	{
		pLog->Record("write system failed!");
		goto EXIT_RECOVERY;
	}

	bSuccess = true;
EXIT_RECOVERY:
	if (bSuccess)
	{
		pLog->Record("Finish to recovery from backup.");
	}
	else
	{
		pLog->Record("Fail to recovery from backup!");
	}
	if (pParam)
	{
		delete []pParam;
		pParam = NULL;
	}
	
	if (pLog)
	{
		delete pLog;
		pLog = NULL;
	}

	if (pComm)
	{
		delete pComm;
		pComm = NULL;
	}
	
	return bSuccess;
	
}


bool do_rk_sparse_update(const char *partitionName, const char *src_path){
    bool bSuccess=false, bRet;
    int i, iRet;
    CRKComm *pComm = NULL;
    CRKLog *pLog = NULL;
    PARAM_ITEM_VECTOR vecParam;
    int nParamSize = -1;
    char *pParam = NULL;
    DWORD dwBackupOffset = 0;

    pLog = new CRKLog();
    if(!pLog)
        goto EXIT_RECOVERY;
    pLog->Record("Start to do_rk_sparse_update ...");

    pComm = new CRKUsbComm(pLog);
    if(!pComm)
    {
        pLog->Record("ERROR:do_rk_sparse_update-->new CRKComm failed!");
        goto EXIT_RECOVERY;
    }

    iRet = pComm->RKU_ShowNandLBADevice();
    pLog->Record("Info:do_rk_sparse_update-->RKU_ShowNandLBADevice ret=%d",iRet);
    pLog->Record("Start to read parameter...");
    bRet = get_parameter_loader(pComm, pParam, nParamSize);


    if(bRet)
    {
        pParam = new char[nParamSize];
        if (pParam)
        {
            bRet = get_parameter_loader(pComm, pParam, nParamSize);
        }
    }

    if(!bRet)
    {
        pLog->Record("Read parameter failed!");
        goto EXIT_RECOVERY;
    }

    pLog->Record("Start to parse parameter...");
    bRet = parse_parameter(pParam, vecParam);
    if(!bRet)
    {
        pLog->Record("Parse parameter failed!");
        goto EXIT_RECOVERY;
    }


    for(i = 0;i < vecParam.size();i++)
    {
        if (strcmp(vecParam[i].szItemName, partitionName) == 0){
            dwBackupOffset = vecParam[i].uiItemOffset;
            break;
        }
    }

    if(dwBackupOffset == 0){
        pLog->Record("Get %s offset failed!", partitionName);
        goto EXIT_RECOVERY;
    }else{
        printf("dwBackupOffset is %d.\n", dwBackupOffset);
        //dwBackupOffset
        RKSparse *sparse = new RKSparse(src_path);
        if(sparse->SparseFile_Download(dwBackupOffset, pComm))
            bSuccess = true;
        delete sparse;
    }


EXIT_RECOVERY:
    if (bSuccess)
    {
        pLog->Record("Finish to recovery from backup.");
    }
    else
    {
        pLog->Record("Fail to recovery from backup!");
    }
    if (pParam)
    {
        delete []pParam;
        pParam = NULL;
    }

    if (pLog)
    {
        delete pLog;
        pLog = NULL;
    }

    if (pComm)
    {
        delete pComm;
        pComm = NULL;
    }

    return bSuccess;
}
bool do_rk_gpt_update(char *szFw,void *pCallback,void *pProgressCallback,char *szBootDev){
    bool bSuccess=false, bRet;
    int iRet,iFileSize;
    CRKImage *pImage=NULL;
    CRKLog *pLog=NULL;
    CRKAndroidDevice *pDevice=NULL;
    CRKComm *pComm=NULL;
    PARAM_ITEM_VECTOR vecParam;
    //int nParamSize = -1;
    //char *pParam = NULL;
    //DWORD dwBackupOffset = 0;
    BYTE *m_paramBuffer;
    BYTE *m_gptBuffer = nullptr;
    BYTE *backup_gpt;
    PARAM_ITEM_VECTOR vecItems;
    CONFIG_ITEM_VECTOR vecUuids;
    long long  uiFlashSize;  //bit
    tstring strFw = szFw;
   // BYTE uid[RKDEVICE_UID_LEN];
    tstring strUid;
    //STRUCT_RKDEVICE_DESC device;
    FILE *m_pFile;
    DWORD uiActualRead;
	(void)pCallback;(void)pProgressCallback;(void)szBootDev;
    pLog->Record("ERROR:strFw======%s",szFw);
    pLog = new CRKLog();
    if (!pLog)
	goto EXIT_UPGRADE;
    pLog->Record("Start to do_rk_gpt_update ...");
    pComm = new CRKUsbComm(pLog);
	if (!pComm)
	{
		pLog->Record("ERROR:do_rk_gpt_update-->new CRKComm failed!");
		goto EXIT_UPGRADE;
	}
    uiFlashSize = pComm->m_FlashSize;
    pLog->Record("uiFlashSize ------------ %lld...",uiFlashSize);
    if (!m_gptBuffer)
	{
		m_gptBuffer = new BYTE[SECTOR_SIZE*67];
		if (!m_gptBuffer)
		{
			if (pLog)
			{
				pLog->Record(_T("ERROR:RKA_Gpt_Download-->new memory failed,err=%d)"),errno);
			}
			goto EXIT_UPGRADE;
		}
	}
	memset(m_gptBuffer,0,SECTOR_SIZE*67);
    //文件操作
    m_pFile = fopen(szFw,"rb");
    fseek(m_pFile, 0, SEEK_END);
    iFileSize=ftell(m_pFile);
    m_paramBuffer = new BYTE[iFileSize];
    memset(m_paramBuffer,0,iFileSize);
    pLog->Record(_T("iFileSize is %d"),iFileSize);
    fseek(m_pFile,0,SEEK_SET);
	uiActualRead = fread(m_paramBuffer,1,iFileSize,m_pFile);
    if(uiActualRead != iFileSize)
    {
        pLog->Record(_T("ERROR:RKA_Gpt_Download-->read parameter file fail,read is %d,total is %d!"),uiActualRead,iFileSize);
    }
    bRet = parse_parameter((char *)(m_paramBuffer+8),vecItems);
	if (!bRet)
	{
		if (pLog)
		{
			pLog->Record(_T("ERROR:RKA_Gpt_Download-->parse_parameter failed)"));
		}
		goto EXIT_UPGRADE;
	}
	bRet = get_uuid_from_parameter((char *)(m_paramBuffer+8),vecUuids);
	backup_gpt = m_gptBuffer+34*SECTOR_SIZE;
	create_gpt_buffer(m_gptBuffer,vecItems,vecUuids,uiFlashSize/512);
	memcpy(backup_gpt, m_gptBuffer + 2* SECTOR_SIZE, 32 * SECTOR_SIZE);
	memcpy(backup_gpt + 32 * SECTOR_SIZE, m_gptBuffer + SECTOR_SIZE, SECTOR_SIZE);
	prepare_gpt_backup(m_gptBuffer, backup_gpt, uiFlashSize/512);
	iRet = pComm->RKU_WriteLBA(0,34,m_gptBuffer);
	if (iRet!=ERR_SUCCESS)
	{
		if (pLog)
		{
			pLog->Record(_T("ERROR:RKA_Gpt_Download-->write gpt master failed,RetCode(%d)"),iRet);
		}
		goto EXIT_UPGRADE;
	}
	if (pLog)
	{
		pLog->Record(_T("INFO:RKA_Gpt_Download-->write gpt master successfully!"));
	}
	iRet = pComm->RKU_WriteLBA(uiFlashSize/512-33,33,backup_gpt);
	if (iRet!=ERR_SUCCESS)
	{
		if (pLog)
		{
			pLog->Record(_T("ERROR:RKA_Gpt_Download-->write gpt backup failed,RetCode(%d)"),iRet);
		}
		goto EXIT_UPGRADE;
	}
	if (pLog)
	{
		pLog->Record(_T("INFO:RKA_Gpt_Download-->write gpt backup also successfully!"));
	}
    bSuccess = true;

EXIT_UPGRADE:
	if (bSuccess)
	{
		pLog->Record("Finish to upgrade parameter.");
	}
	else
	{
		pLog->Record("Fail to upgrade parameter!");
	}
	if (pLog)
	{
		delete pLog;
		pLog = NULL;
	}
	if (pImage)
	{
		delete pImage;
		pImage = NULL;
	}
	if (pDevice)
	{
		delete pDevice;
		pDevice = NULL;
	}
	else
	{
		if (pComm)
		{
			delete pComm;
			pComm = NULL;
		}
	}
	return bSuccess;
}

