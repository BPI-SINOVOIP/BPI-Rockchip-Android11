#include "genMesh.h"
#include <iostream>
#include <math.h>
//#include <cmath>

/* FEC: ��ʼ��, ����ͼ������ֱ���, ����FECӳ������ز���, ������Ҫ��buffer */
void genFecMeshInit(int srcW, int srcH, int dstW, int dstH, FecParams &fecParams, CameraCoeff &camCoeff)
{
	fecParams.srcW = srcW;
	fecParams.srcH = srcH;
	fecParams.dstW = dstW;
	fecParams.dstH = dstH;
	/* ��չ���� */
	fecParams.srcW_ex = 32 * ((srcW + 31) / 32);
	fecParams.srcH_ex = 32 * ((srcH + 31) / 32);
	fecParams.dstW_ex = 32 * ((dstW + 31) / 32);
	fecParams.dstH_ex = 32 * ((dstH + 31) / 32);
	/* ӳ���Ĳ��� */
	int meshStepW, meshStepH;
	if (dstW > 1920) { //32x16
		meshStepW = 32;
		meshStepH = 16;
	}
	else { //16x8
		meshStepW = 16;
		meshStepH = 8;
	}
	/* ӳ���Ŀ�� */
	fecParams.meshSizeW = (fecParams.dstW_ex + meshStepW - 1) / meshStepW + 1;//modify to mesh alligned to 32x32
	fecParams.meshSizeH = (fecParams.dstH_ex + meshStepH - 1) / meshStepH + 1;//modify to mesh alligned to 32x32

	/* mesh�������Ĳ��� */
	fecParams.meshStepW = meshStepW;
	fecParams.meshStepH = meshStepH;
	//fecParams.meshStepW = double(dstW) / double(fecParams.meshSizeW);
	//fecParams.meshStepH = double(dstH) / double(fecParams.meshSizeH);

	/* MeshXY�Ĵ�С */
	fecParams.meshSize1bin = fecParams.meshSizeW * fecParams.meshSizeH;

	/* �����С��MeshXY */
	fecParams.pMeshXY = new unsigned short[fecParams.meshSize1bin * 2 * 2];

	/* ����4��mesh����ز��� */
	unsigned short SpbMeshPNum = 128 / meshStepH * fecParams.meshSizeW;
	unsigned long MeshNumW;
	int LastSpbH;
	fecParams.SpbNum = (dstH + 128 - 1) / 128;
	MeshNumW = fecParams.dstW_ex / meshStepW;
	fecParams.MeshPointNumW = MeshNumW + 1;
	fecParams.SpbMeshPNumH = 128 / meshStepH + 1;//16x8 -> 17, 32x16 -> 9
	LastSpbH = (fecParams.dstH_ex % 128 == 0) ? 128 : (fecParams.dstH_ex % 128);//modify to mesh alligned to 32x32
	fecParams.LastSpbMeshPNumH = LastSpbH / meshStepH + 1;
	/* 4��mesh�Ĵ�С */
	fecParams.meshSize4bin = (fecParams.SpbNum - 1) * fecParams.MeshPointNumW * fecParams.SpbMeshPNumH + fecParams.MeshPointNumW * fecParams.LastSpbMeshPNumH;

	/* Ԥ�ȼ���Ĳ���: ����δУ����С���level=0,level=255�Ķ���ʽ���� */
	genFecPreCalcPart(fecParams, camCoeff);
}

/* FEC: ����ʼ�� */
void genFecMeshDeInit(FecParams &fecParams)
{
	delete[] fecParams.pMeshXY;
}

/* FEC: 4��mesh �ڴ����� */
void mallocFecMesh(int meshSize, unsigned short **pMeshXI, unsigned char **pMeshXF, unsigned short **pMeshYI, unsigned char **pMeshYF)
{
	*pMeshXI = new unsigned short[meshSize];
	*pMeshXF = new unsigned char[meshSize];
	*pMeshYI = new unsigned short[meshSize];
	*pMeshYF = new unsigned char[meshSize];
}

/* FEC: 4��mesh �ڴ��ͷ� */
void freeFecMesh(unsigned short *pMeshXI, unsigned char *pMeshXF, unsigned short *pMeshYI, unsigned char *pMeshYF)
{
	delete[] pMeshXI;
	delete[] pMeshXF;
	delete[] pMeshYI;
	delete[] pMeshYF;
}


/* =============================================================================================================================================================================== */


/* LDCH: ��ʼ��, ����ͼ������ֱ���, ����LDCHӳ������ز���, ������Ҫ��buffer */
void genLdchMeshInit(int srcW, int srcH, int dstW, int dstH, LdchParams &ldchParams, CameraCoeff &camCoeff)
{
	ldchParams.srcW = srcW;
	ldchParams.srcH = srcH;
	ldchParams.dstW = dstW;
	ldchParams.dstH = dstH;

	int map_scale_bit_X = 4;
	int map_scale_bit_Y = 3;
	/* ���㻯����λ�� */
	ldchParams.mapxFixBit = 4;
	if (dstW > 4096)
	{
		ldchParams.mapxFixBit = 3;
	}
	// mesh��Ŀ�, ��2688->169
	ldchParams.meshSizeW = ((dstW + (1 << map_scale_bit_X) - 1) >> map_scale_bit_X) + 1;
	// mesh��ĸ�, ��1520->191
	ldchParams.meshSizeH = ((dstH + (1 << map_scale_bit_Y) - 1) >> map_scale_bit_Y) + 1;

	/* mesh�������Ĳ��� */
	//ldchParams.meshStepW = double(dstW) / double(ldchParams.meshSizeW - 1);/* �����1��ҪӲ������֤һ�� */
	//ldchParams.meshStepH = double(dstH) / double(ldchParams.meshSizeH - 1);
	ldchParams.meshStepW = 16;
	ldchParams.meshStepH = 8;

	/* �����Ŀ� */
	int mapWidAlign = ((ldchParams.meshSizeW + 1) >> 1) << 1;//����, �ֱ���2688*1520, 169->170
	ldchParams.meshSize = mapWidAlign * ldchParams.meshSizeH;

	//ldchParams.meshSizeW = mapWidAlign;

	/* �����mesh���� */
	ldchParams.mapx = new double[ldchParams.meshSize];
	ldchParams.mapy = new double[ldchParams.meshSize];

	/* LDCH: Ԥ�ȼ���Ĳ���: ����δУ����С���level=0,level=255�Ķ���ʽ���� */
	genLdchPreCalcPart(ldchParams, camCoeff);

	/* LDCH: ����LDCH�ܹ�У�������̶� */
	if (ldchParams.isLdchOld)
	{
		calcLdchMaxLevel(ldchParams, camCoeff);
	}
	else
	{
		ldchParams.maxLevel = 255;
	}
}

/* LDCH: ����ʼ�� */
void genLdchMeshDeInit(LdchParams &ldchParams)
{
	delete[] ldchParams.mapx;
	delete[] ldchParams.mapy;
}
