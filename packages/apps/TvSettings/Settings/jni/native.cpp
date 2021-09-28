/*
 * Copyright (C) 2008 The Android Open Source Project
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

#define LOG_TAG "tvsettings.native.cpp"
#include <utils/Log.h>
#include <stdio.h>
#include "jni.h"
#include <math.h>
#include "TVInfo.h"
#include "Vop.h"
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define ROUND(a) (int)(a + 0.5)
static int bt1886eotf(int *segYn, double maxLumi, double minLumi)
{

	double Lw, Lb;
	double r = 2.4;
	double a, b;

	double xIndex;
	double yIndex;

	unsigned char i;
	int xBitmask, yBitmask;
	int segXn[65] = {0,
					 512, 1024, 1536, 2048, 2560, 3072, 3584, 4096,
					 4608, 5120, 5632, 6144, 6656, 7168, 7680, 8192,
					 8704, 9216, 9728, 10240, 10496, 10752, 11008, 11264,
					 11520, 11776, 12032, 12288, 12544, 12800, 13056, 13312,
					 13440, 13568, 13696, 13824, 13952, 14080, 14208, 14336,
					 14464, 14592, 14720, 14848, 14976, 15104, 15232, 15360,
					 15424, 15488, 15552, 15616, 15680, 15744, 15808, 15872,
					 15936, 16000, 16064, 16128, 16192, 16256, 16320, 16383};

	Lw = maxLumi / 10000;
	Lb = minLumi / 10000;

	a = pow((pow(Lw, 1 / r) - pow(Lb, 1 / r)), r);
	b = pow(Lb, 1 / r) / (pow(Lw, 1 / r) - pow(Lb, 1 / r));

	xBitmask = 16383;
	yBitmask = 262143;
	for (i = 0; i < 65; i++)
	{
		xIndex = segXn[i] * 1.0 / xBitmask;
		xIndex = MAX(xIndex + b, 0);
		yIndex = a * pow(xIndex, r) * yBitmask;
		segYn[i] = ROUND(yIndex);
	}
	return 0;
}

static int st2084oetf(int *segYn, double coef, double nFac)
{
	int segXn[65] = {0,
					 1, 2, 4, 8, 16, 24, 32, 64,
					 96, 128, 256, 384, 512, 640, 768, 896,
					 1024, 1280, 1536, 1792, 2048, 2304, 2560, 2816,
					 3072, 3584, 4096, 4608, 5120, 6144, 7168, 8192,
					 9216, 10240, 11264, 12288, 14336, 16384, 18432, 20480,
					 22528, 24576, 26624, 28672, 30720, 32768, 36864, 40960,
					 45056, 49152, 53248, 57344, 61440, 65536, 73728, 81920,
					 90112, 98304, 114688, 131072, 163840, 196608, 229376, 262143};

	int xBitmask, yBitmask;
	unsigned char i;

	double xIndex, yIndex;
	double c1, c2, c3;
	double m, n;

	c1 = 3424 * 1.0 / 4096;
	c2 = 2413 * 1.0 / 4096 * 32;
	c3 = 2392 * 1.0 / 4096 * 32;
	m = 2523 * 1.0 / 4096 * 128;
	n = 2610 * 1.0 / 4096 * (1.0 / nFac);

	xBitmask = 262143;
	yBitmask = 16383;

	for (i = 0; i < 65; i++)
	{
		xIndex = segXn[i] * 1.0 / xBitmask;
		yIndex = coef * pow((c1 + c2 * pow(xIndex, n)) / (1 + c3 * pow(xIndex, n)), m);
		yIndex = ROUND(yIndex * yBitmask);
		segYn[i] = MIN(yIndex, yBitmask);
	}
	return 0;
}

static jintArray get(JNIEnv *env, jobject thiz, jdouble x, jdouble y)
{
	ALOGI("%lf : %lf", x, y);
	(void)thiz;
	jintArray intArray = env->NewIntArray(65);
	jint *result = new jint[65];
	bt1886eotf(result, x, y);
	env->SetIntArrayRegion(intArray, 0, 65, result);
	delete[] result;
	return intArray;
}

static jintArray getOther(JNIEnv *env, jobject thiz, jdouble x, jdouble y)
{
	ALOGI("%lf : %lf", x, y);
	(void)thiz;
	jintArray intArray = env->NewIntArray(65);
	jint *result = new jint[65];
	st2084oetf(result, x, y);
	env->SetIntArrayRegion(intArray, 0, 65, result);
	delete[] result;
	return intArray;
}

static jboolean isSupportHDR(JNIEnv *env, jobject thiz)
{
	(void)*env;
	(void)thiz;
	int supportType = HdmiSupportedDataSpace();
	ALOGI("%d", HdmiSupportedDataSpace());
	return (supportType & HAL_DATASPACE_TRANSFER_ST2084) != 0;
}

static void setHDREnable(JNIEnv *env, jobject thiz, jint enable)
{
	(void)*env;
	(void)thiz;
	ALOGI("setHDREnable");
	setHdmiHDR(enable);
}

static jintArray getEetf(JNIEnv *env, jobject thiz, jfloat maxDst, jfloat minDst)
{
	(void)thiz;
	ALOGI("%f : %f", maxDst, minDst);
	jintArray array = env->NewIntArray(33);
	jint *result = new jint[33];
	makeHDR2SDREETF(1200, 0.02, maxDst, minDst, result);
	env->SetIntArrayRegion(array, 0, 33, result);
	delete[] result;
	return array;
}

static jintArray getOetf(JNIEnv *env, jobject thiz, jfloat maxDst, jfloat minDst)
{
	(void)thiz;
	ALOGI("%f : %f", maxDst, minDst);
	jintArray array = env->NewIntArray(33);
	jint *result = new jint[33];
	makeHDR2SDROETF(maxDst, minDst, result);
	env->SetIntArrayRegion(array, 0, 33, result);
	delete[] result;
	return array;
}

static jintArray getMaxMin(JNIEnv *env, jobject thiz, jfloat maxDst, jfloat minDst)
{
	(void)thiz;
	ALOGI("%f : %f", maxDst, minDst);
	jintArray array = env->NewIntArray(2);
	jint *result = new jint[2];
	makeMaxMin(1200, 0.02, maxDst, minDst, result);
	env->SetIntArrayRegion(array, 0, 2, result);
	delete[] result;
	return array;
}

static const char *classPathName = "com/android/tv/settings/util/JniCall";

static JNINativeMethod methods[] = {
	{"get", "(DD)[I", (void *)get},
	{"getOther", "(DD)[I", (void *)getOther},
	{"isSupportHDR", "()Z", (void *)isSupportHDR},
	{"setHDREnable", "(I)V", (void *)setHDREnable},
	{"getEetf", "(FF)[I", (void *)getEetf},
	{"getOetf", "(FF)[I", (void *)getOetf},
	{"getMaxMin", "(FF)[I", (void *)getMaxMin},
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv *env, const char *className,
								 JNINativeMethod *gMethods, int numMethods)
{
	jclass clazz;

	clazz = env->FindClass(className);
	if (clazz == NULL)
	{
		ALOGE("Native registration unable to find class '%s'", className);
		return JNI_FALSE;
	}
	if (env->RegisterNatives(clazz, gMethods, numMethods) < 0)
	{
		ALOGE("RegisterNatives failed for '%s'", className);
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 *
 * returns JNI_TRUE on success.
 */
static int registerNatives(JNIEnv *env)
{
	if (!registerNativeMethods(env, classPathName,
							   methods, sizeof(methods) / sizeof(methods[0])))
	{
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

// ----------------------------------------------------------------------------

/*
 * This is called by the VM when the shared library is first loaded.
 */

typedef union {
	JNIEnv *env;
	void *venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	(void)reserved;
	UnionJNIEnvToVoid uenv;
	uenv.venv = NULL;
	jint result = -1;
	JNIEnv *env = NULL;

	ALOGI("JNI_OnLoad");

	if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
	{
		ALOGE("ERROR: GetEnv failed");
		goto bail;
	}
	env = uenv.env;

	if (registerNatives(env) != JNI_TRUE)
	{
		ALOGE("ERROR: registerNatives failed");
		goto bail;
	}

	result = JNI_VERSION_1_4;

bail:
	return result;
}
