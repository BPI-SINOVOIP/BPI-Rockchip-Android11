/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#include "hwc_util.h"
#define LOG_TAG "hwcomposer-util"

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <cutils/properties.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static int sysfs_read(const char *path)
{
    char buf[80];
    char freq[50];
    int len;
    int fd = open(path, O_RDONLY);

    ALOGV("%s: [%s:]", __FUNCTION__, path);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: [%s]", __FUNCTION__, path);
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1;
    }

    len = read(fd, freq, 10);
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: [%s]", __FUNCTION__, path);
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);

    return atoi(freq)/1000;
}

static void sysfs_write(const char *path,const char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    ALOGV("%s: [%s: %s]", __FUNCTION__, path, s);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: [%s: %s]", __FUNCTION__, path, s);
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: [%s: %s]", __FUNCTION__, path, s);
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

/*
 * set cpu0 scaling_min_freq
 * Parameters:
 *  freq:
 *      unit is M.
 * Return:
 *      original freq.
 */
int set_cpu_min_freq(int freq)
{
    int set_freq = 1000 * freq;
    int old_freq;
    char freq_buf[50];
    old_freq = sysfs_read(CPU0_SCALING_MIN_FREQ);
    sprintf(freq_buf,"%d",set_freq);
    sysfs_write(CPU0_SCALING_MIN_FREQ, (const char *)freq_buf);
#if (defined TARGET_BOARD_PLATFORM_RK3399) || (defined TARGET_BOARD_PLATFORM_RK3368)
    sysfs_write(CPU4_SCALING_MIN_FREQ, (const char *)freq_buf);
#endif
    ALOGV("%s: change min freq %d==>%d",__FUNCTION__,old_freq,freq);
    return old_freq;
}

/*
 * Control gpu performance mode.
 * Parameters:
 *  on:
 *      1: open performance
 *      0: close performance
 */
void ctl_gpu_performance(int on)
{
    if(on != 0 && on != 1)
    {
        ALOGE("%s: invalid parameters,on=%d", __FUNCTION__, on);
        return;
    }

    if(strcmp(GPU_GOV_PATH,""))
    {
        sysfs_write(GPU_GOV_PATH, on ? "performance" : "simple_ondemand");
    }
    else
    {
        ALOGV("%s: GPU_GOV_PATH is NULL",__FUNCTION__);
    }
}


/*
 * Control cpu performance mode.
 * Parameters:
 *  on:
 *      1: open performance
 *      0: close performance
 *  type:
 *      1: big cpu
 *      0: little cpu
 */
void ctl_cpu_performance(int on, int type)
{
    if((on != 0 && on != 1) ||
        (type != 0 && type != 1))
    {
        ALOGE("%s: invalid parameters,on=%d,type=%d", __FUNCTION__, on, type);
        return;
    }

    if(type)
    {
        sysfs_write(CPU_CLUST1_GOV_PATH, on ? "performance" : "interactive");
    }
    else
    {
        sysfs_write(CPU_CLUST0_GOV_PATH, on ? "performance" : "interactive");
    }
}

/*
 * Control little cpu.
 * Parameters:
 *  on:
 *      1: Enable little cpu
 *      0: Disable little cpu
 */
void ctl_little_cpu(int on)
{
    if(on != 0 && on != 1)
    {
        ALOGE("%s: invalid parameters,on=%d", __FUNCTION__, on);
        return;
    }

    sysfs_write("/sys/devices/system/cpu/cpu0/online", on ? "1" : "0");
    sysfs_write("/sys/devices/system/cpu/cpu1/online", on ? "1" : "0");
    sysfs_write("/sys/devices/system/cpu/cpu2/online", on ? "1" : "0");
    sysfs_write("/sys/devices/system/cpu/cpu3/online", on ? "1" : "0");
}

int hwc_get_int_property(const char* pcProperty,const char* default_value)
{
    char value[PROPERTY_VALUE_MAX];
    int new_value = 0;

    if(pcProperty == NULL || default_value == NULL)
    {
        ALOGE("hwc_get_int_property: invalid param");
        return -1;
    }

    property_get(pcProperty, value, default_value);
    new_value = atoi(value);

    return new_value;
}

bool hwc_get_bool_property(const char* pcProperty,const char* default_value)
{
    char value[PROPERTY_VALUE_MAX];
    bool result = false;

    if(pcProperty == NULL || default_value == NULL)
    {
        ALOGE("hwc_get_int_property: invalid param");
        return -1;
    }

    property_get(pcProperty, value, default_value);
    if(!strcmp(value,"true"))
        result = true;
    else
        result = false;

    return result;
}


int hwc_get_string_property(const char* pcProperty,const char* default_value,char* retult)
{
    if(pcProperty == NULL || default_value == NULL || retult == NULL)
    {
        ALOGE("hwc_get_string_property: invalid param");
        return -1;
    }

    property_get(pcProperty, retult, default_value);

    return 0;
}



static int CompareLines(int *da,int w)
{
    int i,j;
    for(i = 0;i<1;i++) // compare 4 lins
    {
        for(j= 0;j<w;j+=8)
        {
            if((unsigned int)*da != 0xff000000 && (unsigned int)*da != 0x0)
            {
                return 1;
            }
            da +=8;

        }
    }
    return 0;
}

int DetectValidData(int *data,int w,int h)
{
    int i;
    int *da;
    /*  detect model
    -------------------------
    |   |   |    |    |      |
    |   |   |    |    |      |
    |------------------------|
    |   |   |    |    |      |
    |   |   |    |    |      |
    |   |   |    |    |      |
    |------------------------|
    |   |   |    |    |      |
    |   |   |    |    |      |
    |------------------------|
    |   |   |    |    |      |
    |   |   |    |    |      |
    |------------------------|
    |   |   |    |    |      |
    --------------------------
    */
    if(data == NULL)
        return 1;
    for(i = 2; i<h; i+= 8)
    {
        da = data +  i *w;
        if(CompareLines(da,w))
            return 1;
    }

    return 0;
}

#if RK_CTS_WORKROUND
static bool ConvertCharToData(const char *pszHintName, const char *pszData, void *pReturn, IMG_DATA_TYPE eDataType)
{
	bool bFound = false;


	switch(eDataType)
	{
		case IMG_STRING_TYPE:
		{
			strcpy((char*)pReturn, pszData);

			ALOGD_IF(RK_CTS_DEBUG, "Hint: Setting %s to %s\n", pszHintName, (char*)pReturn);

			bFound = true;

			break;
		}
		case IMG_FLOAT_TYPE:
		{
			*(float*)pReturn = (float) atof(pszData);

			ALOGD_IF(RK_CTS_DEBUG, "Hint: Setting %s to %f", pszHintName, *(float*)pReturn);

			bFound = true;

			break;
		}
		case IMG_UINT_TYPE:
		case IMG_FLAG_TYPE:
		{
			/* Changed from atoi to stroul to support hexadecimal numbers */
			*(u32*)pReturn = (u32) strtoul(pszData, NULL, 0);
			if (*(u32*)pReturn > 9)
			{
				ALOGD_IF(RK_CTS_DEBUG, "Hint: Setting %s to %u (0x%X)", pszHintName, *(u32*)pReturn, *(u32*)pReturn);
			}
			else
			{
				ALOGD_IF(RK_CTS_DEBUG, "Hint: Setting %s to %u", pszHintName, *(u32*)pReturn);
			}
			bFound = true;

			break;
		}
		case IMG_INT_TYPE:
		{
			*(int*)pReturn = (int) atoi(pszData);

			ALOGD_IF(RK_CTS_DEBUG, "Hint: Setting %s to %d\n", pszHintName, *(int*)pReturn);

			bFound = true;

			break;
		}
		default:
		{
			ALOGD_IF(RK_CTS_DEBUG, "ConvertCharToData: Bad eDataType");

			break;
		}
	}

	return bFound;
}

static int getProcessCmdLine(char* outBuf, size_t bufSize)
{
	int ret = 0;

	FILE* file = NULL;
	long pid = 0;
	char procPath[128]={0};

	pid = getpid();
	sprintf(procPath, "/proc/%ld/cmdline", pid);

	file = fopen(procPath, "r");
	if ( NULL == file )
	{
		ALOGE("fail to open file (%s)",strerror(errno));
	}

	if ( NULL == fgets(outBuf, bufSize - 1, file) )
	{
		ALOGE("fail to read from cmdline_file.");
	}

	if ( NULL != file )
	{
		fclose(file);
	}

	return ret;
}

bool FindAppHintInFile(FILE *regFile, const char *pszAppName,
								  const char *pszHintName, void *pReturn,
								  IMG_DATA_TYPE eDataType)
{
	bool bFound = false;

	if(regFile)
	{
		char pszTemp[1024], pszApplicationSectionName[1024];
		int iLineNumber;
		bool bUseThisSection, bInAppSpecificSection;

        fseek(regFile, 0, SEEK_SET);
		/* Build the section name */
		snprintf(pszApplicationSectionName, 1024, "[%s]", pszAppName);

		bUseThisSection 		= false;
		bInAppSpecificSection	= false;

		iLineNumber = -1;

		while(fgets(pszTemp, 1024, regFile))
		{
			size_t uiStrLen;

			iLineNumber++;
			ALOGD_IF(RK_CTS_DEBUG, "FindAppHintInFile iLineNumber=%d pszTemp=%s",iLineNumber,pszTemp);

			uiStrLen = strlen(pszTemp);

			if (pszTemp[uiStrLen-1]!='\n')
			{
			    ALOGE("FindAppHintInFile : Error at line %u",iLineNumber);

				continue;
			}

			if((uiStrLen >= 2) && (pszTemp[uiStrLen-2] == '\r'))
			{
				/* CRLF (Windows) line ending */
				pszTemp[uiStrLen-2] = '\0';
			}
			else
			{
				/* LF (unix) line ending */
				pszTemp[uiStrLen-1] = '\0';
			}

			switch (pszTemp[0])
			{
				case '[':
				{
					/* Section */
					bUseThisSection 		= false;
					bInAppSpecificSection	= false;

					if (!strcmp("[default]", pszTemp))
					{
						bUseThisSection = true;
					}
					else if (!strcmp(pszApplicationSectionName, pszTemp))
					{
						bUseThisSection 		= true;
						bInAppSpecificSection 	= true;
					}

					break;
				}
				default:
				{
					char *pszPos;

					if (!bUseThisSection)
					{
						/* This line isn't for us */
						continue;
					}

					pszPos = strstr(pszTemp, pszHintName);

					if (pszPos!=pszTemp)
					{
						/* Hint name isn't at start of string */
						continue;
					}

					if (*(pszPos + strlen(pszHintName)) != '=')
					{
						/* Hint name isn't exactly correct, or isn't followed by an equals sign */
						continue;
					}

					/* Move to after the equals sign */
					pszPos += strlen(pszHintName) + 1;

					/* Convert anything after the equals sign to the requested data type */
					bFound = ConvertCharToData(pszHintName, pszPos, pReturn, eDataType);

					if (bFound && bInAppSpecificSection)
					{
						/*
						// If we've found the hint in the application specific section we may
						// as well drop out now, since this should override any default setting
						*/
						//fclose(regFile);

						return true;
					}

					break;
				}
			}
		}

		//fclose(regFile);
	}
	else
	{
		ALOGE("%s regFile is null",__FUNCTION__);
	}

	return bFound;
}
#endif


