/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#define LOG_TAG "LSClient"

#include <cutils/properties.h>
#include <dirent.h>
#include <errno.h>
#include <log/log.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdlib.h>
#include <iomanip>
#include <sstream>
#include <string>

#include "LsClient.h"
#include "LsLib.h"

uint8_t datahex(char c);
unsigned char* getHASH(uint8_t* buffer, size_t buffSize);
extern bool ese_debug_enabled;

#define ls_script_source_prefix "/vendor/etc/loaderservice_updater_"
#define ls_script_source_suffix ".lss"
#define ls_script_output_prefix \
  "/data/vendor/secure_element/loaderservice_updater_out_"
#define ls_script_output_suffix ".txt"
const size_t HASH_DATA_LENGTH = 21;
const uint16_t HASH_STATUS_INDEX = 20;
const uint8_t LS_MAX_COUNT = 10;
const uint8_t LS_DOWNLOAD_SUCCESS = 0x00;
const uint8_t LS_DOWNLOAD_FAILED = 0x01;

class LSInfo {
 public:
  uint8_t m_status;
  uint8_t m_version;
  uint8_t m_mode;
  uint8_t m_slot1_status;
  uint8_t m_slot1_hash;
  uint8_t m_slot2_status;
  uint8_t m_slot2_hash;
};

static LSC_onCompletedCallback mCallback = nullptr;
static void* mCallbackParams = NULL;

void* performLSDownload_thread(void* data);
static void getLSScriptSourcePrefix(std::string& prefix);
static int compareLSHash(uint8_t* hash, uint8_t length);
static std::string printLSStatus(int status);
static std::string dumpLsInfo(LSInfo* info);

static int compareLSHash(uint8_t* hash, uint8_t length) {
  uint8_t ls253UpdaterScriptHash[HASH_DATA_LENGTH - 1] = {
      0x65, 0x80, 0xFB, 0xA0, 0xCA, 0x59, 0xAE, 0x6C, 0x71, 0x6B,
      0x15, 0xB1, 0xBD, 0xB1, 0x2C, 0x04, 0x29, 0x14, 0x8A, 0x8F};
  uint8_t ls253AppletScriptHash[HASH_DATA_LENGTH - 1] = {
      0x71, 0x7B, 0x8D, 0x0C, 0xEA, 0xE7, 0xEC, 0xC1, 0xCF, 0x47,
      0x33, 0x10, 0xFE, 0x8E, 0x52, 0x5D, 0xB1, 0x43, 0x9B, 0xDE};
  uint8_t lsFactoryScript1Hash[HASH_DATA_LENGTH - 1] = {
      0x4A, 0xD0, 0x37, 0xD0, 0x44, 0x5B, 0x78, 0x55, 0x17, 0x5E,
      0xFD, 0x87, 0x9C, 0xF1, 0x74, 0xBA, 0x77, 0xAD, 0x03, 0x62};
  uint8_t lsFactoryScript2Hash[HASH_DATA_LENGTH - 1] = {
      0xA9, 0xDB, 0x03, 0x53, 0xC2, 0xD7, 0xF8, 0xFC, 0x84, 0x37,
      0xAF, 0xB9, 0x53, 0x06, 0x27, 0x9D, 0xE9, 0x68, 0x45, 0xEF};
  uint8_t lsFactoryScript3Hash[HASH_DATA_LENGTH - 1] = {
      0xA9, 0xAE, 0x5E, 0x66, 0x92, 0x8F, 0x70, 0xBD, 0x0A, 0xC7,
      0x20, 0x8A, 0x6A, 0xBB, 0x63, 0xB3, 0xCA, 0x05, 0x58, 0xC1};
  uint8_t lsFactoryScript4Hash[HASH_DATA_LENGTH - 1] = {
      0x64, 0x73, 0x56, 0xAE, 0x58, 0x27, 0x6C, 0x07, 0x4B, 0xBA,
      0x64, 0x7E, 0x6E, 0xC1, 0x97, 0xC8, 0x57, 0x17, 0x6E, 0x2D};
  uint8_t* hashList[6] = {lsFactoryScript1Hash,   lsFactoryScript2Hash,
                          lsFactoryScript3Hash,   lsFactoryScript4Hash,
                          ls253UpdaterScriptHash, ls253AppletScriptHash};

  if (length != HASH_DATA_LENGTH - 1) {
    return 0xFF;
  }
  for (int i = 0; i < 6; i++) {
    if (0 == memcmp(hash, hashList[i], length)) {
      return i + 1;
    }
  }
  return 0xFF;
}

