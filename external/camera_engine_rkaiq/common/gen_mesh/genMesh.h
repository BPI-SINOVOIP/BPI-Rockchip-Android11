/*
 *  Copyright (c) 2020 Rockchip Corporation
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
 *
 */

#ifndef __GENMESH_H__
#define __GENMESH_H__

#define INV_POLY_COEFF_NUM 21								/* ����ʽϵ������, ��ߴ���(INV_POLY_COEFF_NUM-1)�� */

 /* ������� */
struct CameraCoeff
{
	double cx, cy;											/* ��ͷ�Ĺ��� */
	double a0, a2, a3, a4;									/* ��ͷ�Ļ���ϵ�� */
	double c, d, e;											/* �ڲ�[c d;e 1] */
	double sf;												/* sf�����ӽ�, sfԽ���ӽ�Խ�� */

	/* level = 0ʱ��rho-tanTheta����ʽ��� */
	int invPolyTanNum0;										/* ��Ϻ��ϵ������ */
	double invPolyTanCoeff0[INV_POLY_COEFF_NUM];		/* ����ʽϵ��, ��ߴ���(INV_POLY_COEFF_NUM-1)�� */
	/* level = 0ʱ��rho-cotTheta����ʽ��� */
	int invPolyCotNum0;										/* ��Ϻ��ϵ������ */
	double invPolyCotCoeff0[INV_POLY_COEFF_NUM];		/* ����ʽϵ��, ��ߴ���(INV_POLY_COEFF_NUM-1)�� */

	/* level = 255ʱ��rho-tanTheta����ʽ��� */
	int invPolyTanNum255;									/* ��Ϻ��ϵ������ */
	double invPolyTanCoeff255[INV_POLY_COEFF_NUM];		/* ����ʽϵ��, ��ߴ���(INV_POLY_COEFF_NUM-1)�� */
	/* level = 255ʱ��rho-cotTheta����ʽ��� */
	int invPolyCotNum255;									/* ��Ϻ��ϵ������ */
	double invPolyCotCoeff255[INV_POLY_COEFF_NUM];		/* ����ʽϵ��, ��ߴ���(INV_POLY_COEFF_NUM-1)�� */
};

/* ����FECӳ�����صĲ��� */
struct FecParams
{
	int correctX;										/* ˮƽx����У��: 1����У��, 0����У�� */
	int correctY;										/* ��ֱy����У��: 1����У��, 0����У�� */
	int saveMaxFovX;									/* ����ˮƽx�������FOV: 1������, 0�������� */
	int isFecOld;										/* �Ƿ�ɰ�FEC: 1�����ǣ�0������ */
	int saveMesh4bin;									/* �Ƿ񱣴�meshxi,xf,yi,yf4��bin�ļ���1������, 0�������� */
	char mesh4binPath[256];								/* ����meshxi,xf,yi,yf4��bin�ļ���·�� */
	int srcW, srcH, dstW, dstH;							/* �������ͼ��ķֱ��� */
	int srcW_ex, srcH_ex, dstW_ex, dstH_ex;				/* ��չ�����������ֱ��� */
	double cropStepW[2000], cropStepH[2000];
	double cropStartW[2000], cropStartH[2000];
	int meshSizeW, meshSizeH;
	double meshStepW, meshStepH;
	int meshSize1bin;
	int meshSize4bin;
	unsigned short	SpbNum;
	unsigned long	MeshPointNumW;
	unsigned short	SpbMeshPNumH;
	unsigned short	LastSpbMeshPNumH;

	unsigned short	*pMeshXY;

};

/* ����LDCHӳ�����صĲ��� */
struct LdchParams
{
	int saveMaxFovX;									/* ����ˮƽx�������FOV: 1������, 0�������� */
	int isLdchOld;										/* �Ƿ�ɰ�LDCH: 1�����ǣ�0������ */
	int saveMeshX;										/* �Ƿ񱣴�MeshX.bin�ļ�: 1������, 0�������� */
	char meshPath[256];									/* ����MeshX.bin�ļ���·�� */
	int srcW, srcH, dstW, dstH;							/* �������ͼ��ķֱ��� */
	int meshSizeW, meshSizeH;
	double meshStepW, meshStepH;
	int mapxFixBit;										/* ���㻯����λ�� */
	int meshSize;
	int maxLevel;
	double *mapx;
	double *mapy;
};


/* =============================================================================================================================================================================== */

/* FEC: ��ʼ��, ����ͼ������ֱ���, ����FECӳ������ز���, ������Ҫ��buffer */
void genFecMeshInit(int srcW, int srcH, int dstW, int dstH, FecParams &fecParams, CameraCoeff &camCoeff);

