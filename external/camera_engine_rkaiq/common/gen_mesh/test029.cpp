
/* 8kͼ--->2��4kͼ������FEC�ּ�У�� */
void test029()
{
	int srcW = 7680;
	int srcH = 4320;
	int dstW = 7680;
	int dstH = 4320;
	int margin = 256;

	/* ȫͼ8k��ز��� */
	CameraCoeff camCoeff;
	camCoeff.a0 = -4628.92834904855135391699150204658508300781250000000000;
	camCoeff.a2 = 0.00008439805632153267055031026222522427815420087427;
	camCoeff.a3 = -0.00000000947972529654520536345924537060744774485954;
	camCoeff.a4 = 0.00000000000276046059610868196196561020719728129884;
	camCoeff.cx = (srcW - 1.0) * 0.5;
	camCoeff.cy = (srcH - 1.0) * 0.5;
	FecParams fecParams;
	fecParams.correctX = 1;								/* ˮƽx����У��: 1����У��, 0����У�� */
	fecParams.correctY = 1;								/* ��ֱy����У��: 1����У��, 0����У�� */
	fecParams.saveMaxFovX = 1;							/* ����ˮƽx�������FOV: 1������, 0�������� */
	fecParams.isFecOld = 0;								/* �Ƿ�ɰ�FEC: 1�����ǣ�0������ */
	fecParams.saveMesh4bin = 0;							/* �Ƿ񱣴�meshxi,xf,yi,yf4��bin�ļ�: 1������, 0�������� */
	sprintf(fecParams.mesh4binPath, "../data_out/");	/* ����meshxi,xf,yi,yf4��bin�ļ��ĸ�Ŀ¼ */

	/* ��ͼ4k��ز��� */
	CameraCoeff camCoeff_left;
	FecParams fecParams_left;
	/* ��ͼ4k��ز��� */
	CameraCoeff camCoeff_right;
	FecParams fecParams_right;
	/* LDCH��ز�����ʼ�� */
	genFecMeshInit8kTo4k(srcW, srcH, dstW, dstH, margin, camCoeff, camCoeff_left, camCoeff_right, fecParams, fecParams_left, fecParams_right);
	/* ӳ���buffer���� */
	unsigned short	*pMeshXI, *pMeshYI;																				/* X, Y�������� */
	unsigned char	*pMeshXF, *pMeshYF;																				/* X, YС������ */
	mallocFecMesh(fecParams.meshSize4bin, &pMeshXI, &pMeshXF, &pMeshYI, &pMeshYF);									/* ȫͼFECӳ���buffer���� */
	unsigned short	*pMeshXI_left, *pMeshYI_left;																	/* X, Y�������� */
	unsigned char	*pMeshXF_left, *pMeshYF_left;																	/* X, YС������ */
	mallocFecMesh(fecParams_left.meshSize4bin, &pMeshXI_left, &pMeshXF_left, &pMeshYI_left, &pMeshYF_left);			/* ��ͼFECӳ���buffer���� */
	unsigned short	*pMeshXI_right, *pMeshYI_right;																	/* X, Y�������� */
	unsigned char	*pMeshXF_right, *pMeshYF_right;																	/* X, YС������ */
	mallocFecMesh(fecParams_right.meshSize4bin, &pMeshXI_right, &pMeshXF_right, &pMeshYI_right, &pMeshYF_right);	/* ��ͼFECӳ���buffer���� */

	/* �������ͼ��buffer���� */
	/* ȫͼ */
	unsigned long srcSize = (srcW * srcH) > (dstW * dstH) ? (srcW * srcH) : (dstW * dstH);
	unsigned char *pImgY = new unsigned char[srcSize];
	unsigned char *pImgUV = new unsigned char[srcSize];
	unsigned char *pImgOut = new unsigned char[srcSize * 2];
	/* ��ͼ */
	int srcW_left = srcW * 0.5 + margin;
	int srcH_left = srcH;
	int dstW_left = srcW * 0.5 + margin;
	int dstH_left = srcH;
	unsigned long srcSize_left = (srcW_left * srcH_left) > (dstW_left * dstH_left) ? (srcW_left * srcH_left) : (dstW_left * dstH_left);
	unsigned char *pImgY_left = new unsigned char[srcSize_left];
	unsigned char *pImgUV_left = new unsigned char[srcSize_left];
	unsigned char *pImgOut_left = new unsigned char[srcSize_left * 2];
	/* ��ͼ */
	int srcW_right = srcW * 0.5 + margin;
	int srcH_right = srcH;
	int dstW_right = srcW * 0.5 + margin;
	int dstH_right = srcH;
	unsigned long srcSize_right = (srcW_right * srcH_right) > (dstW_right * dstH_right) ? (srcW_right * srcH_right) : (dstW_right * dstH_right);
	unsigned char *pImgY_right = new unsigned char[srcSize_right];
	unsigned char *pImgUV_right = new unsigned char[srcSize_right];
	unsigned char *pImgOut_right = new unsigned char[srcSize_right * 2];

	/* ��ȡ����ͼ�� */
	char srcYuvPath[512] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_full_7680x4320_08.nv12";
	readYUV(srcYuvPath, srcW, srcH, 0, NULL, pImgY, pImgUV);
	char srcYuvPath_left[512] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_left_4096x4320_08.nv12";
	readYUV(srcYuvPath_left, srcW_left, srcH_left, 0, NULL, pImgY_left, pImgUV_left);
	char srcYuvPath_right[512] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_right_4096x4320_08.nv12";
	readYUV(srcYuvPath_right, srcW_right, srcH_right, 0, NULL, pImgY_right, pImgUV_right);

	/* ����FECӳ�������У�� */
	int level = 0;					/* level��Χ: 0-255 */
	int levelValue[] = { 0,64,128,192,255 };
	//for (level = 0; level <= 0; level = level + 1)
	for (int levelIdx = 0; levelIdx < 5; ++levelIdx)
	{
		level = levelValue[levelIdx];
		printf("level = %d\n", level);

		/* ��ͼ����FECУ�� */
		bool success_left = genFECMeshNLevel(fecParams_left, camCoeff_left, level, pMeshXI_left, pMeshXF_left, pMeshYI_left, pMeshYF_left);				/* ���ɶ�ӦУ��level��FECӳ��� */
		FEC_Cmodel_4bin(srcW_left, srcH_left, dstW_left, dstH_left, pImgY_left, pImgUV_left,
			pMeshXI_left, pMeshXF_left, pMeshYI_left, pMeshYF_left, pImgOut_left, 0, 0, 0, 0, 0, 0);													/* ����FEC */
		cv::Mat dstImgBGR_left;																															/* ���� */
		NV12toRGB(pImgOut_left, dstW_left, dstH_left, dstImgBGR_left);
		char dstBmpPath_left[256];
		sprintf(dstBmpPath_left, "../data_out/fec_left_%dx%d_level%03d.bmp", dstW_left, dstH_left, level);
		cv::imwrite(dstBmpPath_left, dstImgBGR_left);

		/* ��ͼ����FECУ�� */
		bool success_right = genFECMeshNLevel(fecParams_right, camCoeff_right, level, pMeshXI_right, pMeshXF_right, pMeshYI_right, pMeshYF_right);		/* ���ɶ�ӦУ��level��FECӳ��� */
		FEC_Cmodel_4bin(srcW_right, srcH_right, dstW_right, dstH_right, pImgY_right, pImgUV_right,
			pMeshXI_right, pMeshXF_right, pMeshYI_right, pMeshYF_right, pImgOut_right, 0, 0, 0, 0, 0, 0);												/* ����FEC */
		cv::Mat dstImgBGR_right;																														/* ���� */
		NV12toRGB(pImgOut_right, dstW_right, dstH_right, dstImgBGR_right);
		char dstBmpPath_right[256];
		sprintf(dstBmpPath_right, "../data_out/fec_right_%dx%d_level%03d.bmp", dstW_right, dstH_right, level);
		cv::imwrite(dstBmpPath_right, dstImgBGR_right);

		/* ��֤: ȫͼ����FECУ�� */
		bool success_full = genFECMeshNLevel(fecParams, camCoeff, level, pMeshXI, pMeshXF, pMeshYI, pMeshYF);											/* ���ɶ�ӦУ��level��FECӳ��� */
		FEC_Cmodel_4bin(srcW, srcH, dstW, dstH, pImgY, pImgUV,
			pMeshXI, pMeshXF, pMeshYI, pMeshYF, pImgOut, 0, 0, 0, 0, 0, 0);																				/* ����FEC */
		cv::Mat dstImgBGR_full;																															/* ���� */
		NV12toRGB(pImgOut, dstW, dstH, dstImgBGR_full);
		char dstBmpPath_full[256];
		sprintf(dstBmpPath_full, "../data_out/fec_full_%dx%d_level%03d.bmp", dstW, dstH, level);
		cv::imwrite(dstBmpPath_full, dstImgBGR_full);

		/* ��֤: ��ͼFEC��� + ��ͼFEC��� ---> ƴ�ӵ�ȫͼ��� */
		cv::Mat dstImgBGR_stitch = cv::Mat(dstH, dstW, CV_8UC3);
		dstImgBGR_left(cv::Range(0, dstH_left), cv::Range(0, dstW_left - margin)).copyTo(dstImgBGR_stitch(cv::Range(0, dstH), cv::Range(0, dstW * 0.5)));
		dstImgBGR_right(cv::Range(0, dstH_left), cv::Range(margin, dstW_right)).copyTo(dstImgBGR_stitch(cv::Range(0, dstH), cv::Range(dstW * 0.5, dstW)));
		char dstBmpPath_stitch[256];
		sprintf(dstBmpPath_stitch, "../data_out/fec_stitch_%dx%d_level%03d.bmp", dstW, dstH, level);
		cv::imwrite(dstBmpPath_stitch, dstImgBGR_stitch);
	}
	/* ȫͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgY;
	delete[] pImgUV;
	delete[] pImgOut;
	freeFecMesh(pMeshXI, pMeshXF, pMeshYI, pMeshYF);							/* �ڴ��ͷ� */
	genFecMeshDeInit(fecParams);												/* ����ʼ�� */

	/* ��ͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgY_left;
	delete[] pImgUV_left;
	delete[] pImgOut_left;
	freeFecMesh(pMeshXI_left, pMeshXF_left, pMeshYI_left, pMeshYF_left);		/* �ڴ��ͷ� */
	genFecMeshDeInit(fecParams_left);											/* ����ʼ�� */

	/* ��ͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgY_right;
	delete[] pImgUV_right;
	delete[] pImgOut_right;
	freeFecMesh(pMeshXI_right, pMeshXF_right, pMeshYI_right, pMeshYF_right);	/* �ڴ��ͷ� */
	genFecMeshDeInit(fecParams_right);											/* ����ʼ�� */


}