static std::string dumpLsInfo(LSInfo* info) {
  std::stringstream buff;
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_status);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_version);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_mode);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_slot1_status);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_slot1_hash);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_slot2_status);
  buff << std::setw(2) << std::setfill('0') << std::hex
       << (int)(info->m_slot2_hash);
  return buff.str();
}

void getLSScriptSourcePrefix(std::string& prefix) {
  char source_path[PROPERTY_VALUE_MAX] = {0};
  int len = property_get("vendor.ese.loader_script_path", source_path, "");
  if (len > 0) {
    FILE* fd = fopen(source_path, "rb");
    if (fd != NULL) {
      char c;
      while (!feof(fd) && fread(&c, 1, 1, fd) == 1) {
        if (c == ' ' || c == '\n' || c == '\r' || c == 0x00) break;
        prefix.push_back(c);
      }
      fclose(fd);
    } else {
      ALOGD("%s Cannot open file %s\n", __func__, source_path);
    }
  }
  if (prefix.empty()) {
    prefix.assign(ls_script_source_prefix);
  }
}

/*******************************************************************************
**
** Function:        LSC_Start
**
** Description:     Starts the LSC update with encrypted data privided in the
                    updater file
**
** Returns:         SUCCESS if ok.
**
*******************************************************************************/
LSCSTATUS LSC_Start(const char* name, const char* dest, uint8_t* pdata,
                    uint16_t len, uint8_t* respSW) {
  static const char fn[] = "LSC_Start";
  LSCSTATUS status = LSCSTATUS_FAILED;
  if (name != NULL) {
    status = Perform_LSC(name, dest, pdata, len, respSW);
  } else {
    ALOGE("%s: LS script file is missing", fn);
  }
  ALOGD_IF(ese_debug_enabled, "%s: Exit; status=0x0%X", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_doDownload
**
** Description:     Start LS download process
**
** Returns:         SUCCESS if ok
**
*******************************************************************************/
LSCSTATUS LSC_doDownload(LSC_onCompletedCallback callback, void* args) {
  static const char fn[] = "LSC_doDownload";

  mCallback = callback;
  mCallbackParams = args;

  LSCSTATUS status;
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&thread, &attr, &performLSDownload_thread, NULL) < 0) {
    ALOGE("%s: Thread creation failed", fn);
    status = LSCSTATUS_FAILED;
  } else {
    status = LSCSTATUS_SUCCESS;
  }
  pthread_attr_destroy(&attr);
  return status;
}