/* FEC: ����ʼ�� */
void genFecMeshDeInit(FecParams &fecParams);

/* FEC: Ԥ�ȼ���Ĳ���: ����δУ����С���level=0,level=255�Ķ���ʽ���� */
void genFecPreCalcPart(FecParams &fecParams, CameraCoeff &camCoeff);

/* FEC: 4��mesh �ڴ����� */
void mallocFecMesh(int meshSize, unsigned short **pMeshXI, unsigned char **pMeshXF, unsigned short **pMeshYI, unsigned char **pMeshYF);

/* FEC: 4��mesh �ڴ��ͷ� */
void freeFecMesh(unsigned short *pMeshXI, unsigned char *pMeshXF, unsigned short *pMeshYI, unsigned char *pMeshYF);

/*
��������: ���ɲ�ͬУ���̶ȵ�meshӳ���, ����ISP��FECģ��
	����:
	1��FECӳ������ز���, ������Ҫ��buffer: FecParams &fecParams
	2������궨����: CameraCoeff &camCoeff
	3����ҪУ���ĳ̶�: level(0-255: 0��ʾУ���̶�Ϊ0%, 255��ʾУ���̶�Ϊ100%)
	���:
	1��bool �Ƿ�ɹ�����
	2��pMeshXI, pMeshXF, pMeshYI, pMeshYF
*/
bool genFECMeshNLevel(FecParams &fecParams, CameraCoeff &camCoeff, int level, unsigned short *pMeshXI, unsigned char *pMeshXF, unsigned short *pMeshYI, unsigned char *pMeshYF);


/* =============================================================================================================================================================================== */

/* LDCH: ��ʼ��, ����ͼ������ֱ���, ����LDCHӳ������ز���, ������Ҫ��buffer */
void genLdchMeshInit(int srcW, int srcH, int dstW, int dstH, LdchParams &ldchParams, CameraCoeff &camCoeff);

/* LDCH: ����ʼ�� */
void genLdchMeshDeInit(LdchParams &ldchParams);

/* LDCH: Ԥ�ȼ���Ĳ���: ����δУ����С���level=0,level=255�Ķ���ʽ���� */
void genLdchPreCalcPart(LdchParams &ldchParams, CameraCoeff &camCoeff);

/* LDCH: ����LDCH�ܹ�У�������̶� */
void calcLdchMaxLevel(LdchParams &ldchParams, CameraCoeff &camCoeff);

/*
��������: ���ɲ�ͬУ���̶ȵ�meshӳ���, ����ISP��LDCHģ��

	����:
	1��LDCHӳ������ز���, ������Ҫ��buffer: LdchParams &ldchParams
	2������궨����: CameraCoeff &camCoeff
	3����ҪУ���ĳ̶�: level(0-255: 0��ʾУ���̶�Ϊ0%, 255��ʾУ���̶�Ϊ100%)
	���:
	1��bool �Ƿ�ɹ�����
	2��pMeshX
*/
bool genLDCMeshNLevel(LdchParams &ldchParams, CameraCoeff &camCoeff, int level, unsigned short *pMeshX);

/* =============================================================================================================================================================================== */

/* ���ͼ���ROI������ز��� */
struct RoiParams
{
	int startW;		/* ROI�������ʼ�� */
	int startH;
	int roiW;		/* ROI����Ŀ�� */
	int roiH;
};

/* �������ͼ���ROI��������FEC mesh���вü�, �õ���������ĳߴ� */
bool cropFecMesh(FecParams &fecParams, RoiParams &roiParams, int level, unsigned short *pCropMeshXI, unsigned char *pCropMeshXF, unsigned short *pCropMeshYI, unsigned char *pCropMeshYF);

/* �������ͼ���ROI��������LDCH mesh���вü�, �õ���������ĳߴ� */
bool cropLdchMesh(LdchParams &ldchParams, RoiParams &roiParams, int level, unsigned short *pMeshX, unsigned short *pRoiMeshX);

/* FEC: ��ʼ��, ����8k ---> 2��4k */
void genFecMeshInit8kTo4k(int srcW, int srcH, int dstW, int dstH, int margin,
	CameraCoeff &camCoeff, CameraCoeff &camCoeff_left, CameraCoeff &camCoeff_right,
	FecParams &fecParams, FecParams &fecParams_left, FecParams &fecParams_right);

/* LDCH: 8k ---> 2��4k */
void genLdchMeshInit8kTo4k(int srcW, int srcH, int dstW, int dstH, int margin,
	CameraCoeff &camCoeff, CameraCoeff &camCoeff_left, CameraCoeff &camCoeff_right,
	LdchParams &ldchParams, LdchParams &ldchParams_left, LdchParams &ldchParams_right);

#endif // !__GENMESH_H__
