
/* 8kͼ--->2��4kͼ������LDCH�ּ�У�� */
void test030()
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
	LdchParams ldchParams;
	ldchParams.saveMaxFovX = 0;											/* ����ˮƽx�������FOV: 1������, 0�������� */
	ldchParams.isLdchOld = 1;											/* �Ƿ�ɰ�LDCH: 1�����ǣ�0������ */
	ldchParams.saveMeshX = 1;											/* �Ƿ񱣴�MeshX.bin�ļ�: 1������, 0�������� */
	sprintf(ldchParams.meshPath, "../data_out/");						/* ����MeshX.bin�ļ��ĸ�Ŀ¼ */
	/* ��ͼ4k��ز��� */
	CameraCoeff camCoeff_left;
	LdchParams ldchParams_left;
	/* ��ͼ4k��ز��� */
	CameraCoeff camCoeff_right;
	LdchParams ldchParams_right;
	/* LDCH��ز�����ʼ�� */
	genLdchMeshInit8kTo4k(srcW, srcH, dstW, dstH, margin, camCoeff, camCoeff_left, camCoeff_right, ldchParams, ldchParams_left, ldchParams_right);
	/* ӳ���buffer���� */
	unsigned short *pMeshX = new unsigned short[ldchParams.meshSize];							/* ȫͼ */
	unsigned short *pMeshX_left = new unsigned short[ldchParams_left.meshSize];					/* ��ͼ */
	unsigned short *pMeshX_right = new unsigned short[ldchParams_right.meshSize];				/* ��ͼ */

	/* �������ͼ��buffer���� */
	/* ȫͼ */
	unsigned long srcSize = (srcW * srcH) > (dstW * dstH) ? (srcW * srcH) : (dstW * dstH);
	unsigned short *pImgIn = new unsigned short[srcSize * 3];
	unsigned short *pImgOut = new unsigned short[srcSize * 3];
	/* ��ͼ */
	int srcW_left = srcW * 0.5 + margin;
	int srcH_left = srcH;
	int dstW_left = srcW * 0.5 + margin;
	int dstH_left = srcH;
	unsigned long srcSize_left = (srcW_left * srcH_left) > (dstW_left * dstH_left) ? (srcW_left * srcH_left) : (dstW_left * dstH_left);
	unsigned short *pImgIn_left = new unsigned short[srcSize_left * 3];
	unsigned short *pImgOut_left = new unsigned short[srcSize_left * 3];
	/* ��ͼ */
	int srcW_right = srcW * 0.5 + margin;
	int srcH_right = srcH;
	int dstW_right = srcW * 0.5 + margin;
	int dstH_right = srcH;
	unsigned long srcSize_right = (srcW_right * srcH_right) > (dstW_right * dstH_right) ? (srcW_right * srcH_right) : (dstW_right * dstH_right);
	unsigned short *pImgIn_right = new unsigned short[srcSize_right * 3];
	unsigned short *pImgOut_right = new unsigned short[srcSize_right * 3];

	/* ��ȡ����ͼ�� */
	char srcBGRPath[256] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_full_7680x4320_08.bmp";
	readRGBforLDCH(srcBGRPath, srcW, srcH, pImgIn);
	char srcBGRPath_left[256] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_left_4096x4320_08.bmp";
	readRGBforLDCH(srcBGRPath_left, srcW_left, srcH_left, pImgIn_left);
	char srcBGRPath_right[256] = "../data_in/image/group_023_imx415_2.8mm_7680x4320_half/imx415_2.8mm_right_4096x4320_08.bmp";
	readRGBforLDCH(srcBGRPath_right, srcW_right, srcH_right, pImgIn_right);

	/* ����LDCHӳ�������У�� */
	int level = 0;				/* level��Χ: 0-255 */
	int levelValue[] = { 0,64,128,192,255 };
	//for (level = 0; level <= 255; level = level + 1)
	for (int levelIdx = 0; levelIdx < 5; ++levelIdx)
	{
		level = levelValue[levelIdx];
		printf("level = %d\n", level);

		/* ��ͼ����LDCHУ�� */
		bool success_left = genLDCMeshNLevel(ldchParams_left, camCoeff_left, level, pMeshX_left);					/* ���ɶ�ӦУ��level��LDCHӳ��� */
		LDCH_Cmodel(dstW_left, dstH_left, pImgIn_left, pImgOut_left, pMeshX_left);									/* ����LDCH_Cmodel */
		cv::Mat dstImgBGR_left;
		ldchOut2Mat(dstW_left, dstH_left, 8, pImgOut_left, dstImgBGR_left);											/* LDCH������תΪcv::Mat��ʽ */
		char dstBmpPath_left[256];																					/* ���� */
		sprintf(dstBmpPath_left, "../data_out/ldch_left_%dx%d_level%03d.bmp", dstW_left, dstH_left, level);
		cv::imwrite(dstBmpPath_left, dstImgBGR_left);

		/* ��ͼ����LDCHУ�� */
		bool success_right = genLDCMeshNLevel(ldchParams_right, camCoeff_right, level, pMeshX_right);				/* ���ɶ�ӦУ��level��LDCHӳ��� */
		LDCH_Cmodel(dstW_right, dstH_right, pImgIn_right, pImgOut_right, pMeshX_right);								/* ����LDCH_Cmodel */
		cv::Mat dstImgBGR_right;
		ldchOut2Mat(dstW_right, dstH_right, 8, pImgOut_right, dstImgBGR_right);										/* LDCH������תΪcv::Mat��ʽ */
		char dstBmpPath_right[256];																					/* ���� */
		sprintf(dstBmpPath_right, "../data_out/ldch_right_%dx%d_level%03d.bmp", dstW_right, dstH_right, level);
		cv::imwrite(dstBmpPath_right, dstImgBGR_right);

		/* ��֤: ȫͼ����LDCHУ�� */
		bool success_full = genLDCMeshNLevel(ldchParams, camCoeff, level, pMeshX);									/* ���ɶ�ӦУ��level��LDCHӳ��� */
		LDCH_Cmodel(dstW, dstH, pImgIn, pImgOut, pMeshX);															/* ����LDCH_Cmodel */
		cv::Mat dstImgBGR_full;
		ldchOut2Mat(dstW, dstH, 8, pImgOut, dstImgBGR_full);														/* LDCH������תΪcv::Mat��ʽ */
		char dstBmpPath_full[256];																					/* ���� */
		sprintf(dstBmpPath_full, "../data_out/ldch_full_%dx%d_level%03d.bmp", dstW, dstH, level);
		cv::imwrite(dstBmpPath_full, dstImgBGR_full);

		/* ��֤: ��ͼFEC��� + ��ͼFEC��� ---> ƴ�ӵ�ȫͼ��� */
		cv::Mat dstImgBGR_stitch = cv::Mat(dstH, dstW, CV_8UC3);
		dstImgBGR_left(cv::Range(0, dstH_left), cv::Range(0, dstW_left - margin)).copyTo(dstImgBGR_stitch(cv::Range(0, dstH), cv::Range(0, dstW * 0.5)));
		dstImgBGR_right(cv::Range(0, dstH_left), cv::Range(margin, dstW_right)).copyTo(dstImgBGR_stitch(cv::Range(0, dstH), cv::Range(dstW * 0.5, dstW)));
		char dstBmpPath_stitch[256];
		sprintf(dstBmpPath_stitch, "../data_out/ldch_stitch_%dx%d_level%03d.bmp", dstW, dstH, level);
		cv::imwrite(dstBmpPath_stitch, dstImgBGR_stitch);
	}
	/* ȫͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgIn;						/* �ڴ��ͷ� */
	delete[] pImgOut;
	delete[] pMeshX;
	genLdchMeshDeInit(ldchParams);			/* ����ʼ�� */

	/* ��ͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgIn_left;					/* �ڴ��ͷ� */
	delete[] pImgOut_left;
	delete[] pMeshX_left;
	genLdchMeshDeInit(ldchParams_left);		/* ����ʼ�� */

	/* ��ͼ����ڴ��ͷźͷ���ʼ�� */
	delete[] pImgIn_right;					/* �ڴ��ͷ� */
	delete[] pImgOut_right;
	delete[] pMeshX_right;
	genLdchMeshDeInit(ldchParams_right);	/* ����ʼ�� */
}