/*
 * Copyright 2019 Rockchip Electronics Co., Ltd
 * Dayao Ji <jdy@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <iostream>
#include "rkupdate/RKSparse.h"
#include "rkupdate/RKDevice.h"
using namespace std;

RKSparse::RKSparse(const char* filePath){
    // 1. 打开文件
    m_pFile = fopen(filePath, "rb");
    fileName = filePath;
    // 2. 设置m_header
    readfile(0, sizeof(m_header), (PBYTE)&m_header);
    // 3. 设置m_chunk
    readfile(0 + sizeof(m_header), sizeof(m_chunk), (PBYTE)&m_chunk);
};

RKSparse::~RKSparse(){
    if(m_pFile != NULL){
        fclose(m_pFile);
    }
    remove(fileName);
    sync();
}
bool RKSparse::IsSparseImage(){
    if(m_header.magic != SPARSE_HEADER_MAGIC)
    {
        printf("Image isn't Sparse.\n");
        return false;
    }else{
        printf("Image is Sparse.\n");
        return true;
    }
}

long long RKSparse::GetSparseImageSize(){
    return m_header.blk_sz * m_header.total_blks;
}

bool RKSparse::SparseFile_Download(DWORD dwPos, CRKComm *pComm){
    UINT uiLBATransferSize = (LBA_TRANSFER_SIZE) * 1;
    UINT uiBufferSize = uiLBATransferSize;  // buffer 的长度
    UINT uiLBASector = uiLBATransferSize/SECTOR_SIZE;
    UINT uiCurChunk; //当前块的计数
    UINT uiChunkDataSize, uiWriteByte, uiLen, uiBegin, uiFillByte, uiCrc;
    int iRet, i;
    long long uiEntryOffset; //数据的偏移量
    chunk_header chunk;
    PBYTE pBuffer = NULL;

    uiEntryOffset = sizeof(m_header);
    uiCurChunk = 0;
    uiLen = 0;
    uiWriteByte = 0;
    uiBegin = dwPos;
    
    //申请buffer，用来放置要写入的数据
    pBuffer = new BYTE[uiBufferSize];
    if(pBuffer == NULL){
        printf("Failed ==== new Buffer %s:%d.\n", __func__, __LINE__);
        return false;
    }

    while(uiCurChunk < m_header.total_chunks){
        bool bRet = false;
        bRet = readfile(uiEntryOffset, sizeof(chunk), (PBYTE)&chunk); 
        if(!bRet){
            printf("Failed ==== readfile %s:%d.\n", __func__, __LINE__);
            delete []pBuffer;
            pBuffer = NULL;
            return false;
        }
        uiCurChunk++;
        uiEntryOffset += sizeof(chunk);
        switch (chunk.chunk_type)
        {
        case CHUNK_TYPE_RAW:
            uiChunkDataSize = chunk.total_sz - sizeof(chunk_header); //当前chunk 数据长度
            while (uiChunkDataSize)
            {
                memset(pBuffer, 0, uiBufferSize);

                if(uiChunkDataSize < uiBufferSize )
                {
                    uiWriteByte = uiChunkDataSize;
                    uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
                }
                else
                {
                    uiWriteByte = uiBufferSize;
                    uiLen = uiLBASector;
                }

                if (!readfile(uiEntryOffset, uiWriteByte, pBuffer))
                {
                    printf("ERROR:get chunk data failed,chunk=%d, %s:%d",uiCurChunk, __func__, __LINE__);
                    delete []pBuffer;
                    pBuffer = NULL;
                    return false;
                }

                uiEntryOffset += uiWriteByte;
                iRet = pComm->RKU_WriteLBA(uiBegin, uiLen, pBuffer, 0);
                if( iRet!=ERR_SUCCESS )
                {
                    printf("ERROR:RKA_SparseFile_Download-->RKU_WriteLBA failed,RetCode(%d),chunk=%d", iRet, uiCurChunk);

                    delete []pBuffer;
                    pBuffer = NULL;
                    return false;
                }
                uiBegin += uiLen;
                //currentByte += uiWriteByte;
                uiChunkDataSize -= uiWriteByte;

            }
            break;
        case CHUNK_TYPE_FILL:
            uiChunkDataSize = chunk.chunk_sz * m_header.blk_sz;
            if (readfile(uiEntryOffset, 4, (PBYTE)&uiFillByte))
            {
                printf("ERROR:RKA_SparseFile_Download-->get fill byte failed,chunk=%d", uiCurChunk);
                delete []pBuffer;
                pBuffer = NULL;
                return false;
            }

            uiEntryOffset += 4;
            while(uiChunkDataSize)
            {
                memset(pBuffer, 0, uiBufferSize);
                if (uiChunkDataSize < uiBufferSize )
                {
                    uiWriteByte = uiChunkDataSize;
                    uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
                }
                else
                {
                    uiWriteByte = uiBufferSize;
                    uiLen = uiLBASector;
                }

                for (i = 0; i < uiWriteByte/4; i++)
                {
                    *(UINT *)(pBuffer + i*4) = uiFillByte;
                }

                uiEntryOffset += uiWriteByte;
                iRet = pComm->RKU_WriteLBA(uiBegin, uiLen, pBuffer, 0);

                if(iRet != ERR_SUCCESS)
                {
                    printf(" ERROR:RKA_SparseFile_Download-->RKU_WriteLBA failed,RetCode(%d),chunk=%d", iRet, uiCurChunk);

                    delete []pBuffer;
                    pBuffer = NULL;
                    return false;
                }
                uiBegin += uiLen;
                //currentByte += uiWriteByte;
                uiChunkDataSize -= uiWriteByte;

            }
            break;
            
        case CHUNK_TYPE_DONT_CARE:
            uiChunkDataSize = chunk.chunk_sz * m_header.blk_sz;
            //currentByte += uiChunkDataSize;
            uiLen = ( (uiChunkDataSize%SECTOR_SIZE==0) ? (uiChunkDataSize/SECTOR_SIZE) : (uiChunkDataSize/SECTOR_SIZE+1) );
            uiBegin += uiLen;
            break;
        case CHUNK_TYPE_CRC32:
            bRet = readfile(uiEntryOffset, 4, (PBYTE)&uiCrc);
            uiEntryOffset += 4;
            break;
        } 
    }
    printf("======%s:%d====== success.\n", __func__, __LINE__);
    return true;
}

bool RKSparse::readfile(long long dwOffset, DWORD dwSize, PBYTE lpBuffer){

    if (dwOffset < 0 || dwSize == 0)
    {
        return false;
    }
    //if ( dwOffset + dwSize > m_fileSize)
    //{
    //    return false;
    //}

    fseeko(m_pFile, dwOffset, SEEK_SET);
    UINT uiActualRead;
    uiActualRead = fread(lpBuffer, 1, dwSize, m_pFile);

    if (dwSize!=uiActualRead)
    {
        printf("readfile failed.\n");
        return false;
    }
    return true;
}

void RKSparse::display(){
    printf("in RKSparse display.\n");
    printf("m_header->magic is %x.\n",  m_header.magic);
}