/*******************************************************************************
**
** Function:        printLSStatus
**
** Description:     print LS applet state and Slot 1 & 2 data
**
** Returns:         LS status log
**
*******************************************************************************/
std::string printLSStatus(int lsStatus) {
  ALOGD_IF(ese_debug_enabled, "%s enter  ", __func__);

  uint8_t slotHashBuffer[HASH_DATA_LENGTH] = {0};
  uint16_t length = 0;
  uint16_t lsVersion = 0;
  uint8_t lsMode = 0;
  std::stringstream outStream;
  std::stringstream outHash;

  LSInfo lsInfo;
  memset(&lsInfo, 0xFF, sizeof(LSInfo));
  lsInfo.m_status = lsStatus;

  outStream << "\nCurrent LS info:";
  /*Read LS applet mode*/
  LSCSTATUS status = LSC_ReadLscInfo(&lsMode, &lsVersion);
  if (status != LSCSTATUS_SUCCESS) {
    outStream << dumpLsInfo(&lsInfo);
    outStream << "\nFailed to access LS applet!\n";
    return outStream.str();
  }

  ALOGI_IF(ese_debug_enabled, "LS applet version is %d.%d", (lsVersion >> 8),
           (lsVersion & 0xFF));
  if (lsMode == 2) {
    ALOGI_IF(ese_debug_enabled, "LS is in UPDATE mode!");
  }
  lsInfo.m_version = lsVersion & 0xFF;
  lsInfo.m_mode = lsMode;

  /*Read the hash from slot 1*/
  status = LSC_ReadLsHash(slotHashBuffer, &length, 1);
  if (status != LSCSTATUS_SUCCESS) {
    ALOGI_IF(ese_debug_enabled, "Failed to read Hash value from slot 1.");
    outStream << dumpLsInfo(&lsInfo);
    return outStream.str();
  }
  if (slotHashBuffer[HASH_DATA_LENGTH - 1] == LS_DOWNLOAD_SUCCESS) {
    ALOGI_IF(ese_debug_enabled, "LS Slot 1 passed.");
    lsInfo.m_slot1_status = LS_DOWNLOAD_SUCCESS;
  } else {
    ALOGI_IF(ese_debug_enabled, "LS Slot 1 failed.");
    lsInfo.m_slot1_status = LS_DOWNLOAD_FAILED;
  }
  lsInfo.m_slot1_hash = compareLSHash(slotHashBuffer, HASH_DATA_LENGTH - 1);
  if (lsInfo.m_slot1_hash == 0xFF) {
    outHash << "\n slot 1 hash:\n";
    for (int i = 0; i < HASH_DATA_LENGTH - 1; i++) {
      outHash << std::setw(2) << std::setfill('0') << std::hex
              << (int)slotHashBuffer[i];
    }
  }

  /*Read the hash from slot 2*/
  status = LSC_ReadLsHash(slotHashBuffer, &length, 2);
  if (status != LSCSTATUS_SUCCESS) {
    ALOGI_IF(ese_debug_enabled, "Failed to read Hash value from slot 1.");
    outStream << dumpLsInfo(&lsInfo);
    return outStream.str();
  }
  if (slotHashBuffer[HASH_DATA_LENGTH - 1] == LS_DOWNLOAD_SUCCESS) {
    ALOGI_IF(ese_debug_enabled, "LS Slot 2 passed.");
    lsInfo.m_slot2_status = LS_DOWNLOAD_SUCCESS;
  } else {
    ALOGI_IF(ese_debug_enabled, "LS Slot 2 failed.");
    lsInfo.m_slot2_status = LS_DOWNLOAD_FAILED;
  }
  lsInfo.m_slot2_hash = compareLSHash(slotHashBuffer, HASH_DATA_LENGTH - 1);
  if (lsInfo.m_slot2_hash == 0xFF) {
    outHash << "\n slot 2 hash:\n";
    for (int i = 0; i < HASH_DATA_LENGTH - 1; i++) {
      outHash << std::setw(2) << std::setfill('0') << std::hex
              << (int)slotHashBuffer[i];
    }
  }

  outStream << dumpLsInfo(&lsInfo) << outHash.str();

  ALOGD_IF(ese_debug_enabled, "%s exit\n", __func__);
  return outStream.str();
}

