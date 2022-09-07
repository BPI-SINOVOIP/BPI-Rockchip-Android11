/*
 * Copyright 2019 Rockchip Electronics Co., Ltd
 * Dayao Ji <jdy@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _RKSPARSE_H
#define _RKSPARSE_H
#include "DefineHeader.h"
#include "RKComm.h"
#define LBA_TRANSFER_SIZE 16*1024
class RKSparse{
public:
    sparse_header m_header;
    chunk_header m_chunk;
    FILE *m_pFile;
    const char *fileName;
    RKSparse(const char *filePath);
    ~RKSparse();
    bool IsSparseImage();
    long long GetSparseImageSize();
    bool SparseFile_Download(DWORD dwPos, CRKComm *pComm);
    //bool SparseFile_Check();
    void display();
private:
    bool readfile(long long dwOffset, DWORD dwSize, PBYTE lpBuffer);
    //bool writefile();
};
#endif
