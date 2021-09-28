/******************************************************************************
 *
 * $Id: FileIO.c 177 2010-02-26 03:01:09Z rikita $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2009 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under Software License
 * Agreement (SLA) executed between the Licensee and AKM.
 *
 * Use of the software by unauthorized third party, or use of the software
 * beyond the scope of the SLA is strictly prohibited.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#include "FileIO.h"


/*!
 Load parameters from file which is specified with #SETTING_FILE_NAME.
 This function reads data from a beginning of the file line by line, and 
 check parameter name sequentially. In otherword, this function depends on 
 the order of eache parameter described in the file.
 @return If function fails, the return value is 0. When function fails, the
 output is undefined. Therefore, parameters which are possibly overwritten
 by this function should be initialized again. If function succeeds, the
 return value is 1.
 @param[out] prms A pointer to #AK8975PRMS structure. Loaded parameter is 
 stored to the member of this structure.
 */
int16 LoadParameters(AK8975PRMS * prms)
{
	int16 tmp, ret;
	FILE *fp = NULL;
	
	//Open setting file for read.
	if ((fp = fopen(SETTING_FILE_NAME, "r")) == NULL) {
		DBGPRINT(DBG_LEVEL1, "%s Setting file open error.\n", __FUNCTION__);
		return 0;
	}
	
	ret = 1;
	// Load data to HDST, HO, HREF, THRE
	// Form 0
	tmp = 0;
	ret = ret && LoadInt16(fp, "HSUC_HDST_FORM0", &tmp);
	prms->HSUC_HDST[0] = (AKSC_HDST)tmp;
	ret = ret && LoadInt16vec(fp, "HSUC_HO_FORM0", &prms->HSUC_HO[0]);
	ret = ret && LoadInt16vec(fp, "HFLUCV_HREF_FORM0", &prms->HFLUCV_HREF[0]);
	
	// Form 1
	tmp = 0;
	ret = ret && LoadInt16(fp, "HSUC_HDST_FORM1", &tmp);
	prms->HSUC_HDST[1] = (AKSC_HDST)tmp;
	ret = ret && LoadInt16vec(fp, "HSUC_HO_FORM1", &prms->HSUC_HO[1]);
	ret = ret && LoadInt16vec(fp, "HFLUCV_HREF_FORM1", &prms->HFLUCV_HREF[1]);
	
	if (fclose(fp) != 0) {
		DBGPRINT(DBG_LEVEL1, "%s Setting file close error.\n", __FUNCTION__);
		ret = 0;
	}
	
	return ret;
}

/*!
 Load \c int16 type value from file. The name of parameter is specified with 
 \a lpKeyName. If the name matches the beginning of read line, the string
 after #DELIMITER will converted to \c int16 type value.
 When this function fails, the \a *val is not updated. 
 @return If function fails, the return value is 0. When function fails, the
 value @ val is not overwritten. If function succeeds, the return value is 1.
 @param[in] fp Pointer to \c FILE structure.
 @param[in] lpKeyName The name of parameter.
 @param[out] val Pointer to \c int16 type value. Upon successful completion 
 of this function, read value is copied to this variable.
 */
int16 LoadInt16(FILE * fp, const char *lpKeyName, int16 * val)
{
	int tmp = 0;
	char buf[64] = { '\0' };
	
	// ATTENTION! %ns should be modified according to the size of buf.
	if (fscanf(fp, "%63s" DELIMITER "%6d", buf, &tmp) != 2) {
		DBGPRINT(DBG_LEVEL1, "File read error.\n");
		return 0;
	}
	// Compare the read parameter name with given name.
	if (strncmp(buf, lpKeyName, sizeof(buf)) != 0) {
		DBGPRINT(DBG_LEVEL1, "%s read error.\n", lpKeyName);
		return 0;
	}
	*val = (int16) tmp;
	
	return 1;
}

/*!
 Load \c int16vec type value from file and convert it to int16vec type 
 structure. This function adds ".x", ".y" and ".z" to the last of parameter 
 name and try to read value with combined name.
 @return If function fails, the return value is 0. When function fails, the
 output is undefined. If function succeeds, the return value is 1.
 @param[in] fp A opened \c FILE pointer.
 @param[in] lpKeyName The parameter name.
 @param[out] vec A pointer to int16vec structure. Upon successful completion
 of this function, read values are copied to this variable.
 */