/*******************************************************************************
**
** Function:        performLSDownload_thread
**
** Description:     Perform LS during hal init
**
** Returns:         None
**
*******************************************************************************/
void* performLSDownload_thread(__attribute__((unused)) void* data) {
  ALOGD_IF(ese_debug_enabled, "%s enter  ", __func__);
  /*generated SHA-1 string for secureElementLS
  This will remain constant as handled in secureElement HAL*/
  char sha1[] = "6d583e84f2710e6b0f06beebc1a12a1083591373";
  uint8_t hash[20] = {0};

  for (int i = 0; i < 40; i = i + 2) {
    hash[i / 2] =
        (((datahex(sha1[i]) & 0x0F) << 4) | (datahex(sha1[i + 1]) & 0x0F));
  }

  uint8_t resSW[4] = {0x4e, 0x02, 0x69, 0x87};

  std::string sourcePath;
  std::string sourcePrefix;
  std::string outPath;
  int index = 1;
  LSCSTATUS status = LSCSTATUS_SUCCESS;
  Lsc_HashInfo_t lsHashInfo;

  getLSScriptSourcePrefix(sourcePrefix);
  do {
    /*Open the script file from specified location and name*/
    sourcePath.assign(sourcePrefix);
    sourcePath += ('0' + index);
    sourcePath += ls_script_source_suffix;
    FILE* fIn = fopen(sourcePath.c_str(), "rb");
    if (fIn == NULL) {
      ALOGE("%s Cannot open LS script file %s, Error: %s\n", __func__,
            sourcePath.c_str(), strerror(errno));
      break;
    }
    ALOGD_IF(ese_debug_enabled, "%s File opened %s\n", __func__,
             sourcePath.c_str());

    /*Read the script content to a local buffer*/
    fseek(fIn, 0, SEEK_END);
    long lsBufSize = ftell(fIn);
    if (lsBufSize < 0) {
      ALOGE("%s Failed to get current value of position indicator\n", __func__);
      fclose(fIn);
      status = LSCSTATUS_FAILED;
      break;
    }
    rewind(fIn);
    if (lsHashInfo.lsRawScriptBuf == nullptr) {
      lsHashInfo.lsRawScriptBuf = (uint8_t*)phNxpEse_memalloc(lsBufSize + 1);
    }
    memset(lsHashInfo.lsRawScriptBuf, 0x00, (lsBufSize + 1));
    if (fread(lsHashInfo.lsRawScriptBuf, (size_t)lsBufSize, 1, fIn) != 1)
      ALOGD_IF(ese_debug_enabled, "%s Failed to read file", __func__);
    fclose(fIn);
    LSCSTATUS lsHashStatus = LSCSTATUS_FAILED;

    /*Get 20bye SHA1 of the script*/
    lsHashInfo.lsScriptHash =
        getHASH(lsHashInfo.lsRawScriptBuf, (size_t)lsBufSize);
    phNxpEse_free(lsHashInfo.lsRawScriptBuf);
    lsHashInfo.lsRawScriptBuf = nullptr;
    if (lsHashInfo.lsScriptHash == nullptr) break;

    if (lsHashInfo.readBuffHash == nullptr) {
      lsHashInfo.readBuffHash = (uint8_t*)phNxpEse_memalloc(HASH_DATA_LENGTH);
    }
    memset(lsHashInfo.readBuffHash, 0x00, HASH_DATA_LENGTH);

    /*Read the hash from applet for specified slot*/
    lsHashStatus =
        LSC_ReadLsHash(lsHashInfo.readBuffHash, &lsHashInfo.readHashLen, index);

    /*Check if previously script is successfully installed.
    if yes, continue reading next script else try update wit current script*/
    if ((lsHashStatus == LSCSTATUS_SUCCESS) &&
        (lsHashInfo.readHashLen == HASH_DATA_LENGTH) &&
        (0 == memcmp(lsHashInfo.lsScriptHash, lsHashInfo.readBuffHash,
                     HASH_DATA_LENGTH - 1)) &&
        (lsHashInfo.readBuffHash[HASH_STATUS_INDEX] == LS_DOWNLOAD_SUCCESS)) {
      ALOGD_IF(ese_debug_enabled, "%s LS Loader sript is already installed \n",
               __func__);
      continue;
    }

    /*Create output file to write response data*/
    outPath.assign(ls_script_output_prefix);
    outPath += ('0' + index);
    outPath += ls_script_output_suffix;

    FILE* fOut = fopen(outPath.c_str(), "wb+");
    if (fOut == NULL) {
      ALOGE("%s Failed to open file %s\n", __func__, outPath.c_str());
      break;
    }
    fclose(fOut);

    /*Uptdates current script*/
    status = LSC_Start(sourcePath.c_str(), outPath.c_str(), (uint8_t*)hash,
                       (uint16_t)sizeof(hash), resSW);
    ALOGD_IF(ese_debug_enabled, "%s script %s perform done, result = %d\n",
             __func__, sourcePath.c_str(), status);
    if (status != LSCSTATUS_SUCCESS) {
      lsHashInfo.lsScriptHash[HASH_STATUS_INDEX] = LS_DOWNLOAD_FAILED;
      /*If current script updation fails, update the status with hash to the
       * applet then clean and exit*/
      lsHashStatus =
          LSC_UpdateLsHash(lsHashInfo.lsScriptHash, HASH_DATA_LENGTH, index);
      if (lsHashStatus != LSCSTATUS_SUCCESS) {
        ALOGD_IF(ese_debug_enabled, "%s LSC_UpdateLsHash Failed\n", __func__);
      }
      ESESTATUS estatus = phNxpEse_deInit();
      if (estatus == ESESTATUS_SUCCESS) {
        estatus = phNxpEse_close();
        if (estatus == ESESTATUS_SUCCESS) {
          ALOGD_IF(ese_debug_enabled, "%s: Ese_close success\n", __func__);
        }
      } else {
        ALOGE("%s: Ese_deInit failed", __func__);
      }

      if (mCallback != nullptr) {
        (mCallback)(false, printLSStatus(LS_DOWNLOAD_FAILED), mCallbackParams);
        break;
      }
    } else {
      /*If current script execution is succes, update the status along with the
       * hash to the applet*/
      lsHashInfo.lsScriptHash[HASH_STATUS_INDEX] = LS_DOWNLOAD_SUCCESS;
      lsHashStatus =
          LSC_UpdateLsHash(lsHashInfo.lsScriptHash, HASH_DATA_LENGTH, index);
      if (lsHashStatus != LSCSTATUS_SUCCESS) {
        ALOGD_IF(ese_debug_enabled, "%s LSC_UpdateLsHash Failed\n", __func__);
      }
    }
  } while (++index <= LS_MAX_COUNT);

  phNxpEse_free(lsHashInfo.readBuffHash);

  if (status == LSCSTATUS_SUCCESS) {
    if (mCallback != nullptr) {
      (mCallback)(true, printLSStatus(LS_DOWNLOAD_SUCCESS), mCallbackParams);
    }
  }
  pthread_exit(NULL);
  ALOGD_IF(ese_debug_enabled, "%s pthread_exit\n", __func__);
  return NULL;
}

/*******************************************************************************
**
** Function:        getHASH
**
** Description:     generates SHA1 of given buffer
**
** Returns:         20 bytes of SHA1
**
*******************************************************************************/
unsigned char* getHASH(uint8_t* buffer, size_t buffSize) {
  static uint8_t outHash[HASH_DATA_LENGTH] = {0};
  unsigned int md_len = -1;
  const EVP_MD* md = EVP_get_digestbyname("SHA1");
  if (NULL != md) {
    EVP_MD_CTX mdctx;
    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    EVP_DigestUpdate(&mdctx, buffer, buffSize);
    EVP_DigestFinal_ex(&mdctx, outHash, &md_len);
    EVP_MD_CTX_cleanup(&mdctx);
  }
  return outHash;
}

/*******************************************************************************
**
** Function:        datahex
**
** Description:     Converts char to uint8_t
**
** Returns:         uint8_t variable
**
*******************************************************************************/
uint8_t datahex(char c) {
  uint8_t value = 0;
  if (c >= '0' && c <= '9')
    value = (c - '0');
  else if (c >= 'A' && c <= 'F')
    value = (10 + (c - 'A'));
  else if (c >= 'a' && c <= 'f')
    value = (10 + (c - 'a'));
  return value;
}