int16 LoadInt16vec(FILE * fp, const char *lpKeyName, int16vec * vec)
{
	char keyName[64];
	int16 ret = 1;
	
	snprintf(keyName, sizeof(keyName), "%s.x", lpKeyName);
	ret = ret && LoadInt16(fp, keyName, &vec->u.x);
	
	snprintf(keyName, sizeof(keyName), "%s.y", lpKeyName);
	ret = ret && LoadInt16(fp, keyName, &vec->u.y);
	
	snprintf(keyName, sizeof(keyName), "%s.z", lpKeyName);
	ret = ret && LoadInt16(fp, keyName, &vec->u.z);
	
	return ret;
}


/*!
 Save parameters to file which is specified with #SETTING_FILE_NAME. This 
 function saves variables when the offsets of magnetic sensor estimated 
 successfully.
 @return If function fails, the return value is 0. When function fails, the
 parameter file may collapsed. Therefore, the parameters file should be 
 discarded. If function succeeds, the return value is 1.
 @param[out] prms A pointer to #AK8975PRMS structure. Member variables are 
 saved to the parameter file.
 */
int16 SaveParameters(AK8975PRMS * prms)
{
	int16 ret = 0;
	FILE *fp;
	
	//Open setting file for write.
	if ((fp = fopen(SETTING_FILE_NAME, "w")) == NULL) {
		DBGPRINT(DBG_LEVEL1, "%s Setting file open error.\n", __FUNCTION__);
		return 0;
	}

	// Form 0
	ret = 1;
	ret = ret && SaveInt16(fp, "HSUC_HDST_FORM0", (int16)prms->HSUC_HDST[0]);
	ret = ret && SaveInt16vec(fp, "HSUC_HO_FORM0", &prms->HSUC_HO[0]);
	ret = ret && SaveInt16vec(fp, "HFLUCV_HREF_FORM0", &prms->HFLUCV_HREF[0]);
	
	// Form 1
	ret = ret && SaveInt16(fp, "HSUC_HDST_FORM1", (int16)prms->HSUC_HDST[1]);
	ret = ret && SaveInt16vec(fp, "HSUC_HO_FORM1", &prms->HSUC_HO[1]);
	ret = ret && SaveInt16vec(fp, "HFLUCV_HREF_FORM1", &prms->HFLUCV_HREF[1]);
	
	if (fclose(fp) != 0) {
		DBGPRINT(DBG_LEVEL1, "%s Setting file close error.\n", __FUNCTION__);
		ret = 0;
	}
	
	return ret;
}

/*!
 Save parameters of int16 type structure to file. This function adds 
 @return If function fails, the return value is 0. When function fails,
 parameter is not saved to file. If function succeeds, the return value is 1.
 @param[in] fp Pointer to \c FILE structure.
 @param[in] lpKeyName The name of paraemter.
 @param[in] val Pointer to \c int16 type value.
 */
int16 SaveInt16(FILE * fp, const char *lpKeyName, const int16 val)
{
	if (fprintf(fp, "%s" DELIMITER "%d\n", lpKeyName, val) < 0) {
		DBGPRINT(DBG_LEVEL1, "%s write error.\n", lpKeyName);
		return 0;
	} else {
		return 1;
	}
}

/*!
 Save parameters of int16vec type structure to file. This function adds 
 ".x", ".y" and ".z" to the last of parameter name and save value with 
 the combined name.
 @return If function fails, the return value is 0. When function fails, not
 all parameters are saved to file, i.e. parameters file may collapsed.
 If function succeeds, the return value is 1.
 @param[in] fp Pointer to \c FILE structure.
 @param[in] lpKeyName The name of paraemter.
 @param[in] vec Pointer to \c int16vec type structure.
 */
int16 SaveInt16vec(FILE * fp, const char *lpKeyName,
				   const int16vec * vec)
{
	int16 ret = 0;
	char keyName[64];
	
	ret = 1;
	snprintf(keyName, sizeof(keyName), "%s.x", lpKeyName);
	ret = ret && SaveInt16(fp, keyName, vec->u.x);
	
	snprintf(keyName, sizeof(keyName), "%s.y", lpKeyName);
	ret = ret && SaveInt16(fp, keyName, vec->u.y);
	
	snprintf(keyName, sizeof(keyName), "%s.z", lpKeyName);
	ret = ret && SaveInt16(fp, keyName, vec->u.z);
	
	return ret;
}
